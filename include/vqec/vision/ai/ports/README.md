# ports

Vendor-neutral runtime interfaces used by application composition. No GStreamer, QNN,
FastCV, Camera Service wire or product-origin types may cross this directory.

- **Status:** source-delivered port surface
- **Layer:** contracts
- **Naming registry:** `ports`
- **Implemented by:** `src/adapters/camera`, `src/adapters/qualcomm`,
  `src/adapters/reference`, `src/adapters/zvec`, `src/adapters/storage`,
  `src/adapters/fw_control`

## Responsibility

| Domain | Header | Port |
|---|---|---|
| `inference/` | `vqec_vision_raw_source.hpp` | One logical RAW-source lifecycle and moveable frame-owner envelope. |
| `inference/` | `vqec_vision_inference_graph.hpp` | Neutral model-graph lifecycle and frame/tensor submission. |
| `inference/` | `vqec_vision_image_processor.hpp` | Borrowed NV12 view to exact model input tensor. |
| `inference/` | `vqec_vision_image_alignment.hpp` | Capability-gated alignment/crop for cascade models. |
| `media/` | `vqec_vision_cascade_frame_lease.hpp` | Session-owned retained-frame lease. |
| `perception/` | `vqec_vision_tracker.hpp` | Serialized per-source tracking after decode. |
| `perception/` | `vqec_vision_embedding_decoder.hpp` | Aligned crop tensor to normalized embedding. |
| `perception/` | `vqec_vision_embedding_index.hpp` | Revision-pinned gallery index operations. |
| `perception/` | `vqec_vision_face_gallery_store.hpp` | Authoritative gallery persistence. |
| `perception/` | `vqec_vision_face_enrollment.hpp`, `vqec_vision_face_enrollment_image.hpp` | Enrollment control and bounded image source. |
| `perception/` | `vqec_vision_face_image_inference.hpp` | Face detector/cascade image boundary. |
| `perception/` | `vqec_vision_image_path_authorizer.hpp` | Enrollment image path authorization. |
| `management/` | `vqec_vision_usecase_control.hpp` | Desired/effective usecase-state control. |
| `management/` | `vqec_vision_app_*.hpp` | App inventory, package, entitlement, configuration and manager boundaries. |
| `features/` | `vqec_vision_feature_processor.hpp` | Serialized feature algorithm emitting bounded events. |
| `features/` | `vqec_vision_feature_processor_factory.hpp` | Activation-time processor construction. |
| `features/` | `vqec_vision_feature_event_sink.hpp` | Borrowed-event delivery with stable retry identity. |
| `output/` | `vqec_vision_evidence_*.hpp` | Durable evidence outbox and transport boundaries. |

## Contents

| Path | Purpose |
|---|---|
| `management/` | Install, entitlement, configuration and runtime usecase control. |
| `inference/` | Source acquisition, preprocessing and inference graph execution. |
| `perception/` | Tracking, embedding, gallery and enrollment capabilities. |
| `features/` | Usecase processor construction and event delivery. |
| `output/` | Durable evidence handoff. |
| `media/` | Retained media/frame lifetime capabilities. |

## Limits and next work

- Application code must not call plugin APIs directly; it depends on these ports only.
- Other platform adapters must preserve the same ownership and epoch semantics.
- The image-processor tensor path is wired into the pump; additional platform implementations remain.
- `image_alignment_port` is implemented by the Qualcomm FastCV aligner and consumed by the cascade coordinator; golden parity remains open.

## See also

- [RAW-source port](../../../../../docs/architecture/raw_source_port.md), [inference graph port](../../../../../docs/architecture/inference_graph_port.md)
