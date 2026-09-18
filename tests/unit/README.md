# unit

Single-module unit tests for contracts, validation, bookkeeping and scheduler arithmetic.

- **Status:** board-smoke — the complete eSDK configuration passes 135/135 CTest entries and
  130/130 cross-built executables pass on QCS6490 `.98` on 2026-09-18.
- **Runner:** eSDK AArch64 compiler and SDK QEMU (`ctest`); native target execution uses the
  board runner, which supplies the manifest and Zvec fixtures two device-free tests need.

## Responsibility

- Cover structural plan validation (valid `UINT8`/`FLOAT32`, 4K NV12 byte count, invalid
  geometry/FPS, explicit placement, NaN/zero coefficients, unsupported type, invalid paths
  and queue budgets).
- Cover contracts, submission window, cadence, supervisor fairness and reference helpers.

## Contents

| Path | Purpose |
|---|---|
| `core/` | Pure value, validation, contract and bounded bookkeeping tests |
| `application/` | Application pipeline, session, composition and supervisor tests |
| `runtime/` | Loader, registry, activation, lifecycle and scheduling tests |
| `perception/` | Decoder, tracker, recognition and embedding-policy tests |
| `adapters/` | Adapter tests grouped again by camera/FW/Qualcomm/reference/storage owner |

## Limits and next work

- Tests do not prove plugin negotiation, SDK compatibility, golden parity or DMA safety.
- Test `main` is the approved language exception; other symbols follow the naming registry.

## See also

- [eSDK emulation](../../docs/testing/esdk_emulation.md)
- [Repository source layout](../../docs/development/source_layout.md)
