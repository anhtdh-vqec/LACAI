#!/usr/bin/env python3
"""Mock FW RTSP output service for LACAI integration tests.

Reads the released FW shared-memory encoded ring that LACAI produces
(`/dev/shm/camera_ai_<ring_id>`) and publishes it as RTSP. This replaces the FW
RTSP service without depending on the FW application sources.

Ring layout (released `camera_ai::SharedMemoryFrameRingBuffer`, version 5):
  header:  magic, version, header_size, slot_header_size, slot_count,
           payload_size, write_index, reserved0, write_sequence(u64), ...
  slot i:  one SharedSlotHeader (seqlock + metadata + SPS/PPS) followed by
           `payload_size` bytes of H.264 access-unit data.

The reader never touches the producer's mutex/consumer registry: it polls
`write_sequence` and copies slots under the per-slot seqlock, so it is a
lock-free observer.

Subcommands:
  read      attach to the ring and serve RTSP
  simulate  self-test: encode videotestsrc into the ring (stand-in producer)

Examples:
  vqec_vision_ring_rtsp.py read --ring-id encoded_ai_detect0_cam0_ch0 --port 8554
  vqec_vision_ring_rtsp.py simulate --ring-id encoded_ai_detect0_cam0_ch0
"""

import argparse
import array
import mmap
import os
import socket
import struct
import sys
import time

import gi

gi.require_version("Gst", "1.0")
from gi.repository import GLib, Gst  # noqa: E402

RING_VERSION = 5
SLOT_COUNT = 16
PAYLOAD_SIZE = 1 << 20
SLOT_HEADER_SIZE = 1232
HEADER_SIZE = 4096

# SharedRingHeader scalar field offsets.
H_MAGIC = 0
H_VERSION = 4
H_HEADER_SIZE = 8
H_SLOT_HEADER_SIZE = 12
H_SLOT_COUNT = 16
H_PAYLOAD_SIZE = 20
H_WRITE_SEQUENCE = 32
H_RING_ID = 64

# SharedSlotHeader field offsets.
S_SEQLOCK = 0
S_DATA_SIZE = 8
S_WIDTH = 20
S_HEIGHT = 24
S_STRIDE = 28
S_IS_KEYFRAME = 40
S_H264_SPS_SIZE = 44
S_H264_PPS_SIZE = 48
S_FRAME_ID = 56
S_TIMESTAMP_NS = 64
S_MEDIA_PTS_NS = 72
S_SEQUENCE = 104
S_CODEC = 176
S_H264_SPS = 208
S_H264_PPS = 720
SPS_PPS_MAX = 512


def shm_path(ring_id):
    sanitized = "".join(c if c.isalnum() or c in "._-" else "_" for c in ring_id)
    return f"/dev/shm/camera_ai_{sanitized}"


class RingReader:
    def __init__(self, path):
        self.path = path
        self.fd = -1
        self.mapping = None

    def open(self, timeout_s=10.0):
        deadline = time.time() + timeout_s
        while time.time() < deadline:
            if os.path.exists(self.path):
                break
            time.sleep(0.1)
        self.fd = os.open(self.path, os.O_RDWR)
        size = os.fstat(self.fd).st_size
        self.mapping = mmap.mmap(self.fd, size, mmap.MAP_SHARED,
                                 mmap.PROT_READ | mmap.PROT_WRITE)
        return self.header_size() > 0

    def close(self):
        if self.mapping is not None:
            self.mapping.close()
            self.mapping = None
        if self.fd >= 0:
            os.close(self.fd)
            self.fd = -1

    def u32(self, offset):
        return struct.unpack_from("<I", self.mapping, offset)[0]

    def u64(self, offset):
        return struct.unpack_from("<Q", self.mapping, offset)[0]

    def header_size(self):
        return self.u32(H_HEADER_SIZE)

    def slot_header_size(self):
        return self.u32(H_SLOT_HEADER_SIZE)

    def slot_count(self):
        return self.u32(H_SLOT_COUNT)

    def payload_size(self):
        return self.u32(H_PAYLOAD_SIZE)

    def write_sequence(self):
        return self.u64(H_WRITE_SEQUENCE)

    def slot_base(self, index):
        return self.header_size() + index * (self.slot_header_size() + self.payload_size())

    def read_slot(self, sequence):
        index = sequence % self.slot_count()
        base = self.slot_base(index)
        if base + self.slot_header_size() + self.payload_size() > len(self.mapping):
            return None
        first = self.u32(base + S_SEQLOCK)
        if first & 1:
            return None
        data_size = self.u32(base + S_DATA_SIZE)
        if data_size == 0 or data_size > self.payload_size():
            return None
        sequence_value = self.u64(base + S_SEQUENCE)
        keyframe = self.u32(base + S_IS_KEYFRAME) != 0
        codec = self.mapping[base + S_CODEC:base + S_CODEC + 4].split(b"\0", 1)[0]
        payload = bytes(self.mapping[base + self.slot_header_size():
                                     base + self.slot_header_size() + data_size])
        second = self.u32(base + S_SEQLOCK)
        if first != second:
            return None
        return {"sequence": sequence_value, "keyframe": keyframe, "codec": codec,
                "payload": payload}


