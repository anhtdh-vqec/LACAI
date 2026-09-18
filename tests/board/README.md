# board

Qualcomm SDK, memory/fence, thermal/performance and fault/soak tests that must run on the
QCS6490 target. Host or QEMU green is insufficient evidence.

- **Status:** no board test sources here; native board execution uses `tools/board/vqec_vision_board_native_tests.sh` (117/117 on `.98` with fixtures)
- **Depends on:** a reachable QCS6490 target (see [board notes](../../docs/testing/qsc6490_board.md))

## Responsibility

- Qualify live SDK, DMA/fence completion, thermal/performance envelopes and recovery.
- Provide the evidence the emulation results cannot.

## See also

- [QCS6490 board notes](../../docs/testing/qsc6490_board.md), [QNN board validation](../../docs/testing/qnn_board_validation.md)
- [eSDK emulation limits](../../docs/testing/esdk_emulation.md)
