# QCS6490 board test target

The currently allocated development target is `192.168.138.98`. Boards `.99` and `.48` are
in use by other developers and must not be accessed until the user reallocates them. The existing
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

## 2026-09-16 AI-owned protected gallery on `.98`

The eSDK-built production binary was staged separately from the existing live binary,
using the already staged licensed model packages and Zvec libraries. The original
transient Zvec collection and permitted enrollment JPEG were retained. The first
attempt exposed a Zvec C API behavior on a missing derived collection: its open error
was not `NOT_FOUND`. The adapter now checks the configured path before opening it;
missing means create, while unreadable or invalid existing paths fail closed. A real
Zvec fresh-rebuild regression test passes under eSDK/QEMU.

The service started with an AI-owned protected directory at mode 0700 and separate
configuration for its encrypted gallery/key/lock filenames, gallery ID, preprocessing
revision and byte limit. A trusted session-bus test peer invoked the image-path
`BeginEnrollment` request. `GetGalleryStatus()` reported `(1, 0, 0, true, false)`
before enrollment, then `(2, 1, 1, true, false)` after one accepted image. The three
protected files were owned by root at mode 0600; the ciphertext contained no plaintext
subject reference in a byte scan. This scan alone does not prove cryptographic safety.

After a clean service stop/restart, the peer read `(2, 1, 1, true, false)` without a
second enrollment; the encrypted file digest was unchanged. The service resumed both
model slots, with SCRFD cascade embeddings and no reported cascade failures in the
sampled log. A TCP host `ffprobe` returned H.264, 1920×1080, 30/1 for
`rtsp://192.168.138.98:8554/live/ai/detect0`. The live recognition label was not
visually rechecked in this run. The compatibility camera simulator and private session
bus do not establish released-FW D-Bus/camera integration. Power-cut durability,
hardware-bound key protection, backup/restore, liveness/accuracy, sustained FPS,
CPU/thermal and released-FW qualification remain open.

A further rapid restart temporarily failed to open the simulator's QMMF camera and the
service stopped with no running source session. Starting the simulator again after
resource release and then starting the service recovered both model slots and RTSP.
The final running test session used that recovered simulator process. Automated camera
restart/backoff and clean QMMF release need separate qualification; this incident does
not invalidate the protected-gallery revision and ciphertext recovery observation.

## 2026-09-16 enrollment image source on `.98`

After the target allocation moved to `192.168.138.98`, the eSDK-built POSIX path
authorizer and JPEG image-source tests passed natively. The opt-in production smoke ran
`jpegdec -> videoscale -> videoconvert -> qtivtransform engine=fcv -> appsink`; the adapter
verified the returned memory was DMA-BUF backed, validated its plane/allocation bounds and
retained the Gst sample owner through the neutral `raw_frame`. The first probe exposed an
invalid `memory:GBM` caps assumption and the second exposed the missing I420-to-NV12
conversion; the recorded passing run includes both fixes. This is native allocator/import
evidence for one synthetic image, not end-to-end FD/FR performance or zero-copy proof.

## 2026-09-16 image enrollment and live FR output on `.98`

The eSDK-built production service processed the authorized test JPEG through dedicated
SCRFD and EdgeFace graphs and completed the session-bus D-Bus enrollment request. The
terminal status reported one accepted sample and advanced the gallery from revision 1 to
revision 2. The image path was below the configured enrollment root; no image or embedding
was added to Git.

The first end-to-end attempt exposed two target-only issues. The GBM DMA-BUF returned by
`qtivtransform` could be mapped through the GStreamer allocator but not directly with
`mmap`; the cold image path now retains a packed memfd for FastCV alignment while detector
preprocessing continues to consume the DMA-BUF. The still-image detector also produced no
tracker identity, so the pipeline now assigns the immutable nonzero image buffer ID as the
request-local track identity after it has proved that exactly one landmark-bearing face
exists.

After enrollment, the same service process ran live recognition, Qualcomm overlay/H.264
encoding and the released FW ring. A host `ffprobe` TCP RTSP probe reported H.264,
1920x1080 and 30/1 FPS at `rtsp://192.168.138.98:8554/live/ai/detect0`; the ring write
sequence exceeded 800 without a recognition, label-correlation, render or executor error.
The service remained running so visual name matching could be checked by a person at the
camera.

