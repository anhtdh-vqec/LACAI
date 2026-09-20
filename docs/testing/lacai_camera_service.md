# FW mock services for LACAI board tests

This document describes the two board-side mock FW services used only for LACAI integration
tests: a camera mock and an RTSP ring reader. They replace the product FW for testing and
never implement AI, overlay, encode or ring production.

**Status:** board-smoke — the mock camera and RTSP ring reader drove recorded board runs
(see [QCS6490 target](qsc6490_board.md)); the ring regression is native-tested. **Layer:**
reference. **Source:** `tools/fixtures/vqec_vision_fw_camera_sim.py`,
`tools/fixtures/vqec_vision_ring_rtsp.py`.

These two board-side services replace the product FW for integration tests only. LACAI
itself is the production binary `vqec_ai_vision_applications`; the mocks never implement
AI, overlay, encode or ring production. They are test satellites, not product code.

## Responsibility

- Reproduce only the FW camera control/media and RTSP ring-reader responsibilities LACAI
  depends on, for integration tests.
- Must not implement AI, overlay, encode or ring production, and must not be treated as
  zero-copy, model-accuracy or released-FW acceptance evidence.

## File enrollment service policy

When the face-enrollment D-Bus adapter is enabled, production startup also requires at
least one `--enrollment-image-root` plus `--enrollment-max-image-bytes`,
`--enrollment-image-timeout-ms`, `--enrollment-jpeg-decoder`,
`--enrollment-converter`, `--enrollment-scaler`, `--enrollment-transform` and
`--enrollment-transform-engine`. Select these values from the installed BSP and deployment
policy. The service does not choose a fallback plugin. The requested JPEG must already be
inside an allowed root on the target filesystem; biometric test files are never committed.

## 1. Mock FW camera (`tools/fixtures/vqec_vision_fw_camera_sim.py`)

Reproduces the two FW camera responsibilities LACAI depends on. Its default `camera` source
uses the real Qualcomm camera through `qtiqmmfsrc`; the explicit `test_pattern` source emits
deterministic NV12 while preserving the same D-Bus, socket, DMA-BUF and ACK path:

- **Control:** owns the system-bus name `com.vnpt.camera.Camera` and implements
  `com.vnpt.camera.Camera1` `StartStream`/`StopStream`/`GetStatus` with `(a{sv})` string
  dictionaries, so LACAI's `dbus_rpc`/`camera_control`/`source_lifecycle` acquire the
  third-stream lease exactly as against real FW.
- **Media:** serves the released raw-frame wire on an AF_UNIX `SOCK_SEQPACKET` socket named
  `<socket_dir>/<channel>_third_<consumer>.sock` (`0_third_ai.sock`). Each message is the
  packed 104-byte native-endian `FrameHeader` plus one FD via `SCM_RIGHTS`; the consumer
  returns the 8-byte `ReturnHeader` ACK before the frame is released.

The header layout and socket naming match
[`raw_source_port`](../architecture/raw_source_port.md),
[`camera_legacy_adapter`](../architecture/camera_legacy_adapter.md) and the FW
`shared/raw_frame_transport` wire. The simulator packs NV12 into an ACK-gated, bounded
buffer pool. The default backing is memfd. An explicit `--dma-heap <device>` selects a
Linux DMA-BUF heap for a registered-FastRPC fixture; the path must be a reviewed device
node on the target. The sender closes its duplicated descriptor after `SCM_RIGHTS`, but
does **not** reuse the pool allocation until the exact `buf_id` is ACKed. Unknown or
duplicate ACKs fault the connection. On disconnect, the old pool is discarded, not reused
by the next connection. DMA-BUF writes are bracketed by `DMA_BUF_IOCTL_SYNC` CPU write
start/end; this is cache maintenance, not a device-completion fence. The copy from QMMF
into the pool remains, even in DMA-BUF mode. Neither mode establishes released-FW
allocator/fence compatibility, end-to-end zero-copy or product performance acceptance.

