# defaults

Versioned examples, never product defaults. Do not enable purchased features automatically.

- **Status:** example only
- **Rule:** product profiles and budgets must come from board-qualified workloads

## Contents

| File | Purpose |
|---|---|
| `deployment.example.json` | Demonstrates two identical FW RAW source references |
| `model_package_registry.example.json` | Demonstrates distinct package/artifact bindings for two generic models |
| `deployment.face.example.json` | One-source face-only composition (SCRFD + EdgeFace) |
| `deployment.person_face.example.json` | One-source composition with person detection and the FR root detector active together |
| `model_package_registry.face.example.json` | Package bindings for SCRFD and EdgeFace |
| `model_package_registry.person_face.example.json` | Package bindings for YOLOv8n-person, SCRFD and EdgeFace |
| `usecase_control_snapshot.person_face.example.json` | Example trusted startup snapshot enabling person detection and FR |
| `hardware_admission_profile.qcs6490.example.json` | Measured QCS6490 admission profile example with memory, DDR, FW concurrency and thermal limits |
| `metadata_runtime_profile.qcs6490.example.json` | P2 metadata lifecycle, storage, retention, source revision and per-feature access-domain example |
| `metadata_runtime_profile.fire_smoke.example.json` | S04 event/region trajectory projection without requiring an unrelated person model |
| `deployment.fire_smoke.example.json` | One-source S04 fire/smoke model assignment |
| `usecase_control_snapshot.fire_smoke.example.json` | Transitional S04 startup authority fixture using canonical product identity |
| `fire_smoke_configuration.example.json` | Strict revision-1 S04 behavior/evidence/metadata configuration |
| `feature_catalog.fire_smoke.example.json` | S04 processor dependency and bounded feature resource contract |
| `usecase_app_manifest.fire_smoke.example.json` | Declarative S04 application package manifest |
| `usecase_app_entitlement.fire_smoke.example.json` | Example S04 entitlement signing input; filesystem presence grants no authority |
| `usecase_app_catalog.example.json` | Complete S01-S18 product catalog; compiled support and lifecycle gates remain authoritative |

The example dimensions, rates, memory values, model assignments and released
`detect0`/`detect1` outputs are illustrative. Never install it as an effective configuration
without product review; FW RAW-source resolution and board admission stay outside JSON parsing.
The person/face example deliberately lists the two primary models only. EdgeFace is a
secondary model activated through the SCRFD dependency in the matching model catalog.
The usecase snapshot is illustrative and grants no entitlement by itself. Production must
authenticate the catalog and entitlement revision before passing the document to AI APP.

`usecase_app_manifest.fire_smoke.example.json` and
`fire_smoke_configuration.example.json` are declarative S04 package/configuration fixtures.
They are not signed install receipts or entitlement grants. The entitlement example is signing
input only and grants nothing by filesystem presence. Production App Manager must verify
the package envelope, complete dependency closure and grant before publishing a runtime snapshot.

## See also

- [Deployment schema](../schemas/deployment.schema.json), [multi-source configuration](../../docs/architecture/multi_source_configuration.md)
