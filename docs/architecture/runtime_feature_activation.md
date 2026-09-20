# Runtime feature activation boundary

Feature activation is composed after usecase-to-model resolution and deployment/model
admission, and before the serialized source executor starts. `feature_activation_manager`
owns one processor and `feature_stage` per ready `(source_id, feature_id)` association.
The runtime composition layer consumes a frozen manager record and catalog mapping, then
binds each stage to the model slot named by its catalog dependency. A later compatible
activation delta constructs a complete candidate owner set and transactionally rebinds the
numeric fan-out pointers without replacing model, tracker or source owners.

**Status:** source-delivered — runtime composition binds feature stages to model slots and
owns the fan-out/pipeline objects. **Layer:** app.
**Source:** `src/app/composition/vqec_vision_runtime_composition_factory.hpp`,
`src/runtime/feature_manager/vqec_vision_feature_activation_manager.{hpp,cpp}`,
`src/runtime/feature_manager/vqec_vision_feature_stage.{hpp,cpp}`.

## Responsibility

- Consume the activation manager's immutable record and catalog mapping and bind each
  `(source_id, feature_id)` stage to the model slot named by its catalog dependency.
- Keep the mapping activation-time and numeric so per-frame code does not look up feature
  or model names, allocate, resolve catalogs or re-evaluate entitlement.
- Prevalidate every source pipeline before changing any live fan-out pointer. Unchanged
  feature associations adopt their existing processor and stage owners, preserving temporal
  state; changed configuration receives a new owner.
- Must not act as the commercial compute switch; FW selects usecases through the usecase
  activation contract.

## Activation mapping

Only single-model dependencies may enter `multi_model_feature_pipeline`; a feature
depending on several models requires a separately specified bounded temporal join.
Disabled, denied, unsupported, resource-limited and faulted records create no fan-out
entry. Fan-outs and pipelines are owned by the runtime bundle, while stages remain
owned by the activation manager and must outlive those borrowed bindings.

For a compatible delta, the service builds and freezes a candidate activation owner. It
then adopts only exact `(source, feature, usecase, model slot, catalog/deployment revision,
configuration revision, attribute scopes)` matches from the live owner. The runtime bundle
validates all candidate source bindings before replacing any pipeline array. The candidate
becomes the live lifetime owner only after that replacement succeeds.

The mapping is activation-time and numeric: source deployment order and
`source.model_ids_` order define slots. Per-frame code does not look up feature or
model names, allocate, resolve catalogs or re-evaluate entitlement. Output dispatch
still rechecks authorization from the event payload and policy revision.

## Relationship to usecase selection

This feature-stage boundary is not the commercial compute switch. FW selects usecases
through [the usecase activation contract](../contracts/fw_usecase_control.md). That
selection derives the effective root-model set before admission. This manager creates
stages only for effective usecases within the generation's prepared capacity.

## Limits and next work

- A feature depending on several models requires a separately specified bounded temporal
  join; only single-model dependencies may enter `multi_model_feature_pipeline`.
- Multi-model joins and a cross-process state handoff remain outside this boundary.

## See also

- [feature activation manager](feature_activation_manager.md)
- [multi-model feature pipeline](multi_model_feature_pipeline.md)
- [usecase activation](usecase_activation.md)
- [FW usecase activation](../contracts/fw_usecase_control.md)
