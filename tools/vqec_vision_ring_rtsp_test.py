#!/usr/bin/env python3
"""Regression for RTSP reader following a replaced v5 ring; no camera or biometrics."""

import importlib.util
import struct
import tempfile
from pathlib import Path


def vqec_vision_ai_tools_rrtst_write_ring(_path, _sequence):
    # Independent released v5 layout fixture; intentionally different from frame data.
    header_bytes = 4096
    slot_header_bytes = 1232
    slot_count = 2
    payload_bytes = 128
    image = bytearray(header_bytes + slot_count * (slot_header_bytes + payload_bytes))
    struct.pack_into("<7I", image, 0, 0, 5, header_bytes,
                     slot_header_bytes, slot_count, payload_bytes, 0)
    struct.pack_into("<Q", image, 32, _sequence)
    _path.write_bytes(image)


def vqec_vision_ai_tools_rrtst_run():
    source = Path(__file__).with_name("vqec_vision_ring_rtsp.py")
    spec = importlib.util.spec_from_file_location("ring_rtsp", source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    with tempfile.TemporaryDirectory(prefix="vqec_vision_ring_fixture_") as directory:
        path = Path(directory) / "ring"
        vqec_vision_ai_tools_rrtst_write_ring(path, 123)
        reader = module.RingReader(str(path))
        if not reader.open(timeout_s=0) or reader.write_sequence() != 123:
            raise AssertionError("initial ring fixture did not open")
        old_identity = reader.identity
        if reader.vqec_vision_ai_tools_rrtsp_refresh_mapping():
            raise AssertionError("unchanged inode was unnecessarily reopened")
        replacement = Path(directory) / "replacement"
        vqec_vision_ai_tools_rrtst_write_ring(replacement, 1)
        replacement.replace(path)
        if (not reader.vqec_vision_ai_tools_rrtsp_refresh_mapping() or
                reader.identity == old_identity or reader.write_sequence() != 1):
            raise AssertionError("reader retained the obsolete mapped ring")
        path.unlink()
        reader.vqec_vision_ai_tools_rrtsp_refresh_mapping()
        if reader.mapping is not None or reader.fd != -1:
            raise AssertionError("missing ring retained stale mapping/descriptor")
        path.write_bytes(b"partial header")
        reader.vqec_vision_ai_tools_rrtsp_refresh_mapping()
        if reader.mapping is not None:
            raise AssertionError("partially initialized ring was accepted")
        vqec_vision_ai_tools_rrtst_write_ring(path, 456)
        reader.vqec_vision_ai_tools_rrtsp_refresh_mapping()
        if reader.mapping is None or reader.write_sequence() != 456:
            raise AssertionError("reader did not recover after the ring reappeared")
        reader.close()
    print("PASS ring replacement, missing/partial ring and reconnect")


if __name__ == "__main__":
    vqec_vision_ai_tools_rrtst_run()