This run used the compatibility camera mock. At one sample the service used about 57% of
one CPU and the mock about 34%; the mock performs a full NV12 copy into memfd and the
renderer performs another copy into a QTI surface. These figures do not represent the
released FW DMA-BUF path and are not a CPU acceptance result. A prior camera HAL run had
orphaned CSL resources and reported LRME allocation failure; a controlled reboot of the
assigned `.98` test board restored direct `qtiqmmfsrc` capture before the passing run.

## 2026-09-16 FR cascade run

The board was reachable at `.99` using the approved test account. The production binary
was rebuilt with the eSDK and `VQEC_VISION_AI_ENABLE_FASTCV=ON`; the required Zvec shared
libraries were staged outside the repository under `/opt/anhtdh/lib`. A 300-step run used
the face deployment/catalog/package registry and the live camera simulator on
`/run/camera_ai`:

```text
steps=305 routed=24 delivered=0 denied=0 failed=0
cascade_tasks=1 cascade_embeddings=1 cascade_failed=0 first_error=0
```

SCRFD and EdgeFace prepared and executed on the board, and the cascade produced one
embedding without a graph or ownership failure. The run had an empty gallery, so no
identity label was expected. The service's `e2e_avg_us` remains unusable for latency
acceptance because the current camera pipeline PTS is not mapped to the service steady
clock; this is tracked separately from the successful execution evidence. No claim of
25--30 FPS FR output or attendance readiness is made from this run.
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

## 2026-09-16 combined person + FR run on `.98`

The authorized `.98` target ran the eSDK-built production service with two primary roots
on one 1920x1080 source: `yolov8n_person` at model slot 0 and `scrfd_500m_bnkps` at slot 1.
`edgeface_s_gamma_05` remained a secondary SCRFD dependency. File enrollment through the
private D-Bus session completed one template and advanced the gallery revision from 1 to 2.

The first combined activation was rejected before acquisition because the declared 16 MiB
tensor budget was smaller than the admitted model closure. Raising the example envelope to
32 MiB allowed composition. After a clean board restart, logs repeatedly reported person
and face results plus successful `cascade_accepted=1 embedded=1 cascade_failed=0` samples.
No cascade failure appeared in the verified run.

A host TCP RTSP probe reported H.264, 1920×1080 and 30/1. A captured frame visibly contained
one green `person` box and two separate face boxes. This proves concurrent composition and
overlay retention in the compatibility setup. It does not prove model accuracy, persistent
gallery recovery, released-FW DMA-BUF interop, thermal stability or load/unload behavior for
the proposed dynamic usecase control plane.

## 2026-09-16 runtime-control and FR validation (.98)

Approved eSDK candidate: 120/120 QEMU CTest; 114/114 native logic/contract binaries
after supplying target decoder-package manifest fixtures. Same-process D-Bus runtime
switching checked person/FD/FR model-library residency, all-off control availability,
peer authorization and protected-gallery preservation. File enrollment/retry/conflict
and two-template subject deletion passed. Ring-reader replacement regression passed
natively; ffprobe decoded final H264 at 1920×1080 with metadata `30/1`.
See [FR validation](face_recognition_production_validation.md) for the full matrix and
open production gates. This does not certify thermal/AI FPS, accuracy, signed grants,
TEE keys, fault recovery or released-FW DMA-BUF completion.

Private-index follow-up on `.98`: Zvec now uses configured private tmpfs with mode-0700
parent/collection, pins the parent FD, rejects unsafe paths/modes and destroys derived
files on close. Real-library private-storage regression and D-Bus disable/re-enable/file
enrollment rerun pass; disabled FR leaves no derived collection. The obsolete persistent
collection was removed after the encrypted gallery rebuilt successfully. Swap/crash-dump
and hardware-key qualification remain open.

## 2026-09-16 CPU optimization and profiling on `.98`

Following comparative analysis against legacy `ai_app` (which utilized ~25% of one core for
single-model person flow on QCS6490), LACAI underwent three targeted optimization phases
built with the approved eSDK toolchain and verified natively on `.98`:

