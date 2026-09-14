# FW mock services for LACAI board tests

These two board-side services replace the product FW for integration tests only. LACAI
itself is the production binary `vqec_ai_vision_applications`; the mocks never implement
AI, overlay, encode or ring production. They are test satellites, not product code.

## 1. Mock FW camera (`tools/vqec_vision_fw_camera_sim.py`)

Reproduces the two FW camera responsibilities LACAI depends on, sourcing pixels from the
real Qualcomm camera through `qtiqmmfsrc`:

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
`shared/raw_frame_transport` wire. The FD is a memfd holding a copy of each NV12 frame, not
a vendor dma-buf: the wire, socket naming, lease and ACK semantics match FW, the memory
backing does not.

## 2. Mock FW RTSP (`tools/vqec_vision_ring_rtsp.py read`)

Reads the released FW shared-memory encoded ring that the AI side produces
(`/dev/shm/camera_ai_<ring_id>`, version 5 layout) and publishes it as RTSP. This replaces
the FW RTSP service without the FW application sources. The reader never touches the
producer mutex/consumer registry: it polls `write_sequence` and copies slots under the
per-slot seqlock, so it is a lock-free observer. It publishes
`rtsp://<host>:<port><mount>` (default `8554`, `/live/ai/detect0`) through `qtirtspbin`.

By design the AI binary owns overlay, H.264 encode and ring production (see
[`fw_ring_sink`](../architecture/fw_ring_sink.md),
[`encoded_output`](../architecture/encoded_output.md),
[`encoder_preparation`](../architecture/encoder_preparation.md)); FW owns RTSP. The mock
RTSP service is therefore the whole output-side FW replacement.

## Notes

- Only one process may hold the camera; stop a pipeline with `SIGINT`.
- `qtiqmmfsrc` needs the QMMF camera server; a hard-killed pipeline can wedge it.
- These are logic/wiring aids. They are not accuracy, hardware, zero-copy or acceptance
  evidence, and they do not modify the repository.
