# Feature event contract

`feature_event_batch` is the neutral boundary between feature algorithms and eventual
event/output routing. It contains no Qualcomm, model-specific or FW wire types. Stable
`source_id`, `feature_id`, event schema/version and event ID values come from trusted
activation data and the concrete feature implementation; the contract has no built-in
list of commercial features.

Each event binds the exact source epoch/frame/geometry, configuration revision, source
clock occurrence time, optional model-version provenance, unique nonzero track references
and schema-versioned fields. The optional evidence request is only a correlation ID; it
does not authorize capture or transfer image/video ownership. Field schema IDs are the
actual attribute scopes an output router must present to `output_gate`.

Activation selects per-processor event, track-reference and field ceilings inside global
AI safety maxima. Empty event batches are valid. Vectors must be reserved during
activation and reused by the serialized per-source owner; the ceilings do not permit
unbounded per-frame allocation. Validation rejects duplicate event IDs, track references,
model versions or field schema/version pairs and preserves the caller's output contract.

`feature_processor_port` is the portable algorithm boundary. It validates one activation,
resets temporal state on a nonzero source epoch and consumes only validated tracked
observations with explicit monotonic time and source-gap state. A successful call returns
a batch that passes this contract; any failure must leave its output argument unchanged.
The port retains no observation references after return.

The feature manager must establish installed, entitled, desired, supported, admitted and
running state before calling a processor. This boundary performs no license decision,
event delivery, dedup persistence, evidence capture or model selection. Concrete feature
implementations still need algorithm-specific configuration, replay/golden tests and
measured resource admission.
