# Perception result stage

`perception_result_stage` is the portable composition boundary from one completed model
result to one tracked observation batch. It binds a configured source identity/geometry,
one model decode stage and one tracker stage; no Qualcomm or FW transport type crosses
this boundary.

The stage reconstructs `preview_frame_key` from activation-time camera/channel identity
and the source epoch/frame ID/PTS retained in the submitted job ticket. It requires the
tensor result's pipeline PTS to match that ticket exactly. It never derives a source frame
from pipeline PTS or current time.

After correlation, the model-specific decoder produces a validated detection batch and
the tracker produces a validated tracked batch with explicit monotonic time and source-gap
state. Both intermediate and final publications are transactional. A decoder exception is
converted to `io_error`; tracker failure follows the tracker stage's epoch-fault rules.

This stage is configured once and is serialized per model/source association. It does not
own tensor memory after return, run feature processors, authorize output or retain the RAW
frame owner. Multi-model composition chooses the matching decoder/stage by the immutable
model slot from `multi_model_pump_report`.
