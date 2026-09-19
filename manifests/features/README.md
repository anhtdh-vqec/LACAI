# features

Feature IDs, dependencies, permissions, compatibility and resource requirements.

- **Status:** synthetic example only

| File | Purpose |
|---|---|
| `feature_catalog.example.json` | Synthetic input for the versioned neutral feature catalog contract |
| `feature_catalog.fire_smoke.example.json` | S04 product-slice dependency and processor contract fixture |

It demonstrates model-role and bounded processor requirements; it is not an active feature
grant, product configuration or qualification result. A trusted activation layer must still
select source/feature assignments and enforce entitlement.

The fire/smoke catalog uses the canonical `security.fire_smoke_detection` package's compiled
`fire_smoke_alarm` processor contract. It is product integration metadata, but the example file
is not itself an install, entitlement or admission receipt.

## See also

- [Feature catalog](../../docs/architecture/feature_catalog.md), [feature catalog schema](../../config/schemas/feature_catalog.schema.json)
