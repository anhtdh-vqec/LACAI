# tests

Verification sources and fixtures grouped first by evidence type, then by implementation owner.

- **Status:** board-smoke — eSDK 135/135 and QCS6490 `.98` 130/130 candidate tests pass
- **Naming registry:** `unit`, `ctest`, `gold`, `replay`, `integ`, `board`
- **Depends on:** production targets, neutral fakes and explicit fixtures
- **Used by:** CI, QEMU and board validation workflows

## Responsibility

- Separate pure/cross-module logic evidence from integration and board acceptance.
- Mirror owner paths under `unit/` and `contract/` for direct review routing.
- Keep golden, replay and board evidence immutable and purpose-specific.

## Contents

| Path | Purpose |
|---|---|
| `unit/` | Single-owner tests grouped by source layer/adapter |
| `contract/` | Cross-owner boundary tests grouped by source layer/adapter |
| `integration/` | Process/component integration fixtures |
| `board/` | Target-specific probes and evidence inputs |
| `golden/`, `replay/`, `fuzz/` | Deterministic expected data, recorded inputs and robustness tests |

## Limits and next work

- QEMU/native passes do not imply released-FW, accuracy, thermal or long-run acceptance.

## See also

- [Repository source layout](../docs/development/source_layout.md)
- [Board workspace](../docs/testing/board_workspace.md)
