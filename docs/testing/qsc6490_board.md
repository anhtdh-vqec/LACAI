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

## 2026-09-14 board run (device online)

Board reachable and used as the native target. Artifacts were built only with the approved
eSDK (expanded configuration) and copied to `/opt/anhtdh` on the board.

- Native test binaries: **81/81 passed** (all `vqec_vision_ai_*test*` executables),
  covering camera/GStreamer/Qualcomm fixtures and the neutral runtime, worker, pool,
  decoder, tracker, feature, encoder/ring and secondary-scheduler units.
- `VQEC_VISION_AI_REQUIRE_QUALCOMM_PLUGINS=1 vqec_vision_ai_plugin_graph_test`: exit 0
  (installed `qtimlvconverter`/`qtimlqnn` properties inspected, NULL-state graph config).
- Service executable:
  - `--mode harness ... --steps 160 --require-sources 2` → exit 0, `routed_sources=2`;
  - `--mode production --platform fake ...` → exit 0, `routed_sources=2`;
  - `--mode production --platform qualcomm` → exit 3 (fail-closed, no fallback).
- QNN runtime: `qnn-platform-validator --backend dsp --testBackend` → DSP unit test
  **Passed**, `Core Version = Hexagon Architecture V68`. Image QAIRT is 2.43.0.
- Model smoke `qnn-net-run` against `/usr/lib/libQnnHtp.so`: SCRFD-500M-KPS wrote
  `score_8/16/32`, `bbox_8/16/32`, `kps_8/16/32`; YOLOv8n-person wrote `boxes_out`,
  `conf_out`.
- LACAI-owned QNN engine `vqec_vision_ai_qnn_engine_smoke`: SCRFD (9 outputs) and
  YOLOv8n-person (2 outputs) **execute on HTP** (exit 0). This exposed and fixed a real
  defect: the generated model library composes but does not finalize the graph, so
  `prepare` must call `graphFinalize` before `graphExecute`.
- Numeric parity: with the same native input, the owned engine output is **byte-identical**
  to `qnn-net-run --use_native_input_files --use_native_output_files` — SCRFD 9/9 and
  YOLOv8n 2/2 tensors match. This is engine-versus-runtime parity, not model accuracy
  against a labelled reference.
- First board latency baseline (owned engine, synchronous `graphExecute`, client buffers,
  50 iterations after load; excludes preprocess and does not include graph prepare):
  SCRFD-500M-KPS min 3.77 / avg 5.20 / max 6.62 ms; YOLOv8n-person min 10.83 / avg 12.12 /
  max 13.27 ms. Recorded as a seed, not an acceptance threshold.

A later 2026-09-14 re-run after the QoS mailbox, source-session worker, metrics and CMake
changes re-passed **81/81** native test binaries. The service now also prints a metrics line
(`metrics steps=170 routed=42 delivered=42 denied=0 failed=0` for the two-source smoke), and
the owned engine still executes SCRFD (avg ~3.2 ms) and YOLOv8n (avg ~10.5 ms) on HTP.

Board workspace `/opt/anhtdh` holds `bin/`, `config/`, `inputs/`, `models/` and `out/`.
Newly written executables on the board's `/opt` overlay occasionally need a `sync` (or a
copy to `/tmp`) before exec; the native test binaries and service binary run there
directly.

Still not qualified: model accuracy (inputs were zero/random), async/shared/update, live FW
camera/DMA completion, hardware encoder/ring, performance and thermal. Those remain in the
board qualification backlog.
