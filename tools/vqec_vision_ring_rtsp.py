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
  vqec_vision_ring_rtsp.py read --ring-id encoded_ai_detect0_cam0_ch0 \
      --port 8554 --mount /detect0 --fps 30
"""

import argparse
import mmap
import os
import struct
import sys
import time

import gi

gi.require_version("Gst", "1.0")
gi.require_version("GstRtspServer", "1.0")
from gi.repository import GLib, Gst, GstRtspServer  # noqa: E402

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
H264_SPS_NAL_TYPE = 7
H264_PPS_NAL_TYPE = 8
RTP_H264_PAYLOAD_TYPE = 96
NANOSECONDS_PER_SECOND = 1_000_000_000


def has_h264_parameter_sets(payload):
    nal_types = set()
    for index in range(max(0, len(payload) - 3)):
        if (index + 4 < len(payload) and
                payload[index:index + 4] == b"\x00\x00\x00\x01"):
            nal_types.add(payload[index + 4] & 0x1F)
        elif (index + 3 < len(payload) and
              payload[index:index + 3] == b"\x00\x00\x01"):
            nal_types.add(payload[index + 3] & 0x1F)
    return H264_SPS_NAL_TYPE in nal_types and H264_PPS_NAL_TYPE in nal_types


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
        if sequence_value != sequence:
            return None
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
    def __init__(self, port, mount, fps):
        self.appsrc = None
        self.generation = 0
        self.duration_ns = NANOSECONDS_PER_SECOND // fps
        self.server = GstRtspServer.RTSPServer.new()
        self.server.set_address("0.0.0.0")
        self.server.set_service(str(port))
        self.factory = GstRtspServer.RTSPMediaFactory.new()
        self.factory.set_shared(True)
        self.factory.set_launch(
            "( appsrc name=src is-live=true format=time "
            "caps=video/x-h264,stream-format=byte-stream,alignment=au "
            "! h264parse config-interval=-1 "
            f"! rtph264pay name=pay0 pt={RTP_H264_PAYLOAD_TYPE} )")
        self.factory.connect("media-configure", self._media_configure)
        self.server.get_mount_points().add_factory(mount, self.factory)
        if self.server.attach(None) == 0:
            raise RuntimeError("failed to attach the RTSP server")

    def _media_configure(self, _factory, media):
        element = media.get_element()
        self.appsrc = element.get_by_name("src")
        self.generation += 1
        media.connect("unprepared", self._media_unprepared)

    def _media_unprepared(self, _media):
        self.appsrc = None
        self.generation += 1

    def ready(self):
        return self.appsrc is not None

    def push(self, payload, pts_ns):
        if self.appsrc is None:
            return
        buf = Gst.Buffer.new_allocate(None, len(payload), None)
        buf.fill(0, payload)
        buf.pts = pts_ns
        buf.duration = self.duration_ns
        self.appsrc.emit("push-buffer", buf)

    def stop(self):
        if self.appsrc is not None:
            self.appsrc.emit("end-of-stream")



def run_read(args):
    if args.fps <= 0:
        print("--fps must be greater than zero", file=sys.stderr)
        return 2
    Gst.init(None)
    reader = RingReader(shm_path(args.ring_id))
    if not reader.open():
        print(f"ring did not appear: {shm_path(args.ring_id)}", file=sys.stderr)
        return 1
    print(f"ring open: header={reader.header_size()} slot_hdr={reader.slot_header_size()} "
          f"slots={reader.slot_count()} payload={reader.payload_size()}", flush=True)
    publisher = RtspPublisher(args.port, args.mount, args.fps)
    state = {
        "next": 0,
        "started": False,
        "pushed": 0,
        "client_frame": 0,
        "generation": -1,
    }

    def poll():
        if not publisher.ready():
            return True
        if state["generation"] != publisher.generation:
            current = reader.write_sequence()
            state["next"] = max(0, current - reader.slot_count())
            state["started"] = False
            state["client_frame"] = 0
            state["generation"] = publisher.generation
        current = reader.write_sequence()
        while state["next"] < current:
            slot = reader.read_slot(state["next"])
            state["next"] += 1
            if slot is None:
                continue
            if slot["codec"] != b"H264":
                continue
            if not state["started"]:
                if not slot["keyframe"] or not has_h264_parameter_sets(slot["payload"]):
                    continue
                state["started"] = True
            # Ring sequence is process-global and may already be large when a client
            # connects. Rebase timestamps per RTSP media generation so a late joiner does
            # not wait for the producer's historical running time before its first frame.
            publisher.push(
                slot["payload"], state["client_frame"] * publisher.duration_ns)
            state["client_frame"] += 1
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


def main():
    parser = argparse.ArgumentParser(description="Mock FW encoded-ring RTSP service")
    sub = parser.add_subparsers(dest="command", required=True)

    read = sub.add_parser("read")
    read.add_argument("--ring-id", required=True)
    read.add_argument("--port", required=True)
    read.add_argument("--mount", required=True)
    read.add_argument("--fps", type=int, required=True)
    read.set_defaults(func=run_read)



    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
