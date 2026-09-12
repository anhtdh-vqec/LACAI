# feature_manager

Desired/effective feature state, dependency reconciliation and entitlement-aware activation,
plus the serialized feature processor/stage boundary.

- **Status:** source-delivered manager, registry, stage and loader — concrete packages missing
- **Naming registry:** `ftmgr` (`ftmgr`, `ftreg`, `ftstg`, `famgr`, `ftcat`)
- **Depends on:** feature catalog, feature processor factory port, model catalog
- **Used by:** runtime composition / feature fan-out

## Responsibility

- Reconcile desired, entitlement, resource and model-dependency gates on the cold path.
- Own one processor plus stage per ready source/feature association with explicit effective states.
- Resolve a catalog `processor_contract` to a compiled-in factory and validate schema/revision-bound configuration.
- Validate one configured processor and publish only validated event batches.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_feature_activation_manager.cpp` | Cold-path reconciler; per-association processor/stage ownership; slot-to-catalog mapping |
| `vqec_vision_feature_processor_registry.cpp` | Bounded contract-to-factory registry returning distinct processor owners |
| `vqec_vision_feature_stage.cpp` | Serialized processor coordinator with monotonic time and epoch faults |
| `vqec_vision_feature_catalog.cpp` | Optional strict bounded feature-catalog JSON loader |

## Limits and next work

- Dynamic package loading, authenticated configuration and entitlement remain composition responsibilities.
- A missing compiled-in processor is reported as `unsupported`, never as an implicit entitlement grant.
- Association-level failures are isolated (`disabled`, `denied`, `unsupported`,
  `resource_limited`, `ready`, `faulted`); the first operational error returns to the caller.

## See also

- [Feature activation manager](../../../docs/architecture/feature_activation_manager.md)
- [Feature processor registry](../../../docs/architecture/feature_processor_registry.md), [feature stage](../../../docs/architecture/feature_stage.md)
- [Feature catalog](../../../docs/architecture/feature_catalog.md), [feature event contract](../../../docs/architecture/feature_event_contract.md)
