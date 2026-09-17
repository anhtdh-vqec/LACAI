# Qualcomm source review

This dated review records the observations from a read-only inspection of the Qualcomm
GStreamer plugin source and the limits of that evidence. It is the review basis for the
Qualcomm adapter; it does not claim board measurement.

**Status:** source-delivered — read-only review dated 2026-09-09 of the external
`gst-plugins-qti-oss` checkout; no board compile/run or measurement. **Layer:** reference.
**Source:** `n/a`.

Review date: 2026-09-09.
Local repository: `/home/a/Workspace/gst-plugins-qti-oss`
HEAD: `0cdf24a99c625fa616564ebf82fd8813c744ed82`.
`git status --short` at review time: clean. The reference repository was not modified.

Current integration guidance is normalized at the
[Qualcomm plugin adapter reference](../architecture/qualcomm_plugin_adapter_reference.md).
This document keeps the source observations and evidence limits as a review basis.

## Responsibility

- Records direct source observations from the AI-relevant plugin paths and the design
  decisions they support.
- States what must not be inferred from the source and what to check before reusing logic.
- Must not be read as a full-repository audit, board measurement, throughput/accuracy result
  or a commitment for every SDK.

## Scope and evidence level

The focused reading covered AI-path implementations: QNN loader/execute/cleanup, FastCV
conversion/sync/mode, converter normalization, C2D completion/import, DMA allocator, ML
converter transform/pools, object-detection sample wiring, postprocess module and tracker
dependency. The multistream sample catalog was surveyed. This is not a full-repository
audit, codec/camera stack, every model decoder or the proprietary SDK. Nothing was
compiled/run on a board and no throughput/accuracy was measured. The local source is
evidence of this plugin edition's behavior, not a commitment for every SDK.

Every LACAI build/test/CMake uses the eSDK at `/home/a/Workspace/eSDK`; reading plugin
source does not replace runtime checking on the QCS6490 image.

The paths below are relative to the above repository; line numbers are a snapshot.

## Evidence -> design decision

| ID | Source / symbol | Direct observation | Applicable to AI APP |
|---|---|---|---|
| Q01 | gst-plugin-mlqnn/ml-qnn-engine.cc:520 setup_backend | dlopen backend, getProviders, choose providers[0], create backend/device/profiler | loader selects an ABI-compatible provider with checks; do not blindly take the first element |
| Q02 | same file:689 setup_cached_graphs | QNN System reads binary metadata, contextCreateFromBinary, graphRetrieve | prefer a context binary pinned to SDK/target; validate metadata before run |
| Q03 | same file:804 setup_uncached_graphs | model .so, composeGraphs, contextCreate, graphFinalize | compatibility path for a separate .so; do not assume bin and so follow the same procedure |
| Q04 | same file:1131 execute | assign clientBuf pointer, synchronous graphExecute, graph_infos[0] | async at the executor does not mean SDK async; the job keeps input/output; explicit graph_name |
| Q05 | same file:982–1000 and 1190–1210 | workaround output float32 and convert native->float | keep per-tensor dtype; dequantize only what the decoder needs |
| Q06 | gst-plugin-base/gst/video/fcv-video-converter.c:2146 compose | warns async unsupported; direct compose | backend advertises synchronous; a worker wrapper provides real completion |
| Q07 | same file:2266 wait_fence, flush | warning Not implemented; wait returns true | do not use a stub as a fence/quiesce guarantee |
| Q08 | same file:653 stage buffer, 846 conversion, 2388 mode | staging/copy present; op modes low-power/performance/CPU offload/CPU performance | FastCV does not imply zero-copy or every op on DSP |
| Q09 | gst-plugin-base/gst/video/video-converter-engine.c:260 normalize_ip | CPU loop for normalization; index based on width/bpp | use golden + stride-aware code; do not copy the packed assumption into an arbitrary frame |
| Q10 | same file:309 default_backend | prefers GLES if built, then C2D, then OCV; FCV initialized by default | choose FastCV explicitly, do not inherit backend auto selection |
| Q11 | gst-plugin-base/gst/video/c2d-video-converter.c:471,1189 | map device address; internal GArray requests fence; Finish before normalize | distinguish completion token from Linux sync_file FD; C2D is a gated option |
| Q12 | gst-plugin-base/gst/allocators/gstqtiallocator.c:104 | DMA heap or ION per build; pool reuse and map | allocator depends on BSP; do not hardcode allocator for every Qualcomm |
| Q13 | gst-plugin-mlvconverter/mlvconverter.c:2320–2390 | output DMA sync START/END, compose calls fence=NULL | cache sync is not a substitute for device dependency; audit input/output fully |
| Q14 | gst-sample-apps/gst-ai-object-detection/main.c:537,550,781,1169 | converter -> QNN -> postprocess; QNN backend HTP path | use as a reference vertical-slice benchmark; do not use qmmfsrc in production AI |
| Q15 | gst-plugin-mlpostprocess/modules/object-detection/ml-postprocess-yolov8.cc | decoder/NMS C++ and specific tensor caps | a YOLO name is not enough to determine output; decoder per model manifest, not in BSP |
| Q16 | gst-plugin-objtracker/algorithm/bytetrack/CMakeLists.txt + dataType.h | tracker code uses Eigen | tracking is portable perception; not vendor hardware inference |
| Q17 | gst-plugin-base/gst/video/CMakeLists.txt | FCV/C2D/GLES/OCV compile conditional | do not link the whole base library and accidentally pull OpenCV into the product |
| Q18 | gst-plugin-mlqnn/README | needs a separate QNN SDK and distro integration | the OSS checkout is not enough to fully build the target adapter |

