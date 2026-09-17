# Qualcomm adapter — implementation blueprint

This document is the adapter blueprint for QCS6490 / Qualcomm Linux 1.8: it records the original
direct-SDK plan, module ownership, capability descriptor, preprocessing/inference steps, test
matrix and the BSP inputs still required. It is a blueprint, not current implementation status.

**Status:** source-delivered — the current adapter offers two neutral-port backends (released
plugin graph per [ADR 0002](../adr/0002_qualcomm_plugin_backend.md) and the optional LACAI-owned
QNN engine per [ADR 0003](../adr/0003_owned_qnn_engine.md) and
[execution policy](qualcomm_execution_policy.md)); on 2026-09-14 the owned engine was board-verified
(SCRFD/YOLOv8n compose, finalize and execute on HTP V68 with output byte-identical to
`qnn-net-run`, and the service harness plus `--mode production --platform fake` ran natively) and
the private QTI DMA-pool/`qtivoverlay`/`v4l2h264enc` renderer produced a color-correct person stream
on `.48`. This is integration-smoke evidence, not released-FW acceptance. **Layer:** adapters.
**Source:** `src/adapters/qualcomm`.

Baseline user-confirmed: QCS6490, Qualcomm Linux 1.8. Sysroot is not required to write code. The
historical direct-SDK blueprint below is not current implementation status. See
[implementation status](../development/implementation_status.md),
[QCS6490 board](../testing/qsc6490_board.md) and
[FW release compatibility](../contracts/fw_release_compatibility.md). The production service now
selects the private FastCV preprocessing adapter through the neutral `image_processor_port`; see
[qualcomm_preprocessing.md](qualcomm_preprocessing.md) for its exact semantics, ownership, measured
bottlenecks and remaining copies. Preview renderer/encoder must be separate from the inference
graph and preserve the released H264/ring contract; selecting Codec2 requires board evidence, not
just factory availability. Read [qualcomm_plugins_reference.md](../research/qualcomm_plugins_reference.md)
for all plugin usage details, alongside source review Q01–Q18.

Goal: FastCV image ops + QNN inference, neutral interfaces, safe ownership, performance measured
on target. Do not use OpenCV or GStreamer in core.

## Responsibility

- Own vendor-specific FastCV preprocessing and QNN execution behind neutral ports.
- Keep private vendor headers in this directory, not in `include/vqec/vision/ai/contracts`.
- Fail closed on missing library/symbol/version rather than silently falling back to CPU.
- Do not define neutral contracts, semantic postprocess or model rules in the adapter core.

## Module and ownership

| Planned file in `src/adapters/qualcomm` | Responsibility |
|---|---|
| `vqec_vision_sdk_loader.cpp` | allowed paths, symbols, versions, provider compatibility, RAII libs |
| `vqec_vision_buffer_manager.cpp` (not yet created) | planned AI-owned allocation/import/map/cache scope, pools, completion ownership |
| `vqec_vision_fastcv_processor.cpp` | crop/resize/color/rotate/normalize through actual available capability; geometry metadata |
| `vqec_vision_qnn_engine.cpp` | backend/device/context/graph/tensor bind/execute/completion/profiling |
| `vqec_vision_backend_factory.cpp` | capability probe + construct backend per board manifest |
| `vqec_vision_c2d_processor.cpp` (not yet created) | planned optional hardware blit/import path after benchmark + ADR |

Private vendor headers live in this directory, not in `include/vqec/vision/ai/contracts`. Public
override names follow the interface owner; helpers use the `qcom` + `file_id` registry. Semantic
postprocess belongs in perception/detection or a decoder module; do not embed YOLO/face rules in
`qnn_engine`. Vendor postprocess offload is an optional implementation with golden equivalence,
not a condition for every task that uses the adapter.

## Required capability descriptor

`board_id`, `soc_id`, `bsp_build_id`, `adapter_version`, `sdk_build_ids`; accepted source pixel
formats/modifiers/planes/alignments/max dimensions; image ops + output dtype/layout + actual
execution path; memory import/export/map support + cache/sync requirements; QNN backend id +
accepted artifact/compiler/runtime versions; graph/tensor restrictions, max contexts/inflight,
thread safety; sync/async/cancel/quiesce support, timeout recovery; measured workload profiles and
not-yet-measured flags.

Do not hardcode `supported=true` by vendor name. A missing library/symbol/version produces
`unsupported` with a reason, not a silent CPU fallback. A product policy that permits fallback must
be explicit, with a metric, budget and effective degraded state.

## RAW4K to model input

1. Validate descriptor, epoch, memory handles, bounds and negotiated profile.
2. Hold the frame lease; wait for acquire synchronization per the camera/BSP contract.
3. Import or map input through the confirmed BSP memory path.
4. Compute ROI/alignment/letterbox; clamp per policy, do not change model semantics on your own.
5. Preprocess into an AI-owned tensor/surface in a bounded pool.
6. Complete all source reads -> ACK Camera; release mapped/imported views per the
   SDK-guaranteed lifetime.
7. Bind input tensor -> QNN execute -> completion -> decoder.
8. Release output after every decoder consumer is done; recycle input when inference is done.

If several models read the same Camera frame, ACK after the last reading consumer, or create a
shared AI-owned intermediate. One finished branch must not ACK the whole frame. If Camera memory is
passed directly through to the SDK, the lease extends to SDK completion.

## FastCV implementation

- Probe the actual API from pinned headers/libs; do not declare function pointers with signatures
  guessed from another SDK.
