# features

Business-capability packages that consume neutral perception and emit bounded feature events.

- **Status:** source-delivered — package maturity is recorded per child README and catalog
- **Layer:** features
- **Naming registry:** one `dir_id` per feature package
- **Depends on:** feature contracts, portable perception and runtime feature stage
- **Used by:** application feature pipeline

## Responsibility

- Keep one business capability per directory and register it through the processor factory.
- Declare model/attribute/resource dependencies in the feature catalog.
- Avoid camera, DSP, QNN, transport and evidence-storage ownership.

## Contents

| Path | Purpose |
|---|---|
| `fire_smoke/` | S04 typed temporal fire/smoke incident processor |
| `abandoned_object/` through `suspicious_object/` | Security-camera feature packages |
| `traffic/` | Traffic-camera feature family boundary |

## Limits and next work

- Directory presence is not feature acceptance; consult each package and capability matrix.

## See also

- [Feature catalog](../../docs/architecture/feature_catalog.md)
- [Repository source layout](../../docs/development/source_layout.md)
