# runtime

Portable runtime mechanisms that turn validated configuration into admitted, owned and
recoverable execution state without depending on a hardware vendor.

- **Status:** source-delivered — owner modules compile and are covered by eSDK tests
- **Layer:** runtime
- **Naming registry:** `admit`, `ftmgr`, `graph`, `life`, `mreg`, `sched`
- **Depends on:** contracts, ports and `src/core/`
- **Used by:** application composition and feature/perception activation

## Responsibility

- Load/resolve metadata, admit resources and manage lifecycle state.
- Construct portable graph/feature scheduling state from validated contracts.
- Keep external SDK and transport details behind adapter ports.

## Contents

| Path | Purpose |
|---|---|
| `admission/` | Hardware/resource profile evaluation |
| `feature_manager/` | Desired/effective usecase and feature-stage ownership |
| `graph/` | Portable graph worker and cadence execution |
| `lifecycle/` | Recovery and lifecycle invariants |
| `model_registry/` | Catalog/package/manifest/artifact resolution |
| `scheduler/` | Bounded inference scheduling |

## Limits and next work

- Runtime admission is not board performance acceptance.

## See also

- [Repository source layout](../../docs/development/source_layout.md)