class RtspPublisher:
    def __init__(self, port, mount):
        self.pipeline = Gst.parse_launch(
            f"appsrc name=src is-live=true format=time "
            f"! h264parse config-interval=1 "
            f"! qtirtspbin address=0.0.0.0 port={port} mpoint={mount}")
        self.appsrc = self.pipeline.get_by_name("src")
        caps = Gst.Caps.from_string(
            "video/x-h264,stream-format=byte-stream,alignment=au")
        self.appsrc.set_property("caps", caps)
        self.pipeline.set_state(Gst.State.PLAYING)

    def push(self, payload, pts_ns):
        buf = Gst.Buffer.new_allocate(None, len(payload), None)
        buf.fill(0, payload)
        buf.pts = pts_ns
        buf.duration = 33333333
        self.appsrc.emit("push-buffer", buf)

    def stop(self):
        self.appsrc.emit("end-of-stream")
        self.pipeline.set_state(Gst.State.NULL)



FRAME_WIRE = struct.Struct("<QIIII4I4iQQQQQQ")
RETURN_WIRE = struct.Struct("<Q")


class RingWriter:
    """Creates a ring with the released FW layout and writes H.264 AUs."""

    def __init__(self, ring_id):
        self.path = shm_path(ring_id)
        try:
            os.unlink(self.path)
        except FileNotFoundError:
            pass
        self.fd = os.open(self.path, os.O_RDWR | os.O_CREAT, 0o600)
        total = HEADER_SIZE + SLOT_COUNT * (SLOT_HEADER_SIZE + PAYLOAD_SIZE)
        os.ftruncate(self.fd, total)
        self.mapping = mmap.mmap(self.fd, total, mmap.MAP_SHARED,
                                 mmap.PROT_READ | mmap.PROT_WRITE)
        struct.pack_into("<I", self.mapping, H_MAGIC, 0x4C414341)
        struct.pack_into("<I", self.mapping, H_VERSION, RING_VERSION)
        struct.pack_into("<I", self.mapping, H_HEADER_SIZE, HEADER_SIZE)
        struct.pack_into("<I", self.mapping, H_SLOT_HEADER_SIZE, SLOT_HEADER_SIZE)
        struct.pack_into("<I", self.mapping, H_SLOT_COUNT, SLOT_COUNT)
        struct.pack_into("<I", self.mapping, H_PAYLOAD_SIZE, PAYLOAD_SIZE)
        struct.pack_into("<Q", self.mapping, H_WRITE_SEQUENCE, 0)
        self.mapping[H_RING_ID:H_RING_ID + len(ring_id)] = ring_id.encode()
        self.sequence = 0

    def push(self, payload, width, height, keyframe):
        if not payload or len(payload) > PAYLOAD_SIZE:
            return
        index = self.sequence % SLOT_COUNT
        base = HEADER_SIZE + index * (SLOT_HEADER_SIZE + PAYLOAD_SIZE)
        struct.pack_into("<I", self.mapping, base + S_SEQLOCK, 1)
        struct.pack_into("<I", self.mapping, base + S_DATA_SIZE, len(payload))
        struct.pack_into("<I", self.mapping, base + S_WIDTH, width)
        struct.pack_into("<I", self.mapping, base + S_HEIGHT, height)
        struct.pack_into("<I", self.mapping, base + S_STRIDE, width)
        struct.pack_into("<I", self.mapping, base + S_IS_KEYFRAME, 1 if keyframe else 0)
        struct.pack_into("<I", self.mapping, base + S_H264_SPS_SIZE, 0)
        struct.pack_into("<I", self.mapping, base + S_H264_PPS_SIZE, 0)
        struct.pack_into("<Q", self.mapping, base + S_FRAME_ID, self.sequence + 1)
        struct.pack_into("<Q", self.mapping, base + S_SEQUENCE, self.sequence)
        self.mapping[base + S_CODEC:base + S_CODEC + 4] = b"H264"
        self.mapping[base + SLOT_HEADER_SIZE:base + SLOT_HEADER_SIZE + len(payload)] = payload
        struct.pack_into("<I", self.mapping, base + S_SEQLOCK, 0)
        self.sequence += 1
        struct.pack_into("<Q", self.mapping, H_WRITE_SEQUENCE, self.sequence)

    def close(self):
        self.mapping.close()
        os.close(self.fd)


