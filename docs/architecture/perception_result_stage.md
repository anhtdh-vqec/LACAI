# Perception result stage

`perception_result_stage` is the portable composition boundary from one completed model
result to one tracked observation batch. It binds a configured source identity/geometry,
one model decode stage and one tracker stage; no Qualcomm or FW transport type crosses
this boundary.

**Status:** source-delivered — portable composition stage source exists.
**Layer:** app. **Source:** `src/app/vqec_vision_perception_result_stage.{hpp,cpp}`.

## Responsibility

- Correlate one completed model result to exactly one source frame through its retained
  submission ticket.
- Compose one model-specific decoder and one tracker stage and publish their transactional
  results.
- Must not own tensor memory after return, run feature processors, authorize output or
  retain the RAW frame owner.

## Correlation and publication

The stage reconstructs `preview_frame_key` from activation-time camera/channel identity
and the source epoch/frame ID/PTS retained in the submitted job ticket. It requires the
tensor result's pipeline PTS to match that ticket exactly. It never derives a source frame
from pipeline PTS or current time.

After correlation, the model-specific decoder produces a validated detection batch and
the tracker produces a validated tracked batch with explicit monotonic time and source-gap
state. Both intermediate and final publications are transactional. A decoder exception is
converted to `io_error`; tracker failure follows the tracker stage's epoch-fault rules.

This stage is configured once and is serialized per model/source association. Multi-model
composition chooses the matching decoder/stage by the immutable model slot from
`multi_model_pump_report`.

## Limits and next work

- The stage does not own tensor memory after return, run feature processors, authorize
  output or retain the RAW frame owner.
- Multi-model composition must choose the matching decoder/stage by the immutable model
  slot; any other correlation is not supported by this boundary.

## See also

- [multi-model pump](multi_model_pump.md)
- [tracking stage](tracking_stage.md)
- [perception stage factory](perception_stage_factory.md)