1. **Phase 1 (Supervisor Pacing Control)**:
   - Defect: Default supervisor step interval was 250 µs (4,000 wakeups/s), causing the main
     thread to burn ~50% CPU spinning on empty queues.
   - Fix: Added configurable `--runtime-step-interval-us` pacing (tested at 5,000 µs and
     15,000 µs), aligning supervisor iterations with the 30 FPS (33.3 ms) video source rate.
   - Evidence: Supervisor main thread CPU dropped from 50.0% to 10.9%.

2. **Phase 2 (QNN ION Zero-Copy & Pre-allocated Workspace)**:
   - Optimization: Integrated dynamic `libcdsprpc.so` (`rpcmem`) to register physical ION memory
     blocks (`QNN_MEM_TYPE_ION`) with HTP via `QnnMem_register`, enabling HTP to DMA-write directly
     into physical buffers. Pre-allocated `output_workspace_` during `prepare()`, eliminating
     per-frame `std::vector` heap reallocations and zero-initializations on the hot path.
   - Teardown: Strict lifecycle ordering (`memDeRegister` -> `rpcmem_free` -> `freeGraphsInfo` ->
     `dlclose` -> `contextFree`).

3. **Phase 3 (FastCV Neon SIMD Color Conversion & Persistent Scratch Workspaces)**:
   - Optimization: Replaced CPU scalar color loop with ARM Neon SIMD vectorized
     `fcvColorYCbCr420PseudoPlanarToRGB888u8` for BT.601 limited NV12 (with 8-byte stride alignment).
     Introduced persistent thread-confined scratch workspaces (`rgb_scratch_`, `planes_scratch_`,
     `patches_scratch_`, `luma_scratch_`), eliminating 7 heap allocations per detected face.

4. **Phase 4 (Renderer Cache, Plane Memcpy, Queue Removal & Deadline Loop Pacing)**:
   - Defect: Frame drops down to 13–14 FPS and 50–60% CPU were traced to four bottlenecks:
     a) Unconditional `sleep_for` in `service_main.cpp` creating a 35–40 ms loop period (> 33.3 ms 30 FPS period), overwriting incoming camera frames before acquisition.
     b) Per-frame `::mmap` / `::munmap` on 4 MiB NV12 buffer causing page-fault storms and TLB shootdowns.
     c) 1,620 individual `std::memcpy` calls per frame across NV12 row strides.
     d) Redundant GStreamer `queue` element inside the `qtivoverlay` pipeline burning 7.5%–10.0% CPU.
   - Fixes:
     - Implemented deadline loop pacing (`step_cost_ns < interval ? sleep(interval - step_cost_ns) : 0`).
     - Added 8-slot cached `mmap` avoiding repeated system calls and page faults on rotating buffer FDs.
     - Replaced line-by-line copies with 2 contiguous ARM64 Neon memory burst copies for Y and UV planes.
     - Drained all available access units from `appsink` in a non-blocking pull loop.
     - Removed redundant `queue` element between `appsrc` and `qtivoverlay`.

5. **Phase 5 (Decoder Direct Float32 Pointer Indexing)**:
   - Defect: YOLOv8 (8,400 anchors) and SCRFD (16,800 anchors) decoders invoked `vqec_vision_ai_detec_tnrd_read_scalar`
     up to 504,000 times/second at 30 FPS, performing repeated bounds checking, integer divisions,
     modulos, type switches, and `memcpy` calls.
   - Fix: Added fast-path direct `float32*` array indexing in `yolov8_decoder` and `anchor_distance_decoder`,
     reducing decoder CPU overhead while preserving exact schema validation and typed fallbacks.

6. **Phase 6 (Live Video Stream Frame-Stall Bugfix in `qtiv_renderer`)**:
   - Defect: Caching virtual memory mappings keyed purely by the kernel integer file descriptor (`slot.fd_ == frame_fd`) was flawed because Linux reuses the lowest available FD immediately upon `close(fd)` of the previous frame. Subsequent camera frames received over SCM_RIGHTS were assigned the recycled FD number, causing `copy_nv12` to repeatedly copy pixels from the stale mapping of the first frame. The stream rendered 30 FPS H.264 packets with updating bounding boxes but froze the camera background pixels on frame 1 ("giật yên tại 1 frame").
   - Fix: Replaced the unsafe FD-based mapping cache with an RAII `vqec_vision_ai_qcom_qtvr_mmap_guard` that maps each incoming frame freshly and unmaps it reliably upon exit, while retaining the optimized contiguous 2-plane memory copy.
   - Verification on `.98`: Live video frames now dynamically update with real movement (verified with 21.5% inter-frame pixel changes across a 2-second interval). Output stream sustained at 27.9–30.1 FPS with 63.9%–66.3% single-core CPU on the full dual-model + FR pipeline.

