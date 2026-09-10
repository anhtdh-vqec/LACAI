# QCS6490 board test target

The current development target is reachable as `root@192.168.138.98`. The local SSH
configuration provides alias `lacai-qsc6490` and identity
`/home/a/.ssh/lacai_qsc6490`. Use `ssh -o BatchMode=yes lacai-qsc6490` before asking
for credentials. Passwords must remain outside this repository and command output.

Observed on 2026-09-10:

- QCS6490 RB3 Gen2 Vision Kit, AArch64;
- Qualcomm Linux `1.8-ver.1.1`, kernel `6.6.119-qli-1.8-ver.1.1`;
- GStreamer `1.22.12`;
- `qtimlqnn` and `qtimlvconverter` load through `gst-inspect-1.0` from
  `/usr/lib/gstreamer-1.0`.

The validation candidate was built only with the approved eSDK and all available
adapter flags enabled:

```bash
source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux
cmake -S . -B build-esdk -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
  -DVQEC_VISION_AI_ENABLE_QUALCOMM=ON \
  -DVQEC_VISION_AI_ENABLE_CAMERA=ON \
  -DVQEC_VISION_AI_ENABLE_CAMERA_DBUS=ON \
  -DVQEC_VISION_AI_ENABLE_GST_FRAME_BRIDGE=ON
cmake --build build-esdk -j4
```

All 57 generated unit/contract binaries were copied to `/tmp/lacai-board-tests` and
executed natively on the board. Result on 2026-09-10: **57/57 passed**. This includes
Linux socket/FD fixtures, GStreamer DMA-BUF ownership helpers and the synthetic graph
submission/drain/retention lifecycle, plus neutral runtime composition from admission
through source-session/perception ownership. The graph fixture deep-copies its pass-through
buffer at a test-only pad probe so input release is independent from the synthetic
tensor output, matching the ownership shape of an inference transform.

The Qualcomm guard binary also has an opt-in installed-plugin check:

```bash
VQEC_VISION_AI_REQUIRE_QUALCOMM_PLUGINS=1 \
  /tmp/lacai-board-tests/vqec_vision_ai_plugin_graph_test
```

It verifies `appsrc`, `qtimlvconverter`, `capsfilter`, `qtimlqnn` and `appsink`, inspects
the converter/QNN properties used by LACAI, and configures the production graph to NULL
with explicit test metadata. The 2026-09-10 run passed with zero failed checks. It does
not call `load_model`, so the fixture paths are never opened.

This is target ABI and logic smoke evidence. The run did not acquire a live FW Camera
Service stream, load a model through `qtimlqnn`, prove HTP/FastCV execution, validate
device completion for a real DMA-BUF, measure performance or qualify BSP recovery.
Those require a pinned model/backend/system bundle and a controlled FW test source.