## What must not be inferred

1. A DMA-BUF camera may not be directly importable into QNN. No memRegister/memDeRegister
   path was seen in the reviewed ml-qnn-engine.cc; check the BSP SDK.
2. QNN HTP does not prove preprocess, postprocess and tracker all run on HTP.
3. The C2D wrapper's gpointer fence API is not an FD that can be sent directly over IPC.
4. A plugin float32 output is a wrapper choice, not a universal QNN requirement.
5. A multistream sample with several engine branches does not prove a shared model scheduler.
6. Do not equate qmmfsrc raw Bayer with the post-ISP video/x-raw NV12 the AI requires.
7. Sample dimension/caps metadata does not replace the Model Integration Package.

## Points to check carefully if reusing logic

- Select the provider/version union via the official SDK API; do not copy pointer arithmetic
  that reads interface metadata.
- Validate tensor count and output-name mapping independently of input count/output count;
  the plugin has shared graphindices needing regression for subset/reordered outputs.
- Audit partial-init cleanup and binary-reading buffers: audit RAII on all error exits; do
  not infer safety just because the sample has `free()`.
- Do not cache device surfaces based only on a reused integer FD.
- The sample's CPU normalization needs stride/range/quant rounding verified for the model.
- FastCV operation mode/cleanup lifetime may affect multiple instances: ask the SDK about
  thread/global semantics before parallel sessions.
- Per-file BSD-3-Clause-Clear and the tracker have their own source/license: keep notices
  when reusing and review proprietary SDK binary rights. No third-party code has been
  imported into the workspace.

## Usage direction

Updated by [ADR 0002](../adr/0002_qualcomm_plugin_backend.md): first implement a private
plugin-backed adapter on the user-confirmed QCS6490 / Qualcomm Linux 1.8 baseline; direct
SDK is optional later. The original direct-SDK preference below is historical, not a
blocker for coding.

Separate benchmark: use the vendor sample on the same model/input/board for cross-check. Do
not fork the whole plugin stack into core AI; do not add an encode/display pipeline just to
perform inference. C2D/GLES backends are capability-dependent optimizations, not defaults
replacing the chosen FastCV without review.

## Limits and next work

- No board compile/run or throughput/accuracy measurement; the review covers only the
  inspected source and the reviewed snapshot.
- The listed reuse checks remain open before copying any logic or linking vendor libraries.

## See also

- [Qualcomm plugin adapter reference](../architecture/qualcomm_plugin_adapter_reference.md)
- [ADR 0002 — Qualcomm plugin backend](../adr/0002_qualcomm_plugin_backend.md)
