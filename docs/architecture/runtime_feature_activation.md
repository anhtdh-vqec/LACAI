# Runtime feature activation boundary

Feature activation is composed after deployment/model admission and before the
serialized source executor starts. `feature_activation_manager` owns one processor
and `feature_stage` per ready `(source_id, feature_id)` association. The runtime
composition layer consumes the manager's immutable record and catalog mapping, then
binds each stage to the model slot named by its catalog dependency.

Only single-model dependencies may enter `multi_model_feature_pipeline`; a feature
depending on several models requires a separately specified bounded temporal join.
Disabled, denied, unsupported, resource-limited and faulted records create no fan-out
entry. Fan-outs and pipelines are owned by the runtime bundle, while stages remain
owned by the activation manager and must outlive those borrowed bindings.

The mapping is activation-time and numeric: source deployment order and
`source.model_ids_` order define slots. Per-frame code does not look up feature or
model names, allocate, resolve catalogs or re-evaluate entitlement. Output dispatch
still rechecks authorization from the event payload and policy revision.
