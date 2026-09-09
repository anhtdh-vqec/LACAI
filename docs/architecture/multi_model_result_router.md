# Multi-model result router

Status: portable source and contract test cross-compiled for the eSDK target; target test
binary is not executed on the x86 development host.

`multi_model_result_router` is the portable serialized boundary between one
`multi_model_pump` and the decode/tracking stage assigned to each model. It uses the
immutable numeric model slot established at activation; model names and vendor types do
not enter the frame path.

The router binds 1..16 distinct, already-configured `perception_result_stage` owners for
one camera, channel and source geometry. Configuration fails when any stage has a
different source identity or geometry. It borrows the stages for its full lifetime.

For each pump result, the router requires `has_result`, a valid result slot, no reported
error slot and the exact retained submission ticket. It sends the tensor result only to
the matching stage. Each slot independently tracks the last source epoch, frame ID and
source PTS. Within one epoch, IDs and PTS must increase; a nonconsecutive frame ID becomes
an explicit tracker gap. The first result in a new epoch is not marked as a gap because
the tracking stage resets that epoch before updating tracks.

Publication is transactional. A failed correlation, decoder or tracker call does not
overwrite that slot's prior batch and does not advance its progress watermark. Results
from other slots remain untouched. One call processes at most one pump result and uses
only fixed-capacity router storage, although concrete decoder/tracker implementations and
observation payloads may allocate.

This boundary does not fuse outputs from multiple models. A feature requiring multiple
model outputs needs a separate bounded temporal join keyed by camera, channel, source
epoch and frame policy; pump round-robin delivery does not imply that adjacent model
results describe the same frame.

See [multi-model pump](multi_model_pump.md),
[perception result stage](perception_result_stage.md) and
[tracking stage](tracking_stage.md). Direct feature consumers are composed by the
[multi-model feature pipeline](multi_model_feature_pipeline.md).
