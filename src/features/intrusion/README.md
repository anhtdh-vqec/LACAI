# Intrusion

Compute/state: person/object tracks + polygon/line rules.

- **Feature ID:** `intrusion`
- **Status:** planned — no processor, configuration schema, tests or entitlement yet
- **Naming registry:** `intr`
- **Acceptance focus:** entry/exit/intrusion event; geometry, duplicate suppression

## Responsibility

- Integrate through the feature activation manager and feature processor registry; no rule runs here yet.
- Depend on the models, attributes and state declared in the feature catalog and model catalog.

## Limits and next work

- Requires a compiled-in factory registered for its `processor_contract` plus a bounded configuration schema.
- No entitlement, configuration values, artifact paths or measured KPIs are defined here.

## See also

- [Feature catalog](../../../docs/architecture/feature_catalog.md)
- [Feature processor registry](../../../docs/architecture/feature_processor_registry.md)
