# qualcomm

Private Qualcomm adapter for QCS6490 / Qualcomm Linux 1.8. Contains the released plugin
graph backend, the optional LACAI-owned QNN engine and the private preview renderer.

- **Status:** board-verified (sync) — plugin lifecycle fixtures pass on QCS6490; owned QNN engine composes/finalizes/executes SCRFD+YOLOv8n on HTP with byte-identical parity to `qnn-net-run`; async/shared/update still unqualified
- **Layer:** adapters
- **Naming registry:** `qcom` (`plgr`, `ifgr`, `dmbrg`, `tnout`, `frsub`, `qneng`, `qnig`, `bfact`, `sdkld`, `qtvr`)
- **Depends on:** neutral `inference_graph_port`, core plan/contract validation
- **Used by:** application composition through `inference_graph_port` only

## Responsibility

- Provide a caller-supplied plugin graph with explicit factory/property validation.
- Convert a native handle to a Linux FD exactly once and retain the memory owner on root memory.
- Extract ordered typed tensors (`INT8..FLOAT32`, quantized-blob ownership) from results.
- Provide an owned QNN engine: dlopen, backend/device, capability probe, context + model-lib
  compose, typed tensor metadata, synchronous execute and a graph-port binding.
- Allocate QTI DMA output surfaces, copy NV12 by plane stride, render ROI metadata with
  `qtivoverlay`, encode H.264 and publish access units to the released FW ring.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_plugin_graph.cpp` | Factory/property/enum probing, READY bind, PLAYING, bounded submission and result polling |
| `vqec_vision_inference_graph.cpp` | Neutral `inference_graph_port` adapter; owns the retention reference |
| `vqec_vision_frame_submission.cpp` | Reserve/wrap/commit/push primitive; one outstanding job per graph |
| `vqec_vision_dmabuf_bridge.cpp` | Read-only FD memory, original plane layout, root-memory owner retention |
| `vqec_vision_tensor_output.cpp` | Bounded ordered tensor extraction (typed) and shape checks |
| `vqec_vision_sdk_loader.cpp` | Optional QNN runtime `dlopen` loader with pinned QAIRT |
| `vqec_vision_qnn_engine.cpp` | Owned QNN backend/device/context/model-lib compose + sync execute + probe |
| `vqec_vision_qnn_inference_graph.cpp` | `inference_graph_port` binding with tensor submission |
| `vqec_vision_backend_factory.cpp` | Builds the owned engine+graph bundle from resolved paths; fails closed |
| `vqec_vision_qtiv_renderer.cpp` | QTI DMA pool, NV12 plane copy, ROI overlay, H.264 encoder and FW ring writer |
| `vqec_vision_fastcv_processor.cpp` | `qtivtransform`+`qtimlvconverter(engine=fcv)` NV12-to-tensor preprocessing |
| `vqec_vision_fastcv_aligner.cpp` | FastCV affine/color alignment behind `image_alignment_port` |
| `vqec_vision_qtiv_color.cpp` | QTI color conversion helper |
| `vqec_vision_dsp_buffer_cache.cpp` | Bounded mapping cache with pinned access leases and deferred retirement |
| `vqec_vision_dsp_legacy.idl` | Frozen compatibility wire; QAIC generates client stub at build time from the reviewed SDK |
| `vqec_vision_dsp_v1.idl`, `vqec_vision_dsp_v1_wire.{c,h}` | Proposed model-independent transport and tested envelope codec; no callable kernel or runtime selection |
| `vqec_vision_dsp_legacy_*.{c,h}` | Isolated legacy reference fixtures and wire constants; owner/license review pending |
| `vqec_vision_dsp_session.cpp` | Legacy FastRPC transport; model-specific wire operations, not a generic DSP ABI |
| `vqec_vision_dsp_preprocessor.cpp` | Legacy DSP image-processor implementation; semantic qualification remains open |
| `vqec_vision_dsp_decoder.cpp` | Legacy DSP decoder binding; fixed kernel envelopes |
| `vqec_vision_face_enrollment_image_source.cpp` | GStreamer JPEG-to-NV12 DMA-BUF image source for enrollment |

## Limits and next work

- The production service wires this renderer for `--platform qualcomm`; output pool,
  bitrate, GOP, color and caps metadata are required runtime configuration.
- Plugin reports FLOAT32 outputs; native multi-dtype/multi-graph QNN and a batch/temporal/ROI scheduler are missing.
- Owned QNN engine currently executes synchronously with copies; async, shared/registered
  memory and LoRA have contracts but are not wired into execute.
- The current camera harness supplies memfd, so the renderer performs one CPU plane copy
  into its writable QTI DMA surface. Direct released-FW DMA import still needs ownership
  design and measurement; the shared source frame is never modified in place.
- Golden preprocessing, long-run performance and BSP recovery remain unverified.
  Successful graph assembly is not inference qualification.

## See also

- [FastRPC leases and generic protocol requirements](../../../docs/architecture/qualcomm_fastrpc_adapter.md)
- [Qualcomm adapter](../../../docs/architecture/qualcomm_adapter.md), [plugin adapter reference](../../../docs/architecture/qualcomm_plugin_adapter_reference.md)
- [Submission lifecycle](../../../docs/architecture/qualcomm_submission_lifecycle.md), [dmabuf memory bridge](../../../docs/architecture/dmabuf_memory_bridge.md)
- [Owned QNN engine ADR](../../../docs/adr/0003_owned_qnn_engine.md), [execution policy](../../../docs/architecture/qualcomm_execution_policy.md)
- [QNN board validation](../../../docs/testing/qnn_board_validation.md)
