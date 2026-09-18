# Multi-model result router

`multi_model_result_router` routes each pump result to the decode/tracking stage assigned to
its numeric model slot. This document defines its configuration identity checks, per-slot
progress tracking and transactional publication.

**Status:** logic-tested — portable source and contract test built with the eSDK; it runs
under eSDK/QEMU and natively on QCS6490 `.98`. **Layer:** app. **Source:**
`src/app/pipeline/vqec_vision_multi_model_result_router.cpp`,
`tests/contract/application/vqec_vision_multi_model_result_router_test.cpp`.

## Responsibility

- Is the portable serialized boundary between one `multi_model_pump` and the
  decode/tracking stage assigned to each model.
- Uses the immutable numeric model slot established at activation; model names and vendor
  types do not enter the frame path.
- Binds 1..16 distinct, already-configured `perception_result_stage` owners for one camera,
  channel and source geometry, and borrows the stages for its full lifetime.
- Does not fuse outputs from multiple models.

## Routing and progress

Configuration fails when any stage has a different source identity or geometry. For each
pump result, the router requires `has_result`, a valid result slot, no reported error slot
and the exact retained submission ticket. It sends the tensor result only to the matching
stage.

Each slot independently tracks the last source epoch, frame ID and source PTS. Within one
epoch, IDs and PTS must increase; a nonconsecutive frame ID becomes an explicit tracker
gap. The first result in a new epoch is not marked as a gap because the tracking stage
resets that epoch before updating tracks.

## Transactional publication

Publication is transactional. A failed correlation, decoder or tracker call does not
overwrite that slot's prior batch and does not advance its progress watermark. Results
from other slots remain untouched. One call processes at most one pump result and uses
only fixed-capacity router storage, although concrete decoder/tracker implementations and
observation payloads may allocate.

## Limits and next work

- This boundary does not fuse outputs from multiple models.
- A feature requiring multiple model outputs needs a separate bounded temporal join keyed
  by camera, channel, source epoch and frame policy.
- Pump round-robin delivery does not imply that adjacent model results describe the same
  frame.
- Concrete decoder/tracker implementations and observation payloads may still allocate.

## See also

- [Multi-model pump](multi_model_pump.md)
- [Perception result stage](perception_result_stage.md)
- [Tracking stage](tracking_stage.md)
- [Multi-model feature pipeline](multi_model_feature_pipeline.md)
