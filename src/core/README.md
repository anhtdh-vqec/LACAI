# core

Pure, header-visible validation and bookkeeping shared by every adapter and application
module. No I/O, no allocation of runtime pools and no vendor, GStreamer or OpenCV types.

- **Status:** source-delivered — unit/contract tests run under the eSDK QEMU configuration
- **Naming registry:** `core` (`dpval`, `infpl`, `subwn`, `srcbd`, `tnctr`, `inexe`, `otgat`, `ftevt`, `ftcat`, `otgen`, `encot`, `pvpol`, `encwn`, `pvsrf`, `pvctr`)
- **Depends on:** `include/vqec/vision/ai/contracts/`
- **Used by:** `src/app/`, `src/outputs/`, adapters through neutral contracts

## Responsibility

- Validate deployment/source/model/feature descriptors and packed NV12 byte counts.
- Own frame-correlated submission bookkeeping, relative PTS mapping and drain/fault state.
- Authorize outputs from revision-aware source/feature/attribute scopes.
- Validate preview overlay, encoded-AU and feature-event envelopes with bounded limits.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_deployment_config.cpp` | Checked global/per-source memory admission and transactional plan bind |
| `vqec_vision_inference_plan.cpp` | Pure typed plan validation and checked packed NV12 byte count |
| `vqec_vision_source_binding.cpp` | Explicit source geometry/color/memory-policy validation |
| `vqec_vision_tensor_contract.cpp` | Ordered packed output name/shape/byte validation |
| `vqec_vision_submission_window.cpp` | Fixed-capacity admission, PTS mapping, input/result completion, drain |
| `vqec_vision_output_gate.cpp` | Revision-aware source/feature/attribute authorization and invalidation |
| `vqec_vision_feature_event.cpp`, `vqec_vision_feature_catalog.cpp` | Neutral feature event and catalog validation |
| `vqec_vision_output_generation.cpp` | Nonzero monotonic output binding identities across sink rebuilds |
| `vqec_vision_encoded_output.cpp` | Immutable owned H264 output behind the neutral `encoded_sink` port |
| `vqec_vision_preview_pool.cpp`, `vqec_vision_preview_surface.cpp` | Bounded CPU surfaces with per-acquisition lease ownership |
| `vqec_vision_encoder_window.cpp`, `vqec_vision_encoder_contract.cpp` | Preview input admission and correlated encoder completion bookkeeping |
| `vqec_vision_preview_contract.cpp` | Overlay metadata and borrowed H264 AU envelope validation |
| `vqec_vision_inference_execution.cpp` | Inference capability/policy/domain/shared-buffer/model-update validation |

## Limits and next work

- Pool, encoder and submission code owns no image memory and cannot detect hardware completion.
- `output_generation` runtime wiring and executed tests remain pending.
- Trusted grant verification and serialized output dispatch integration stay outside the pure evaluator.
- No model-kit parser, signature verification or decoder is implied.

## See also

- [Multi-source configuration](../../docs/architecture/multi_source_configuration.md)
- [Submission window](../../docs/architecture/submission_window.md), [output generation](../../docs/architecture/output_generation.md)
- [Preview pool](../../docs/architecture/preview_pool.md), [encoder window](../../docs/architecture/encoder_window.md)
