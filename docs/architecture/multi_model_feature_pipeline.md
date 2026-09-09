# Multi-model feature pipeline

Status: portable source and contract test cross-compiled for the eSDK target; target test
binary is not executed on the x86 development host.

`multi_model_feature_pipeline` composes the portable result router with feature fan-out.
It binds one optional `feature_fanout` owner to each immutable model slot. A model may
have no direct feature consumers, while one configured fan-out may belong to only one
model because its feature stages contain temporal state.

The model count must exactly match the already-configured result router. Each accepted
pump result is first correlated, decoded and tracked in its model slot. Only that tracked
batch and its derived source-gap flag reach the fan-out bound to the same slot. Model and
feature names are absent from the per-frame selection path; activation composition owns
the mapping.

A decode/tracking failure produces no feature call. A feature failure does not roll back
the valid tracked batch: the report retains result success plus the fan-out's per-feature
processed/failed masks, and healthy feature stages may already have published candidate
events. Callers must inspect `has_feature_fanout` and the fan-out report before consuming
the supplied event slots. Event authorization and delivery remain separate output
boundaries.

This pipeline supports features whose observation dependency is one model. It does not
feed one feature from several model outputs or infer frame alignment from result delivery
order. Such a feature requires an explicitly configured, bounded temporal join keyed by
source identity and documented freshness policy.

See [multi-model result router](multi_model_result_router.md),
[feature fan-out](feature_fanout.md) and
[feature event dispatch](feature_event_dispatch.md).