def run_bridge(args):
    """Read raw NV12 from the FW wire socket, encode H.264, write the FW ring.

    This is the stand-in for LACAI's encoded output path so the ring reader and
    RTSP service can be tested without the product encoder wiring.
    """
    Gst.init(None)
    writer = RingWriter(args.ring_id)
    pipeline = Gst.parse_launch(
        f"appsrc name=src is-live=true format=time "
        f"! video/x-raw,format=NV12,width={args.width},height={args.height},"
        f"framerate={args.fps}/1 ! queue ! videoconvert ! video/x-raw,format=NV12 "
        "! v4l2h264enc ! h264parse config-interval=1 "
        "! appsink name=enc max-buffers=2 drop=true sync=false")
    appsrc = pipeline.get_by_name("src")
    appsink = pipeline.get_by_name("enc")
    appsrc.set_property("caps", Gst.Caps.from_string(
        f"video/x-raw,format=NV12,width={args.width},height={args.height},"
        f"framerate={args.fps}/1"))
    pipeline.set_state(Gst.State.PLAYING)

    client = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    client.connect(args.socket)
    client.settimeout(2.0)
    frames = 0
    try:
        while True:
            data, anc, _flags, _addr = client.recvmsg(
                FRAME_WIRE.size, socket.CMSG_SPACE(16))
            fds = array.array("i")
            for level, kind, cdata in anc:
                if level == socket.SOL_SOCKET and kind == socket.SCM_RIGHTS:
                    fds.frombytes(cdata[:len(cdata) - (len(cdata) % 4)])
            header = FRAME_WIRE.unpack(data)
            buf_id = header[0]
            frame_size = args.width * args.height * 3 // 2
            payload = b""
            if fds:
                os.lseek(fds[0], 0, 0)
                payload = os.read(fds[0], frame_size)
                os.close(fds[0])
            client.send(RETURN_WIRE.pack(buf_id))
            if len(payload) != frame_size:
                continue
            buf = Gst.Buffer.new_allocate(None, len(payload), None)
            buf.fill(0, payload)
            buf.pts = frames * 33333333
            buf.duration = 33333333
            appsrc.emit("push-buffer", buf)
            sample = appsink.emit("try-pull-sample", Gst.SECOND)
            if sample is None:
                continue
            enc = sample.get_buffer()
            ok, info = enc.map(Gst.MapFlags.READ)
            if not ok:
                continue
            au = bytes(info.data)
            enc.unmap(info)
            keyframe = not (enc.get_flags() & Gst.BufferFlags.DELTA_UNIT)
            writer.push(au, args.width, args.height, keyframe)
            frames += 1
            if frames % 60 == 0:
                print(f"bridged={frames}", flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        pipeline.set_state(Gst.State.NULL)
        writer.close()
        client.close()
    return 0


def run_read(args):
    Gst.init(None)
    reader = RingReader(shm_path(args.ring_id))
    if not reader.open():
        print(f"ring did not appear: {shm_path(args.ring_id)}", file=sys.stderr)
        return 1
    print(f"ring open: header={reader.header_size()} slot_hdr={reader.slot_header_size()} "
          f"slots={reader.slot_count()} payload={reader.payload_size()}", flush=True)
    publisher = RtspPublisher(args.port, args.mount)
    state = {"next": reader.write_sequence(), "started": False, "pushed": 0}

    def poll():
        current = reader.write_sequence()
        while state["next"] < current:
            slot = reader.read_slot(state["next"])
            state["next"] += 1
            if slot is None:
                continue
            if not state["started"]:
                if not slot["keyframe"]:
                    continue
                state["started"] = True
            publisher.push(slot["payload"], slot["sequence"] * 33333333)
            state["pushed"] += 1
            if state["pushed"] % 60 == 0:
                print(f"pushed={state['pushed']}", flush=True)
        return True

    GLib.timeout_add(5, poll)
    loop = GLib.MainLoop()
    try:
        loop.run()
    except KeyboardInterrupt:
        pass
    publisher.stop()
    reader.close()
    return 0


def run_simulate(args):
    """Stand-in producer: encode videotestsrc into the ring layout for testing."""
    Gst.init(None)
    path = shm_path(args.ring_id)
    try:
        os.unlink(path)
    except FileNotFoundError:
        pass
    fd = os.open(path, os.O_RDWR | os.O_CREAT, 0o600)
    total = HEADER_SIZE + SLOT_COUNT * (SLOT_HEADER_SIZE + PAYLOAD_SIZE)
    os.ftruncate(fd, total)
    mapping = mmap.mmap(fd, total, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
    struct.pack_into("<I", mapping, H_MAGIC, 0x4C414341)
    struct.pack_into("<I", mapping, H_VERSION, RING_VERSION)
    struct.pack_into("<I", mapping, H_HEADER_SIZE, HEADER_SIZE)
    struct.pack_into("<I", mapping, H_SLOT_HEADER_SIZE, SLOT_HEADER_SIZE)
    struct.pack_into("<I", mapping, H_SLOT_COUNT, SLOT_COUNT)
    struct.pack_into("<I", mapping, H_PAYLOAD_SIZE, PAYLOAD_SIZE)
    struct.pack_into("<Q", mapping, H_WRITE_SEQUENCE, 0)
    mapping[H_RING_ID:H_RING_ID + len(args.ring_id)] = args.ring_id.encode()

    pipeline = Gst.parse_launch(
        f"videotestsrc is-live=true ! video/x-raw,format=NV12,width={args.width},"
        f"height={args.height},framerate={args.fps}/1 ! v4l2h264enc ! "
        "h264parse config-interval=1 ! appsink name=enc max-buffers=2 drop=true sync=false")
    appsink = pipeline.get_by_name("enc")
    pipeline.set_state(Gst.State.PLAYING)
    sequence = 0
    pushed = 0
    try:
        while True:
            sample = appsink.emit("try-pull-sample", Gst.SECOND)
            if sample is None:
                continue
            buf = sample.get_buffer()
            ok, info = buf.map(Gst.MapFlags.READ)
            if not ok:
                continue
            payload = bytes(info.data)
            buf.unmap(info)
            if len(payload) == 0 or len(payload) > PAYLOAD_SIZE:
                continue
            index = sequence % SLOT_COUNT
            base = HEADER_SIZE + index * (SLOT_HEADER_SIZE + PAYLOAD_SIZE)
            struct.pack_into("<I", mapping, base + S_SEQLOCK, 1)
            struct.pack_into("<I", mapping, base + S_DATA_SIZE, len(payload))
            struct.pack_into("<I", mapping, base + S_WIDTH, args.width)
            struct.pack_into("<I", mapping, base + S_HEIGHT, args.height)
            struct.pack_into("<I", mapping, base + S_STRIDE, args.width)
            keyframe = 0 if (buf.get_flags() & Gst.BufferFlags.DELTA_UNIT) else 1
            struct.pack_into("<I", mapping, base + S_IS_KEYFRAME, keyframe)
            struct.pack_into("<I", mapping, base + S_H264_SPS_SIZE, 0)
            struct.pack_into("<I", mapping, base + S_H264_PPS_SIZE, 0)
            struct.pack_into("<Q", mapping, base + S_FRAME_ID, sequence + 1)
            struct.pack_into("<Q", mapping, base + S_SEQUENCE, sequence)
            mapping[base + S_CODEC:base + S_CODEC + 4] = b"H264"
            mapping[base + SLOT_HEADER_SIZE:base + SLOT_HEADER_SIZE + len(payload)] = payload
            struct.pack_into("<I", mapping, base + S_SEQLOCK, 0)
            sequence += 1
            struct.pack_into("<Q", mapping, H_WRITE_SEQUENCE, sequence)
            pushed += 1
            if pushed % 60 == 0:
                print(f"produced={pushed}", flush=True)
    except KeyboardInterrupt:
        pass
    pipeline.set_state(Gst.State.NULL)
    mapping.close()
    os.close(fd)
    return 0


def main():
    parser = argparse.ArgumentParser(description="Mock FW encoded-ring RTSP service")
    sub = parser.add_subparsers(dest="command", required=True)

    read = sub.add_parser("read")
    read.add_argument("--ring-id", default="encoded_ai_detect0_cam0_ch0")
    read.add_argument("--port", default="8554")
    read.add_argument("--mount", default="/live/ai/detect0")
    read.set_defaults(func=run_read)

    simulate = sub.add_parser("simulate")
    simulate.add_argument("--ring-id", default="encoded_ai_detect0_cam0_ch0")
    simulate.add_argument("--width", type=int, default=1280)
    simulate.add_argument("--height", type=int, default=720)
    simulate.add_argument("--fps", type=int, default=30)
    simulate.set_defaults(func=run_simulate)

    bridge_parser = sub.add_parser("bridge")
    bridge_parser.add_argument("--ring-id", default="encoded_ai_detect0_cam0_ch0")
    bridge_parser.add_argument("--socket", default="/run/camera_ai/0_third_ai.sock")
    bridge_parser.add_argument("--width", type=int, default=1280)
    bridge_parser.add_argument("--height", type=int, default=720)
    bridge_parser.add_argument("--fps", type=int, default=30)
    bridge_parser.set_defaults(func=run_bridge)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
