# Retrieval

Compute/state: attributes/embedding + track entity refs + external search index.

- **Feature ID:** `retrieval`
- **Status:** matching policy contract delivered; processor, configuration schema and
- **Layer:** features
  entitlement are still pending
- **Naming registry:** `retr`
- **Acceptance focus:** search records/results; recall@k, latency, authorization

## Responsibility

- Integrate through the feature activation manager and feature processor registry; no rule runs here yet.
- Depend on the models, attributes and state declared in the feature catalog and model catalog.

## Limits and next work

- Requires a compiled-in factory registered for its `processor_contract` plus a bounded configuration schema.
- The neutral policy groups index candidates by opaque subject, applies configured similarity
  and cross-subject margin, and emits known/unknown/ambiguous. It does not own gallery
  persistence, temporal track state or authorization.
- No entitlement, configuration values, artifact paths or measured KPIs are defined here.

## See also

- [Feature catalog](../../../docs/architecture/feature_catalog.md)
- [Feature processor registry](../../../docs/architecture/feature_processor_registry.md)