The board fixture selects DMA-BUF backing only when explicitly requested, for example
`--dma-heap /dev/dma_heap/qcom,system`. The device node is board-specific and must be
confirmed with BSP; it is not a product default. `--max-in-flight` sizes the bounded pool,
and every slot remains unavailable until its matching ACK. The device-free regression is
`PYTHONDONTWRITEBYTECODE=1 python3 tools/fixtures/vqec_vision_fw_camera_sim_test.py`.
Use `--source test_pattern` (or `LACAI_CAMERA_SOURCE=test_pattern` with the board runner)
only to isolate AI lifecycle, QNN/HTP, overlay, encode and ring behavior from sensor/HAL
availability. It is synthetic-media evidence and never camera-sensor or released-FW acceptance.

## 2. Mock FW RTSP (`tools/fixtures/vqec_vision_ring_rtsp.py read`)

Reads the released FW shared-memory encoded ring that the AI side produces
(`/dev/shm/camera_ai_<ring_id>`, version 5 layout) and publishes it as RTSP. This replaces
the FW RTSP service without the FW application sources. The reader never touches the
producer mutex/consumer registry: it polls `write_sequence` and copies slots under the
per-slot seqlock, so it is a lock-free observer. It publishes
`rtsp://<host>:<port><mount>` through `GstRtspServer` and
`rtph264pay name=pay0`. `--fps` is required. When a client connects, the reader scans the
retained ring window for an IDR containing SPS/PPS, allowing late join when the configured
GOP fits inside that window. The reader detects ring inode replacement/removal after runtime switching, closes its
obsolete mapping and resumes only from an IDR with SPS/PPS. Within an existing RTSP
media session timestamps remain monotonic across ring replacement.
PTS starts at zero for each RTSP media generation; ring-global
sequence is never used as client running time, so a late join does not inherit process uptime
as startup delay.

```bash
python3 tools/fixtures/vqec_vision_ring_rtsp.py read \
  --ring-id encoded_ai_detect0_cam0_ch0 \
  --port 8554 --mount /live/ai/detect0 --fps 30
```

By design the AI binary owns overlay, H.264 encode and ring production (see
[`fw_ring_sink`](../architecture/fw_ring_sink.md),
[`encoded_output`](../architecture/encoded_output.md),
[`encoder_preparation`](../architecture/encoder_preparation.md)); FW owns RTSP. The mock
RTSP service is therefore the whole output-side FW replacement.

## Notes

- Only one process may hold the camera; stop a pipeline with `SIGINT`.
- `qtiqmmfsrc` needs the QMMF camera server; a hard-killed pipeline can wedge it.
- The camera mock and renderer each perform an explicit CPU copy before QTI hardware
  overlay/encode. These are wiring aids, not zero-copy, model-accuracy or released-FW
  acceptance evidence.

## Runtime usecase/enrollment acceptance runner

Use `tools/fixtures/vqec_vision_fr_runtime_dbus_test.py` with a private deployment fixture to retain
one authenticated FW peer while checking live model load/unload, image enrollment,
idempotency and multi-template removal. See
[FR validation](face_recognition_production_validation.md)
for fixture fields, native/QEMU results and outstanding release gates.

Synthetic replacement regression: `PYTHONDONTWRITEBYTECODE=1 python3
/opt/lacai/tools/fixtures/vqec_vision_ring_rtsp_test.py` on the board with GstRtspServer GI.
The development host lacks that GI namespace; this Python regression is native-tested.

## Limits and next work

- The compatibility camera copies QMMF output into memfd or optional DMA-BUF; neither establishes
  released-FW DMA-BUF interoperability, zero-copy or performance acceptance.
- The optional DMA-heap fixture tests AI-side FD registration/import and remote processing
  only. BSP/FW still must provide the released allocator, synchronization, exact frame
  ownership and independent conformance evidence.
- Hardware overlay/encode through the mocks remains a wiring aid, not product evidence.

## See also

- [raw_source_port](../architecture/raw_source_port.md)
- [camera_legacy_adapter](../architecture/camera_legacy_adapter.md)
- [FR production validation](face_recognition_production_validation.md)
- [QCS6490 board test target](qsc6490_board.md)
