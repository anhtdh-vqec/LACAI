# Core

The deployment validator provides checked global/per-source memory admission and a
transactional bind from the deployment-owned source profile into a model-owned inference
plan. It allocates no pools and proves no board workload. See
docs/architecture/multi_source_configuration.md.

vqec_vision_output_generation.cpp issues nonzero monotonic output binding identities
across sink rebuilds without reset/wrap. One serialized runtime owner must share the
allocator; it is not a global singleton or cross-process identity service. See
docs/architecture/output_generation.md. Runtime wiring and executed tests remain pending.

vqec_vision_preview_pool.cpp preallocates bounded CPU surfaces with per-acquisition
lease ownership; only final reader release allows reuse. No hardware import/rendering
or encoder integration. See docs/architecture/preview_pool.md; tests are source-only.

vqec_vision_encoder_window.cpp adds bounded preview input admission and correlated
encoder completion bookkeeping using submission_window. It owns no image memory and
cannot detect hardware completion. See docs/architecture/encoder_window.md.

vqec_vision_preview_contract.cpp validates neutral overlay metadata and borrowed H264
AU envelopes against exact frame/geometry, freshness/revision and bounded byte limits.
No rendering, allocation, authorization decision or H264 syntax decoding is implied.
See docs/architecture/preview_contract.md. Source-only unit tests run through the approved eSDK configuration; device evidence remains separate.

Implemented source: vqec_vision_inference_plan.cpp provides pure typed plan validation and
checked packed NV12 byte count for the initial Qualcomm graph slice.
No GStreamer/vendor/OpenCV dependency. No I/O or model loading in plan validation.

vqec_vision_submission_window.cpp adds fixed-capacity admission bookkeeping, relative PTS mapping,
and explicit input/result completion plus drain/fault state. It owns no frame resources
and is now used by the private Qualcomm graph without introducing vendor types into
core. See docs/architecture/submission_window.md. Hardware validation remains pending.

source_binding validates explicit source geometry/color/memory-policy metadata.
tensor_contract validates ordered packed FLOAT32 output names/shapes and checked
total bytes. Both camera_session preflight and plugin_graph use the same output
validator. No model-kit file parser, signature verification or decoder is implied.

output_gate adds revision-aware source/feature/attribute authorization, validity checks
and explicit invalidation without touching graph resources. Trusted grant verification
and serialized output dispatch integration remain outside this pure evaluator.
