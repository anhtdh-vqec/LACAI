# unit

Single-module unit tests for contracts, validation, bookkeeping and scheduler arithmetic.

- **Status:** source-delivered. The exact registration is authoritative in `tests/CMakeLists.txt`;
  the expanded eSDK configuration currently registers 123 CTest tests, and the cross-built
  target binaries pass natively on QCS6490 via [the board runner](../../tools/vqec_vision_board_native_tests.sh).
- **Runner:** eSDK AArch64 compiler and SDK QEMU (`ctest`); native target execution uses the
  board runner, which supplies the manifest and Zvec fixtures two device-free tests need.

## Responsibility

- Cover structural plan validation (valid `UINT8`/`FLOAT32`, 4K NV12 byte count, invalid
  geometry/FPS, explicit placement, NaN/zero coefficients, unsupported type, invalid paths
  and queue budgets).
- Cover contracts, submission window, cadence, supervisor fairness and reference helpers.

## Limits and next work

- Tests do not prove plugin negotiation, SDK compatibility, golden parity or DMA safety.
- Test `main` is the approved language exception; other symbols follow the naming registry.

## See also

- [eSDK emulation](../../docs/testing/esdk_emulation.md)
