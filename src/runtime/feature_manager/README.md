# feature_manager

Desired/effective state, dependency reconciliation and entitlement-aware activation.
The neutral feature-event and processor contracts are source-delivered. The activation
manager now validates the three catalog authorities, evaluates desired/entitlement/
resource/model gates and owns one processor plus stage per ready source/feature
association.

`vqec_vision_feature_processor_registry` resolves a catalog `processor_contract` to a
compiled-in factory, validates a bounded schema/revision-bound configuration and returns
a distinct processor owner per source/usecase association. Dynamic package loading and
entitlement remain composition responsibilities.

`vqec_vision_feature_stage` validates one configured processor, handles strictly
increasing source epochs and publishes only validated event batches. Ambiguous processor
failure faults the current epoch and requires a later epoch for recovery.

`vqec_vision_feature_activation_manager` is a cold-path reconciler. Catalog and factory
references are borrowed immutable inputs; ready associations own their processor and
stage. Malformed request sets are rejected transactionally. Association-level failures
are isolated and represented by `disabled`, `denied`, `unsupported`,
`resource_limited`, `ready` or `faulted`, with the first operational error returned to
the caller. A missing compiled-in processor is reported as `unsupported`, never as an
implicit entitlement grant.
