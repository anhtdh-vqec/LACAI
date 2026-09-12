# qualcomm

Private Qualcomm adapter for QSC6490 / Qualcomm Linux 1.8. Contains the released plugin
graph backend and the optional LACAI-owned QNN engine, both behind neutral ports.

- **Status:** source-delivered — plugin lifecycle fixtures pass on QCS6490; owned QNN engine not board-qualified
- **Naming registry:** `qcom` (`plgr`, `ifgr`, `dmbrg`, `tnout`, `frsub`, `qneng`, `qnig`, `bfact`, `sdkld`)
- **Depends on:** neutral `inference_graph_port`, core plan/contract validation
- **Used by:** application composition through `inference_graph_port` only

## Responsibility

- Provide a caller-supplied plugin graph with explicit factory/property validation.
- Convert a native handle to a Linux FD exactly once and retain the memory owner on root memory.
- Extract ordered typed tensors (`INT8..FLOAT32`, quantized-blob ownership) from results.
- Provide an owned QNN engine: dlopen, backend/device, capability probe, context + model-lib
  compose, typed tensor metadata, synchronous execute and a graph-port binding.

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

## Limits and next work

- No runnable service, concrete decoder/tracker, preview renderer/encoder or integrated FW ring runtime here.
- Plugin reports FLOAT32 outputs; native multi-dtype/multi-graph QNN and a batch/temporal/ROI scheduler are missing.
- Owned QNN engine currently executes synchronously with copies; async, shared/registered
  memory and LoRA have contracts but are not wired into execute.
- Golden preprocessing, SDK interoperability, performance and BSP recovery remain unverified.
  Successful graph assembly is not inference qualification.

## See also

- [Qualcomm adapter](../../../docs/architecture/qualcomm_adapter.md), [plugin adapter reference](../../../docs/architecture/qualcomm_plugin_adapter_reference.md)
- [Submission lifecycle](../../../docs/architecture/qualcomm_submission_lifecycle.md), [dmabuf memory bridge](../../../docs/architecture/dmabuf_memory_bridge.md)
- [Owned QNN engine ADR](../../../docs/adr/0003_owned_qnn_engine.md), [execution policy](../../../docs/architecture/qualcomm_execution_policy.md)
- [QNN board validation](../../../docs/testing/qnn_board_validation.md)
