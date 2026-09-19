# Feature event contract

`feature_event_batch` is the neutral boundary between feature algorithms and eventual
event/output routing. This document defines the event fields, their validation rules and
the portable `feature_processor_port` algorithm boundary.

**Status:** source-delivered — the neutral contract and port exist with unit tests.
**Layer:** contracts. **Source:**
`include/vqec/vision/ai/contracts/features/vqec_vision_feature_event.hpp`,
`include/vqec/vision/ai/ports/features/vqec_vision_feature_processor.hpp`,
`src/core/features/vqec_vision_feature_event.cpp`, `tests/unit/core/vqec_vision_feature_event_test.cpp`.

## Responsibility

- Provides a neutral event/output boundary that contains no Qualcomm, model-specific or FW
  wire types.
- Takes stable `source_id`, `feature_id`, event schema/version and event ID values from
  trusted activation data and the concrete feature implementation; it has no built-in list
  of commercial features.
- Requires the feature manager to establish installed, entitled, desired, supported,
  admitted and running state before calling a processor.
- Performs no license decision, event delivery, dedup persistence, evidence capture or
  model selection.

## Event batch contract

Each event binds the exact source epoch/frame/geometry, configuration revision, source
clock occurrence time, optional model-version provenance, unique nonzero track references
and schema-versioned fields. The optional evidence request is only a correlation ID; it
does not authorize capture or transfer image/video ownership. Field schema IDs are the
actual attribute scopes an output router must present to `output_gate`.

Activation selects per-processor event, track-reference and field ceilings inside global
AI safety maxima. Empty event batches are valid. Vectors must be reserved during
activation and reused by the serialized per-source owner; the ceilings do not permit
unbounded per-frame allocation. Validation rejects duplicate event IDs, track references,
model versions or field schema IDs and preserves the caller's output contract. One event
cannot contain two versions of the same field because output entitlement is keyed by its
schema ID.

Episode events also carry a stable `event_id`, monotonically contiguous `episode_revision`,
`supersedes_episode_revision`, and the original source-clock `episode_begin_ns`. Open and
snapshot events start at revision 1 and supersede 0. Update and close events reuse the ID and
advance exactly one revision. This makes retry, correction and rollup projection deterministic;
an event delivery transport must not invent a new ID for a lifecycle update.

## Processor port

`feature_processor_port` is the portable algorithm boundary. It validates one activation,
resets temporal state on a nonzero source epoch and consumes only validated tracked
observations with explicit monotonic time and source-gap state. A successful call returns
a batch that passes this contract; any failure must leave its output argument unchanged.
The port retains no observation references after return.

## Limits and next work

- Concrete feature implementations still need algorithm-specific configuration,
  replay/golden tests and measured resource admission.
- This boundary performs no license decision, event delivery, dedup persistence, evidence
  capture or model selection.

## See also

- [Feature event dispatch](feature_event_dispatch.md)
- [Feature stage](feature_stage.md)
- [Feature fan-out](feature_fanout.md)
- [Output gate](output_gate.md)
