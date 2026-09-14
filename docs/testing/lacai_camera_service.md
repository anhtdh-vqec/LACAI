# Board camera fixtures for LACAI integration tests

These board-side test helpers let LACAI run against a local camera without the product FW
Camera Service, and let a viewer watch the feed. They are test aids: not product camera
service, zero-copy, accuracy or hardware-acceptance evidence.

## 1. Mock FW camera service (`vqec_vision_fw_camera_sim.py`)

Reproduces the two FW responsibilities LACAI depends on, sourcing pixels from the real
Qualcomm camera through `qtiqmmfsrc`:

- **Control:** owns the system-bus name `com.vnpt.camera.Camera` and implements
  `com.vnpt.camera.Camera1` `StartStream`/`StopStream`/`GetStatus` with `(a{sv})` string
  dictionaries, so LACAI's `dbus_rpc`/`camera_control` acquire a third-stream lease as
  against real FW.
- **Media:** serves the released raw-frame wire on an AF_UNIX `SOCK_SEQPACKET` socket named
  `<socket_dir>/<channel>_third_<consumer>.sock` (`0_third_ai.sock`). Each message is the
  packed 104-byte native-endian `FrameHeader` plus one FD via `SCM_RIGHTS`; the consumer
  returns the 8-byte `ReturnHeader` ACK before the frame is released.

The header layout and socket naming match the FW
[`shared/raw_frame_transport`](../../../../FW_CAMERA/vqec_camera_service/shared/raw_frame_transport)
wire and LACAI's [`vqec_vision_legacy_wire.hpp`](../../src/adapters/camera/vqec_vision_legacy_wire.hpp).

The FD this mock sends is a memfd holding a copy of each NV12 frame, not a vendor dma-buf.
The wire, socket naming, lease and ACK semantics match FW; the memory backing does not.

Board evidence (2026-09-14, QCS6490 RB3 Gen2):
`StartStream` returned `{code=0, stream_handle, codec=RAW, width=1280, height=720, fps=30}`;
a `SOCK_SEQPACKET` client received consecutive frames with `format=23` (NV12), `n_planes=2`,
a live FD and varying pixels, and ACKs were accepted.

## 2. Raw camera RTSP view (`run_camera_rtsp.sh`, `lacai-camera.service`)

`qtiqmmfsrc -> v4l2h264enc -> h264parse -> qtirtspbin` publishes the raw camera feed at
`rtsp://<host>:8900/live`. Use it to watch the source independently of LACAI.

Only one process may hold the camera. Stop a pipeline with `SIGINT`, not `SIGKILL`; a
hard-killed `qtiqmmfsrc` can wedge the camera server.

## 3. Planned output view

LACAI owns preview overlay/encode/ring production; FW owns RTSP. The output view service
will read LACAI's contract output and publish RTSP using Qualcomm plugins that do not run
on the CPU (`qtivoverlay`, `qtimlvconverter`), not the removed in-process AI tool.

Deploy with `tools/vqec_vision_board_deploy.sh <ssh-host>`.
