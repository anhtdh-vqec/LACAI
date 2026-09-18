# adapters

Private implementations of neutral ports for FW services, hardware vendors, persistence and
device-free reference execution.

- **Status:** source-delivered — implementations are built by owner-specific CMake targets
- **Layer:** adapters
- **Naming registry:** owner-specific `dir_id` values in `docs/development/naming_registry.md`
- **Depends on:** neutral contracts/ports and portable core/runtime data
- **Used by:** application platform composition

## Responsibility

- Contain external ABI, SDK, OS-service and storage dependencies.
- Translate external ownership/completion semantics into neutral LACAI contracts.
- Prevent vendor types and headers from crossing into core, runtime, perception or features.

## Contents

| Path | Purpose |
|---|---|
| `camera/`, `fw_control/`, `fw_output/` | Released/compatibility FW boundaries |
| `qualcomm/` | QCS6490 QNN, GStreamer, media and DSP implementations |
| `reference/` | Device-free conformance implementations |
| `storage/`, `zvec/` | Protected persistence and vector-index adapters |
| Future vendor adapter | Add only with an owned SDK boundary, neutral port and reviewable source |

## Limits and next work

- A directory or compiled adapter does not prove hardware qualification.
- Cross-platform behavior is accepted only through the same neutral contract fixtures.

## See also

- [Repository source layout](../../docs/development/source_layout.md)
- [System architecture](../../docs/architecture/system_architecture.md)
