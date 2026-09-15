# QCS6490 board test target

The currently allocated development target is `192.168.138.99`. Board `.48` is in use by
another developer and must not be accessed until the user reallocates it. The existing
local alias may still point at an earlier target, so verify its resolved hostname before
using it. Try BatchMode access first. Passwords must remain outside this repository and
command output.

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

A later 2026-09-14 re-run after the QoS mailbox, source-session worker, supervisor async
mode, metrics, allocation reuse and CMake split re-passed **82/82** native test binaries.
Since the CMake split, executables are emitted under the module build directory
(`build-esdk-full/tests/`, `build-esdk-full/src/app/`, `build-esdk-full/src/adapters/...`),
not the build root; the board set was rebuilt from those paths. The service prints a metrics
line including routed-result latency
(`metrics steps=170 routed=42 delivered=42 denied=0 failed=0 e2e_avg_us=... samples=42`);
the reference fixture's pipeline PTS is not a steady-clock domain, so that latency value is
only meaningful when an adapter explicitly maps pipeline PTS from the step clock. The
owned QNN path does not currently provide that mapping. The owned engine still executes
SCRFD and YOLOv8n on HTP (latency varies with board
load; ~3-7 ms SCRFD, ~10-18 ms YOLOv8n across runs).

Board workspace `/opt/anhtdh` holds `bin/`, `config/`, `inputs/`, `models/` and `out/`.
Newly written executables on the board's `/opt` overlay occasionally need a `sync` (or a
copy to `/tmp`) before exec; the native test binaries and service binary run there
directly.

Model integration (2026-09-14, M0-M4): the `vqec_vision_model_runner` tool ran the real
YOLOv8n-person package end to end on the board (reference preprocess 2457600-byte input
tensor, owned QNN execute, `yolov8_decoder`, 0 detections on a plain gray NV12 fixture as
expected). Raw-output parity through the real preprocess path: `qnn-net-run` fed with the
runner's dumped input tensor produced `boxes_out` and `conf_out` **byte-identical** to the
runner's engine outputs (2/2). M2 preprocess golden and M4 decoded golden still need the
model team's reference tensor/detections.

Still not qualified: model accuracy (inputs were zero/random), async/shared/update, live FW
camera/DMA completion, hardware encoder/ring, performance and thermal. Those remain in the
board qualification backlog.

## 2026-09-14 live person-flow repair on `.48`

The additional target `192.168.138.48` was reached through the recorded BatchMode SSH
alias. The service was rebuilt with the approved eSDK and run against the real QMMF camera
through the compatibility FW camera service, owned QNN HTP engine and staged
YOLOv8n-person package.

```text
qtiqmmfsrc -> RAW lease -> preprocess -> QNN HTP -> decode/tracking
  -> QTI DMA pool -> qtivoverlay -> v4l2h264enc -> released ring
  -> compatibility FW RTSP -> ffmpeg client
```

The 1280x720 run returned 2-4 tracked person observations per routed result. A client
joined after startup and decoded a frame with three green person boxes. Visual inspection
confirmed the former green top band was gone. The repaired defects were: copying NV12 by
declared plane offset/stride; using a GPU-aligned QTI DMA surface required by
`qtivoverlay`; using Qualcomm's `0xRRGGBBAA` color order with nonzero alpha; and emitting
periodic IDR frames with SPS/PPS for bounded-ring late join.

Board values were supplied explicitly: BT.709, progressive, 4,000,000 bit/s, GOP interval
8, four output surfaces and opaque green `0x00FF00FF`. These are test values, not product
defaults. Because the compatibility camera uses memfd, the test includes CPU copies and
does not prove released-FW DMA-BUF interop, zero-copy, model accuracy, performance,
recording/UI behavior or long-run stability.

