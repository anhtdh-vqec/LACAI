# defaults

Versioned examples, never product defaults. Do not enable purchased features automatically.

- **Status:** example only
- **Rule:** product profiles and budgets must come from board-qualified workloads

## Contents

| File | Purpose |
|---|---|
| `deployment.example.json` | Demonstrates two identical FW RAW source references |
| `model_package_registry.example.json` | Demonstrates distinct package/artifact bindings for two generic models |
| `deployment.person_face.example.json` | One-source composition with person detection and the FR root detector active together |
| `model_package_registry.person_face.example.json` | Package bindings for YOLOv8n-person, SCRFD and EdgeFace |

The example dimensions, rates, memory values, model assignments and released
`detect0`/`detect1` outputs are illustrative. Never install it as an effective configuration
without product review; FW RAW-source resolution and board admission stay outside JSON parsing.
The person/face example deliberately lists the two primary models only. EdgeFace is a
secondary model activated through the SCRFD dependency in the matching model catalog.

## See also

- [Deployment schema](../schemas/deployment.schema.json), [multi-source configuration](../../docs/architecture/multi_source_configuration.md)
