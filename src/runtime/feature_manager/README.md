# feature_manager

Desired/effective state, dependency reconciliation and entitlement-aware activation.
The neutral feature-event and processor contracts are source-delivered; reconciliation,
admission ownership and full runtime state transitions remain unimplemented.

`vqec_vision_feature_stage` validates one configured processor, handles strictly
increasing source epochs and publishes only validated event batches. Ambiguous processor
failure faults the current epoch and requires a later epoch for recovery.
