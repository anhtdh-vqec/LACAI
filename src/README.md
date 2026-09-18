# source tree

Implementation map for LACAI layers. Physical folders identify ownership; public include
paths and logical symbol prefixes remain stable when implementation files move.

- **Status:** board-smoke — eSDK 135/135 and QCS6490 `.98` 130/130 plus service smoke pass
- **Layer:** app
- **Naming registry:** see each owning module README
- **Depends on:** `include/vqec/vision/ai/`
- **Used by:** product executable, tests and packaging

## Responsibility

- Keep portable policy, orchestration and vendor integration in separate physical layers.
- Route changes to the team that owns the implementation and its external contracts.
- Preserve installed header paths and symbol ownership independently of source layout.

## Contents

| Path | Purpose |
|---|---|
| `core/` | Pure values, validation, policy and bounded bookkeeping |
| `runtime/` | Activation, admission, registries, lifecycle and scheduling |
| `perception/` | Decoder, tracking, attribute, pose, embedding and OCR algorithms |
| `features/` | Security/traffic feature processors, one package per business capability |
| `outputs/` | Portable event and media authorization/delivery preparation |
| `app/` | Sessions, pipelines, composition, supervision and executable entry point |
| `adapters/` | FW, platform, storage and vendor-specific implementations behind ports |

## Limits and next work

- The layer map is not permission to introduce reverse dependencies; CMake target links and
  neutral ports remain authoritative.
- Public contract/header regrouping requires a compatibility decision and is intentionally
  outside this physical source refactor.

## See also

- [Repository source layout](../docs/development/source_layout.md)
- [System architecture](../docs/architecture/system_architecture.md)
