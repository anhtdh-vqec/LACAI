# ports

Vendor-neutral runtime interfaces used by application composition. No GStreamer, QNN,
FastCV, Camera Service wire or product-origin types may cross this directory.

- **Status:** source-delivered port surface
- **Naming registry:** `ports`
- **Implemented by:** `src/adapters/camera`, `src/adapters/qualcomm`, `src/adapters/reference`

## Responsibility

| Header | Port |
|---|---|
| `vqec_vision_raw_source.hpp` | One logical FW RAW-source lifecycle and a moveable metadata/native-handle/shared-owner envelope |
| `vqec_vision_inference_graph.hpp` | Neutral model-graph lifecycle, capability/policy and shared-frame or tensor submission |
| `vqec_vision_image_processor.hpp` | Turns a borrowed NV12 view into the exact model input tensor |
| `vqec_vision_tracker.hpp` | Serialized per-source tracking after model decoding |
| `vqec_vision_feature_processor.hpp` | Serialized per-source feature-algorithm boundary emitting bounded neutral events |
| `vqec_vision_feature_event_sink.hpp` | Synchronous borrowed-event delivery; retry keeps the same event ID |

## Limits and next work

- Application code must not call plugin APIs directly; it depends on these ports only.
- Other platform adapters must preserve the same ownership and epoch semantics.
- The image-processor tensor path is wired into the pump; additional platform implementations remain.

## See also

- [RAW-source port](../../../../../docs/architecture/raw_source_port.md), [inference graph port](../../../../../docs/architecture/inference_graph_port.md)
