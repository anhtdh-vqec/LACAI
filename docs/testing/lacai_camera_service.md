# Board camera service (QMMF) for independent tests

This is a board-side test input path that replaces the FW Camera Service for standalone
runs: it captures NV12 from `qtiqmmfsrc`, optionally runs the person model, and publishes
an H.264 RTSP view. It lets each test run without the real Camera Service and without the
FW control lease.

Status: source-delivered and board-verified on QCS6490 RB3 Gen2 (Qualcomm Linux 1.8) at
`/opt/anhtdh`. It is a test/observation path, not a product camera service and not a
zero-copy, performance, accuracy or feature-acceptance claim.

## Components

- `vqec_vision_ai_camera_service` (`tools/vqec_vision_camera_service.cpp`) — board tool.
  Pipelines:
  - capture: `qtiqmmfsrc ! video/x-raw,NV12,WxH@F ! videoconvert ! appsink`
  - output: `appsrc ! queue ! videoconvert ! NV12 ! v4l2h264enc ! h264parse ! qtirtspbin`
  For every captured frame it runs `reference_image_processor -> owned QNN engine ->
  yolov8_decoder`, draws the decoded boxes onto the NV12 planes and pushes the annotated
  frame to the RTSP pipeline. It uses a per-frame memfd so the CPU preprocessor can map the
  frame; this is a copy, not a zero-copy path.
- `tools/vqec_vision_board_deploy.sh` — installs the binary, model kit and two systemd
  units on the target.

Two services exist and only one may own the camera at a time:

| Unit | Pipeline | Purpose |
|---|---|---|
| `lacai-camera.service` | `qtiqmmfsrc -> v4l2h264enc -> qtirtspbin` | raw camera view |
| `lacai-ai.service` | capture + person model + overlay + RTSP | AI output view |

## RTSP

Both units serve `rtsp://<board-ip>:8900/live` (TCP or UDP). Example client:

```sh
gst-launch-1.0 -e rtspsrc location=rtsp://<board-ip>:8900/live protocols=tcp latency=500 \
  ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! autovideosink
```

## Run

```sh
source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux
cmake --build build-esdk-full --target vqec_vision_ai_camera_service -j4
VQEC_VISION_MODEL_SOURCE=<path>/libyolov8n_person_w8a16.so \
  tools/vqec_vision_board_deploy.sh <ssh-host> /opt/anhtdh/models/libyolov8n_person_w8a16.so
ssh <ssh-host> systemctl start lacai-ai.service
```

`qtiqmmfsrc` needs the QMMF camera server. If a pipeline is killed with `SIGKILL` the
camera server can wedge and later runs produce no frames; restart `cam-server.service` or
reboot, and stop pipelines with `SIGINT` (the units and `gst-launch -e` already do).

## Board evidence (2026-09-14)

- `qtiqmmfsrc camera=0` produced NV12 1280x720 at ~27 fps to a file (300 MB / 8 s).
- `lacai-camera.service` served H.264 RTSP; a `gst-launch` client received ~3 Mbps.
- `lacai-ai.service` processed ~30 fps with ~3 detections/frame and published the annotated
  stream. A captured frame shows green person boxes over the live scene.

Not qualified: hardware DMA completion, zero-copy import, encoder latency/bitrate under
load, model accuracy, and any FW Camera Service or recording behaviour.
