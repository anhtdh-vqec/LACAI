# ports

Vendor-neutral runtime interfaces used by application composition. No GStreamer, QNN,
FastCV, Camera Service wire or product-origin types may cross this directory.

- **Status:** source-delivered port surface
- **Naming registry:** `ports`
- **Implemented by:** `src/adapters/camera`, `src/adapters/qualcomm`, `src/adapters/reference`, `src/adapters/zvec`, `src/adapters/storage`, `src/adapters/fw_control`

## Responsibility

| Header | Port |
|---|---|
| `vqec_vision_raw_source.hpp` | One logical FW RAW-source lifecycle and a moveable metadata/native-handle/shared-owner envelope |
| `vqec_vision_inference_graph.hpp` | Neutral model-graph lifecycle, capability/policy and shared-frame or tensor submission |
| `vqec_vision_image_processor.hpp` | Turns a borrowed NV12 view into the exact model input tensor |
| `vqec_vision_image_alignment.hpp` | Landmark-based alignment/crop for secondary (cascade) models, capability-gated |
| `vqec_vision_cascade_frame_lease.hpp` | Session-owned retained-frame acquire/retire/complete lease for the cascade coordinator |
| `vqec_vision_tracker.hpp` | Serialized per-source tracking after model decoding |
| `vqec_vision_embedding_decoder.hpp` | Turns an aligned crop tensor into a typed, normalized embedding |
| `vqec_vision_embedding_index.hpp` | Revision-pinned gallery index operations (data types live in contracts) |
| `vqec_vision_face_gallery_store.hpp` | Authoritative protected gallery persistence |
| `vqec_vision_face_enrollment.hpp`, `vqec_vision_face_enrollment_image.hpp` | Enrollment control and bounded image source |
| `vqec_vision_face_image_inference.hpp` | Face detector/cascade boundary for image enrollment |
| `vqec_vision_image_path_authorizer.hpp` | POSIX enrollment image path authorization |
| `vqec_vision_usecase_control.hpp` | Usecase desired/effective-state control boundary |
| `vqec_vision_feature_processor.hpp` | Serialized per-source feature-algorithm boundary emitting bounded neutral events |
| `vqec_vision_feature_processor_factory.hpp` | Activation-time processor construction by contract |
| `vqec_vision_feature_event_sink.hpp` | Synchronous borrowed-event delivery; retry keeps the same event ID |

## Limits and next work

- Application code must not call plugin APIs directly; it depends on these ports only.
- Other platform adapters must preserve the same ownership and epoch semantics.
- The image-processor tensor path is wired into the pump; additional platform implementations remain.
- `image_alignment_port` is implemented by the Qualcomm FastCV aligner and consumed by the cascade coordinator; golden parity remains open.

## See also

- [RAW-source port](../../../../../docs/architecture/raw_source_port.md), [inference graph port](../../../../../docs/architecture/inference_graph_port.md)
