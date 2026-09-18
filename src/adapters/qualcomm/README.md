# qualcomm

Private Qualcomm adapter for QCS6490 / Qualcomm Linux 1.8. Contains the released plugin
graph backend, the optional LACAI-owned QNN engine and the private preview renderer.

- **Status:** board-smoke — synchronous plugin/QNN evidence exists on QCS6490 and the
  2026-09-18 layout candidate passes natively; async/shared/update remain unqualified
- **Layer:** adapters
- **Naming registry:** `qcom` (`plgr`, `ifgr`, `dmbrg`, `tnout`, `frsub`, `qneng`, `qnig`,
  `bfact`, `sdkld`, `qtvr`, `d1cli`, `d1ovr`)
- **Depends on:** neutral `inference_graph_port`, core plan/contract validation
- **Used by:** application composition through `inference_graph_port` only

## Responsibility

- Provide a caller-supplied plugin graph with explicit factory/property validation.
- Convert a native handle to a Linux FD exactly once and retain the memory owner on root memory.
- Extract ordered typed tensors (`INT8..FLOAT32`, quantized-blob ownership) from results.
- Provide an owned QNN engine: dlopen, backend/device, capability probe, context + model-lib
  compose, typed tensor metadata, synchronous execute and a graph-port binding.
- Allocate bounded rpcmem output surfaces, compose authorized NV12 overlays through the
  versioned cDSP operation, import them directly into `v4l2h264enc` and publish H.264 access
  units to the released FW ring without `qtivoverlay`.

## Contents

| Path | Purpose |
|---|---|
| `dsp/host/` | ARM-side legacy/v1 FastRPC sessions, negotiated generic client, rpcmem, mapping cache and neutral port adapters |
| `dsp/v1/` | LACAI v1 IDL, wire codec, generic dense/overlay operations, bounded service and QAIC skeleton |
| `dsp/legacy/` | Frozen model-specific compatibility ABI and reviewed reference kernels |
| `gstreamer/` | Plugin graph, DMA-BUF wrapping, submission, tensor extraction and FastCV preprocess |
| `media/` | Enrollment image source, affine/color conversion and preview/encode renderer |
| `qnn/` | Owned QNN loader, engine, graph adapter and backend factory |
| `CMakeLists.txt` | Keeps vendor include/link requirements private to the adapter target |

## Limits and next work

- The production service wires this renderer for `--platform qualcomm`; output pool,
  bitrate, GOP, color and caps metadata are required runtime configuration.
- Plugin reports FLOAT32 outputs; native multi-dtype/multi-graph QNN and a batch/temporal/ROI scheduler are missing.
- Owned QNN engine currently executes synchronously with copies; async, shared/registered
  memory and LoRA have contracts but are not wired into execute.
- Registered DMA-BUF input is retained directly through synchronous cDSP compose. A memfd
  compatibility source uses one explicit bounded rpcmem staging copy; the shared source frame
  is never modified in place. Released-FW cache/fence/completion still needs owner acceptance.
- Golden preprocessing, long-run performance and BSP recovery remain unverified.
  Successful graph assembly is not inference qualification.

## See also

- [FastRPC leases and generic protocol requirements](../../../docs/architecture/qualcomm_fastrpc_adapter.md)
- [Qualcomm adapter](../../../docs/architecture/qualcomm_adapter.md), [plugin adapter reference](../../../docs/architecture/qualcomm_plugin_adapter_reference.md)
- [Submission lifecycle](../../../docs/architecture/qualcomm_submission_lifecycle.md), [dmabuf memory bridge](../../../docs/architecture/dmabuf_memory_bridge.md)
- [Owned QNN engine ADR](../../../docs/adr/0003_owned_qnn_engine.md), [execution policy](../../../docs/architecture/qualcomm_execution_policy.md)
- [QNN board validation](../../../docs/testing/qnn_board_validation.md)
- [Repository source layout](../../../docs/development/source_layout.md)