### Measured Board Evidence (.98)

- **RTSP Stream Output Rate (Measured via FFmpeg TCP probe over 10 seconds)**:
  - Nominal camera rate: **30 FPS (1920×1080 NV12)**.
  - Measured output: **301 frames in 10.00 seconds = 30.1 FPS**.
  - Frame drop rate: **0% (Zero dropped frames)**.
  - Previous baseline before Phase 4: 13–14 FPS (>50% dropped frames).

- **Single-model Person flow (`yolov8n_person` @ 30 FPS 1080p, overlay, V4L2 H.264 HW encode, ring)**:
  - Total process CPU: **36.0% – 38.0% of a single core** (representing **~4.5%** of the 8-core SoC capacity).
  - Thread breakdown:
    - Preprocessing thread (`input:s+`, `qtimlvconverter` 1080p -> 640x640): 15.9% – 17.0%
    - Supervisor main thread: 7.5% – 8.0%
    - Output render & appsrc (`src:src`): 6.5% – 7.0%
    - Worker thread (YOLOv8 decoder): 3.5% – 4.0%
    - V4L2 H.264 HW encode: 0.5% – 1.0%
    - GStreamer queue thread: Eliminated (0.0%)

- **Dual-model + FR Cascade flow (`yolov8n_person` + `scrfd_500m_bnkps` + `edgeface` @ 30 FPS)**:
  - System idle: **75.0% – 78.0% idle** (SoC-wide CPU load is only 22% – 25%).
  - Total process CPU: **~60.0% – 65.0% of a single core** across all active threads:
    - Main supervisor thread: 23.5%
    - Preprocessing thread 1 (`input:s+`, YOLOv8): 12.0%
    - Preprocessing thread 2 (`input:s+`, SCRFD): 12.0%
    - Output render & appsrc (`src:src`): 6.5%
    - Worker threads (fast-path decoders): 2.5% – 4.0% each
    - V4L2 H.264 HW encode: 0.5%
    - Cascade status: Zero cascade failures (`cascade_failed=0`), steady 30 FPS inference routing.

### Comparison Against Legacy `ai_app`

Legacy `ai_app` was observed at ~15%–20% of one core for person detection because:
1. **Inference Cadence**: `ai_app` configured `"inference_fps": 10` by default (running 3× fewer inferences per second than LACAI).
2. **cDSP Preprocessing Offload**: `ai_app` offloaded image letterboxing (`ScaleDownMNu8` and `ColorYCbCr420PseudoPlanarToRGB888u8`) to the Hexagon cDSP via `libvqec_dsp_skel.so`.
3. **cDSP Postprocessing Offload**: NMS and coordinate decoding were executed on the cDSP.

LACAI runs models at **full 30 FPS** (matching preview rate, per user requirement) using Qualcomm Linux standard plugins (`qtimlvconverter` + `qtimlqnn`) on CPU FastCV. At full 30 FPS:
- Single-model person flow: **~36% of 1 core** (~4.5% SoC load).
- Dual-model + FR flow: **~64% of 1 core** (~8.0% SoC load).
- Output stream: **Steady 28–30.1 FPS live real-time video with zero frame drops**.


## 2026-09-17 clean-base native suite runner

The ad-hoc native batch loop is replaced by
`tools/vqec_vision_board_native_tests.sh <test_dir> <manifest_models_dir> <zvec_tmpfs_root>
<zvec_scratch_base>`. Two device-free tests need fixtures and otherwise exit non-zero (a
missing-fixture result, not a code failure):

- `vqec_vision_ai_decoder_package_test` needs the staged `manifests/models` tree;
- `vqec_vision_ai_zvec_embedding_index_test` needs an unused collection path on a NON-tmpfs
  parent plus a current-UID-owned mode-0700 tmpfs directory for its private-index checks.

With those supplied on `.98`, the cross-built native suite is **117/117 passed**. This is
logic/contract evidence; it is not device DMA completion, model accuracy, released-FW or
performance acceptance.
