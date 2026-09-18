# Multi-model feature pipeline

`multi_model_feature_pipeline` composes the portable result router with feature fan-out so
each model slot can drive its own feature stages. This document defines the slot binding,
the result-to-feature delivery and the failure isolation.

**Status:** logic-tested — portable source and contract test built with the eSDK; it runs
under eSDK/QEMU and natively on QCS6490 `.98`. **Layer:** app. **Source:**
`src/app/pipeline/vqec_vision_multi_model_feature_pipeline.cpp`,
`tests/contract/application/vqec_vision_multi_model_feature_pipeline_test.cpp`.

## Responsibility

- Binds one optional `feature_fanout` owner to each immutable model slot.
- Requires the model count to exactly match the already-configured result router.
- Keeps model and feature names out of the per-frame selection path; activation
  composition owns the mapping.
- Does not authorize or deliver events; those remain separate output boundaries.

## Slot binding and delivery

A model may have no direct feature consumers, while one configured fan-out may belong to
only one model because its feature stages contain temporal state.

Each accepted pump result is first correlated, decoded and tracked in its model slot. Only
that tracked batch and its derived source-gap flag reach the fan-out bound to the same
slot.

## Failure isolation

A decode/tracking failure produces no feature call. A feature failure does not roll back
the valid tracked batch: the report retains result success plus the fan-out's per-feature
processed/failed masks, and healthy feature stages may already have published candidate
events. Callers must inspect `has_feature_fanout` and the fan-out report before consuming
the supplied event slots.

## Limits and next work

- This pipeline supports features whose observation dependency is one model.
- It does not feed one feature from several model outputs or infer frame alignment from
  result delivery order.
- A multi-output feature requires an explicitly configured, bounded temporal join keyed by
  source identity and a documented freshness policy.

## See also

- [Multi-model result router](multi_model_result_router.md)
- [Feature fan-out](feature_fanout.md)
- [Feature event dispatch](feature_event_dispatch.md)