- Start with synchronous execute in a bounded worker; completion is created after return.
- `fcv SetOperationMode` uses a fixed policy; check SDK global state and `CleanUp` before allowing
  several processors to coexist.
- Preallocate staging buffers per model plan; record count/bytes/copy metrics.
- Check NV12/NV21 plane order, stride and chroma ROI alignment separately.
- CPU mapping: cache begin/end per BSP; device fence dependency is a separate problem.
- Do not call normalize in both converter and tensor packing, causing double normalization.
- Advertise dtype INT8/UINT8/FP16/FP32 and layout NHWC/NCHW only after a golden pass.
- Resize-before-color-convert can save bandwidth but changes numerical ordering: change only if
  the model team accepts it and golden/accuracy passes.
- If UBWC input must be handled: require a supported BSP decompression/import path or negotiate
  linear NV12. Do not map and then treat UBWC as linear.
- Add C2D/GLES only if the FastCV path misses target and the SDK supports it; the source review
  shows this option exists but has not proved it optimal on the product board.

## QNN implementation

Init: verify artifact -> load backend/System libs -> enumerate compatible providers -> create
backend/device -> inspect binary metadata -> create context -> select graph by manifest name ->
validate all I/O -> allocate/bind -> warmup.

- A context binary is the default candidate; a `.so` model is a separate path if the product needs
  it.
- Check version-tagged metadata/tensor unions before reading members; deep-copy metadata if the
  source memory will be released by the SDK.
- Do not assume the first graph or one input. Reject graph/name/dtype/shape mismatch.
- Map quantization exactly per the SDK definition and model contract; do not assume an SDK offset
  uses the same `zero_point` convention. Keep each output's native dtype separate.
- Execute sync in v1 with a bounded executor; serialize the context unless the SDK confirms
  concurrent safety. A deadline is a scheduler contract; do not pretend QNN can cancel.
- Shared memory registration is an optimization phase after the client-buffer baseline. Implement
  it only if the BSP SDK supports that allocator/import; registration lifetime follows context +
  allocation; deregister after completion and before freeing context/buffer.
- The client-buffer baseline may copy inside the SDK: log the path, measure it, do not call it
  zero-copy.
- Do not recreate backend/context or `dlopen` the model every frame.
- Tear down in reverse dependency order after drain; metadata/profiling callbacks must not be
  invoked into a destroyed object/library; check partial-init at every step.

## Completion and recovery

Job states: `queued -> submitted -> completed/failed`; `cancel_requested` is intent, not a terminal
state for a device still using memory.

Stop source: stop submit -> drop queued -> await submitted completion -> release lease -> release
source. SDK stall: the control thread still answers health; quarantine bounded resources and
request FW/BSP recovery. The recovery contract must confirm device access stopped before
reusing/freeing the camera pool. Process kill/restart is safe only if the BSP guarantees DMA
teardown; this needs a test. Do not hold a mutex during `graphExecute`/`Finish`/`wait` or RPC.

## Test matrix

| Gate | Test | Evidence |
|---|---|---|
| A0 SDK inventory | missing lib/symbol, ABI mismatch, invalid artifact | deterministic error + no partial-init leak |
| A1 buffer | padded stride, multiple planes, FD reuse, epoch change | no OOB, exact release, mapping counters |
| A2 preprocess | crop borders, rotate, letterbox, RGB/BGR, quantized data | input tensor golden tolerance |
| A3 inference | bin/so if supported, graph select, mixed dtype, multi I/O, reordered outputs | tensor + decoded golden |
| A4 lifetime | slow execute, timeout, disable, disconnect, shutdown | no early ACK/use-after-free |
| A5 workload | RAW4K + live stream/record + feature combinations | latency/fps/CPU/RSS/thermal/copy report |
| A6 stability | repeated load/unload, 24–72h agreed soak, fault injection | no growing FD/RSS/pool leak, recovery trace |

24–72h is a proposed acceptance target, not a test that has run. A0–A4 pass before optimization;
profile A5 before increasing thread/batch/context. Hardware memory tests cannot be replaced by a
host sanitizer.

## Missing BSP inputs — blocker for production code

SoC + board revision; BSP image; Linux/toolchain/sysroot; FastCV headers/libs; QNN SDK and device
libs including required accelerator dependencies; known-good model/context + SDK demo;
allocator/import/cache/fence samples; supported NV12 4K source mode; permissions/device nodes;
thread/concurrency/reset documentation; redistribution policy; thermal/load budgets. Owner BSP,
deadline end week1/2.

## Limits and next work

- For the direct-SDK path these inputs remain required. ADR 0002 selects the current plugin-backed
  adapter, which cross-builds and has synthetic target smoke coverage.
- Do not fabricate SDK APIs or describe it as a live model-qualified streaming adapter.
- Released-FW camera/ring/RTSP acceptance, direct I/O DMA import, registered QNN memory,
  multi-graph QNN and thermal qualification remain open.
- Golden tensor parity, cascade crop/alignment and BSP recovery remain open.

## See also

- [Qualcomm plugin adapter reference](qualcomm_plugin_adapter_reference.md)
- [Neutral execution policy and the Qualcomm engine](qualcomm_execution_policy.md)
- [Qualcomm preprocessing adapter](qualcomm_preprocessing.md)
- [Qualcomm QNN ION-registered output memory](qualcomm_qnn_ion_memory.md)
- [ADR 0002 Qualcomm plugin backend](../adr/0002_qualcomm_plugin_backend.md)
- [ADR 0003 owned QNN engine](../adr/0003_owned_qnn_engine.md)
