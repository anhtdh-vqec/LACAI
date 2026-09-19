# schemas

Configuration and control validation schemas with bounded fields and version/revision semantics.

- **Status:** source-delivered schemas; runtime acceptance is authoritative

## Contents

| Schema | Purpose |
|---|---|
| `deployment.schema.json` | Reviewable companion for the bounded C++ deployment loader (now with an optional per-source `cascade` retention budget) |
| `model_catalog.schema.json` | Model-team handoff; C++ loader adds rate/identity/normalization/memory, role/dependency and migration checks |
| `model_package_registry.schema.json` | Exact per-model deployment binding to package metadata and artifact paths |
| `feature_catalog.schema.json` | Usecase integration metadata independent of commercial activation |
| `yolov8_decoder.schema.json` | Primary YOLOv8 decoder package; all policy fields required, unknown keys rejected |
| `anchor_distance_decoder.schema.json` | Primary anchor-distance (SCRFD-style) decoder package with landmarks |
| `usecase_control_snapshot.schema.json` | Trusted startup usecase snapshot (catalog + requests + revisions) |
| `usecase_app_manifest.schema.json` | Declarative signed usecase package manifest v1 |
| `usecase_app_entitlement.schema.json` | Signed device/target/app/source/time/output entitlement grant v1 |
| `runtime_control_snapshot.schema.json` | Complete App Manager-to-runtime control projection v1 |
| `hardware_admission_profile.schema.json` | Startup hardware admission profile defining platform memory, FW concurrency and thermal limits |
| `metadata_runtime_profile.schema.json` | P2 metadata service bounds, retention horizons, trajectory provenance and per-feature access domains |
| `integration_contract_registry.schema.json` | Exact C01–C10 ownership, handoff and S01–S18 dependency registry |
| `integration_contract_cases.schema.json` | Bounded valid/rejected baseline conformance-case catalog |
| `integration_contract_receipt.schema.json` | Immutable producer evidence plus AI APP consumer disposition envelope |

## Limits and next work

- Runtime acceptance is authoritative: it also checks cross-field identity uniqueness,
  packed-NV12 allocation size, aggregate memory arithmetic and deployment/model concurrency.
- Authentication, catalog resolution, FW RAW-source resolution and board admission are
  deliberately outside JSON parsing.

## See also

- [Multi-source configuration](../../docs/architecture/multi_source_configuration.md)
- [Model catalog](../../docs/architecture/model_catalog.md), [model package registry](../../docs/architecture/model_package_registry.md), [feature catalog](../../docs/architecture/feature_catalog.md)
- [Three-team integration contract registry](../../docs/contracts/integration_contract_registry.md)
