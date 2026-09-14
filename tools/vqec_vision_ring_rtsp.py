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


def main():
    parser = argparse.ArgumentParser(description="Mock FW encoded-ring RTSP service")
    sub = parser.add_subparsers(dest="command", required=True)

    read = sub.add_parser("read")
    read.add_argument("--ring-id", default="encoded_ai_detect0_cam0_ch0")
    read.add_argument("--port", default="8554")
    read.add_argument("--mount", default="/live/ai/detect0")
    read.set_defaults(func=run_read)



    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
