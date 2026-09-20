#!/usr/bin/env python3
"""Device-free regression for the FW camera simulator's ACK-gated buffer pool."""

import array
import importlib.util
import os
import socket
import sys
import threading
import time
import types
from pathlib import Path


def vqec_vision_ai_tools_fwstt_load_simulator():
    gi = types.ModuleType("gi")
    gi.require_version = lambda *_args: None
    repository = types.ModuleType("gi.repository")
    repository.Gio = types.SimpleNamespace()
    repository.GLib = types.SimpleNamespace()
    repository.Gst = types.SimpleNamespace(SECOND=1000000000)
    repository.GstVideo = types.SimpleNamespace()
    gi.repository = repository
    sys.modules["gi"] = gi
    sys.modules["gi.repository"] = repository
    source = Path(__file__).with_name("vqec_vision_fw_camera_sim.py")
    spec = importlib.util.spec_from_file_location("vqec_vision_fw_camera_sim", source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def vqec_vision_ai_tools_fwstt_run():
    sim = vqec_vision_ai_tools_fwstt_load_simulator()
    args = types.SimpleNamespace(width=4, height=4, fps=25, max_in_flight=1,
                                 dma_heap=None, socket_dir="/unused", channel=0,
                                 consumer="ai", max_frames=0)
    pool = sim.FramePool(args)
    slot = pool.vqec_vision_ai_tools_fwsim_acquire_slot()
    frame_fd = pool.vqec_vision_ai_tools_fwsim_write_frame(slot, bytes([7]) * pool.frame_size)
    try:
        if os.pread(frame_fd, pool.frame_size, 0) != bytes([7]) * pool.frame_size:
            raise AssertionError("pool copy differs from transmitted FD")
        try:
            pool.vqec_vision_ai_tools_fwsim_acquire_slot()
            raise AssertionError("in-flight slot was reused before ACK")
        except sim.queue.Empty:
            pass
    finally:
        os.close(frame_fd)
        pool.vqec_vision_ai_tools_fwsim_release_slot(slot)
    if pool.vqec_vision_ai_tools_fwsim_acquire_slot() != slot:
        raise AssertionError("ACKed slot was not reusable")
    pool.vqec_vision_ai_tools_fwsim_release_slot(slot)

    synthetic_args = types.SimpleNamespace(
        width=4, height=4, fps=25, max_in_flight=1, dma_heap=None,
        socket_dir="/unused", channel=0, consumer="ai", max_frames=0,
        source=sim.SOURCE_TEST_PATTERN)
    synthetic = sim.CameraPipeline(synthetic_args)
    synthetic.start()
    synthetic.next_test_frame_ns = time.monotonic_ns()
    synthetic_frame = synthetic.next_fd(sim.Gst.SECOND)
    if synthetic_frame is None:
        raise AssertionError("test-pattern source produced no frame")
    synthetic_fd, synthetic_bytes, synthetic_slot = synthetic_frame
    try:
        if synthetic_bytes != synthetic_args.width * synthetic_args.height * 3 // 2:
            raise AssertionError("test-pattern source produced an invalid byte count")
        if len(os.pread(synthetic_fd, synthetic_bytes, 0)) != synthetic_bytes:
            raise AssertionError("test-pattern source produced a truncated frame")
    finally:
        os.close(synthetic_fd)
        synthetic.pool.vqec_vision_ai_tools_fwsim_release_slot(synthetic_slot)
        synthetic.stop()

    class FixtureCamera:
        def __init__(self):
            self.pool = pool

        def next_fd(self, _timeout_ns):
            try:
                next_slot = self.pool.vqec_vision_ai_tools_fwsim_acquire_slot()
            except sim.queue.Empty:
                return None
            fd = self.pool.vqec_vision_ai_tools_fwsim_write_frame(
                next_slot, bytes([next_slot + 1]) * self.pool.frame_size)
            return fd, self.pool.frame_size, next_slot

        def vqec_vision_ai_tools_fwsim_reset_pool(self):
            self.pool.vqec_vision_ai_tools_fwsim_close_pool()

    producer = sim.RawFrameProducer(args, FixtureCamera())
    producer.running = True
    server, consumer = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    consumer.settimeout(0.2)
    worker = threading.Thread(target=producer._stream, args=(server,))
    worker.start()
    try:
        for expected_id in (1, 2):
            message, control, _, _ = consumer.recvmsg(sim.FRAME_HEADER.size,
                                                       socket.CMSG_SPACE(array.array("i").itemsize))
            if len(message) != sim.FRAME_HEADER.size or len(control) != 1:
                raise AssertionError("missing frame header or SCM_RIGHTS descriptor")
            frame_id = sim.FRAME_HEADER.unpack(message)[0]
            if frame_id != expected_id:
                raise AssertionError("unexpected frame ID")
            received_fds = array.array("i")
            received_fds.frombytes(control[0][2])
            os.close(received_fds[0])
            try:
                consumer.recv(1)
                raise AssertionError("frame arrived before the prior ACK")
            except socket.timeout:
                pass
            consumer.send(sim.RETURN_HEADER.pack(frame_id))
        consumer.send(sim.RETURN_HEADER.pack(1))
        worker.join(timeout=2)
        if worker.is_alive():
            raise AssertionError("duplicate ACK did not fault connection")
    finally:
        producer.running = False
        consumer.close()
        server.close()
        worker.join(timeout=2)
    print("PASS camera fixture pool, test pattern and exact ACK rejection")


if __name__ == "__main__":
    vqec_vision_ai_tools_fwstt_run()