The first stream exposed a cadence coupling defect: the catalog intentionally requested
1 FPS inference, and the service rendered only result frames, reducing RTSP to 1 FPS. The
repaired source session retains one latest preview frame independently of inference and the
service applies the latest observation snapshot to every camera frame. A 10-second board
sample wrote 291 H.264 access units (**29.1 FPS**) while inference remained 1 FPS. Rebased
per-client RTSP timestamps reduced an `ffprobe` late-join startup sample to 0.98 seconds;
a five-second TCP RTSP decode received 151 frames. These measurements apply only to this
compatibility setup and are not a product performance or latency acceptance claim.

## 2026-09-15 live AI-throughput repair on `.48`

The model cadence was raised from the prior 1/1 smoke value to the source rate of 30/1 in
a board-only profile. The portable CPU preprocessor then limited the application to 97
results per ten seconds (9.7 FPS), with 96.55% of sampled cycles attributed to that stage.

The production platform now supplies a Qualcomm adapter through `image_processor_port`:
`qtivtransform(engine=fcv)` performs manifest-driven letterbox resize and
`qtimlvconverter(engine=fcv)` converts NV12 to UINT8 RGB. The graph's UINT16 quantization
is packed with AArch64 NEON because the plugin's native UINT16 request spent 81.74% of
sampled cycles in its generic normalization loop. Measured progression was 17.6 FPS for
that native UINT16 path and 30.1 FPS after UINT8 plus NEON packing.

With overlay, Qualcomm H.264 encode and ring output enabled, the service routed 300 model
results in ten seconds at 45.4% process CPU. An independent TCP RTSP probe decoded 241
1280x720 H.264 frames in eight seconds, or 30.1 FPS. The final `perf` sample attributed
24.31% of CPU cycles to FastCV color conversion, 8.34% to the QNN-side input/output copy,
5.00% to the remaining adapter preprocess work and 2.78% to YOLO tensor element decode.
The FastCV DSP scale call was present in the captured stack. See
[qualcomm_preprocessing.md](../architecture/qualcomm_preprocessing.md) for boundaries and
remaining qualification work.

This demonstrates frame-rate throughput for one source and one graph. The compatibility
camera still copies QMMF output into memfd, and the run does not establish released-FW
DMA-BUF interoperability, percentile capture-to-output latency, thermal stability,
multi-model capacity or model accuracy.

A follow-up attempt to use the service's `e2e_*` stop metric produced a multi-second
nonsensical value. Inspection confirmed that the owned QNN graph's internal pipeline PTS
anchor is not the executor steady-clock domain. That metric is therefore excluded from
this evidence; a future clock-domain contract must precede percentile latency claims.

## 2026-09-15 live face-cascade run on `.99`

The production service was cross-built with the approved eSDK, including the FastCV and
owned QNN adapters, then run on `.99` against the compatibility FW camera source, SCRFD
primary graph and EdgeFace secondary graph. Both model artifacts retained their recorded
SHA-256 digests outside Git. The service shut down and drained cleanly.

The first run routed 375 primary results and completed 99 embeddings, with six failed
cascade tasks. Every failure occurred when one source frame produced two accepted faces.
The secondary submission ledger required strictly increasing PTS, so it rejected the
second valid ROI because dependent jobs from one frame intentionally share frame identity
and PTS.

The corrected contract now has two explicit sequence policies. Full-frame graphs require
unique source frames. A dependent graph may accept consecutive jobs only when both frame ID
and PTS repeat exactly; equal PTS on another frame and backward PTS remain invalid. Source
PTS is preserved rather than fabricated per ROI. The post-fix run routed 445 primary
results, completed five embeddings and reported zero cascade failures. The scene in that
run did not contain two accepted faces in the same frame, so the repeated-task behavior is
covered by the eSDK logic test and still needs a live multi-face recheck.

A 15-second `/proc/<pid>/stat` sample measured 29.53% process CPU using the one-core
convention, with encoded output disabled. This is a short compatibility-source diagnostic,
not a product CPU, latency, thermal, model-accuracy or released-FW acceptance result. The
service `e2e_*` metric remains excluded because the QNN pipeline PTS and executor steady
clock have no established mapping. When the compatibility camera mock stopped, QMMF logged
a pending-buffer timeout and track deletion failure; no LACAI service process remained.
