# reference

Device-free synthetic backend implementing the neutral ports for contract, wiring and
scheduler tests. Proves lifecycle and composition only, never hardware correctness.

- **Status:** implemented — device-free tests run under the eSDK QEMU configuration
- **Naming registry:** `refer` (`rfsrc`, `rfgph`, `rfsnk`, `rfprc`)
- **Depends on:** neutral `raw_source_port`, `inference_graph_port`, `encoded_sink`, `image_processor_port`
- **Used by:** runtime executor harness and contract tests

## Responsibility

- Supply a deterministic NV12 source and a zero-tensor graph that exercise the complete graph lifecycle.
- Provide a reference event sink and a CPU image-processor baseline (letterbox, BT.601 limited
  RGB, normalize, quantize into the target dtype/quantization).
- Run without OpenCV, GStreamer, QNN or a board.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_reference_source.cpp` | Deterministic synthetic NV12 source implementing `raw_source_port` |
| `vqec_vision_reference_graph.cpp` | Zero-tensor graph implementing the full `inference_graph_port` lifecycle |
| `vqec_vision_reference_sink.cpp` | Synchronous reference event/encoded sink |
| `vqec_vision_reference_processor.cpp` | CPU NV12-to-tensor image-processor baseline |

## Limits and next work

- No board, model, accuracy, zero-copy or DMA-completion claim.
- Preprocess colorimetry must come from the source binding when used in production.

## See also

- [Image processor port](../../../docs/architecture/tensor_output.md)
- [Runtime executor](../../../docs/architecture/runtime_executor.md)
