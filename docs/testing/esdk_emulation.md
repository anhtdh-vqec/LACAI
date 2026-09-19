# AArch64 logic tests through eSDK

This document records how LACAI runs AArch64 logic/wiring tests under the approved eSDK
QEMU, the current pass counts and the evidence boundary. It exists so that a missing host
`qemu-aarch64` is not mistaken for emulation being unavailable.

**Status:** logic-tested — 100/100 neutral and 162/162 expanded under eSDK QEMU; emulation is
logic/wiring evidence only. **Layer:** reference. **Source:** `n/a`.

Configuration/evidence option matrix:
[esdk_configuration_matrix.md](esdk_configuration_matrix.md).
A configuration whose SDK or runner is unavailable is not-run, never green.

The eSDK contains qemu-aarch64 8.2.7 at
`/home/a/Workspace/eSDK/tmp/sysroots/x86_64/usr/bin/qemu-aarch64`. It becomes available
after sourcing the SDK environment. Checking only the initial host PATH incorrectly
suggested that emulation was unavailable.

## Responsibility

- Records the eSDK QEMU runner location and the logic-test procedure.
- States that emulation validates wiring and ABI, not device DMA, vendor plugins, transport,
  accuracy, performance or board compatibility.
- Must not be reported as device/BSP acceptance.

## Current result

- Neutral configuration (Zvec OFF): 100/100 on 2026-09-18, including the three-team contract
  registry and receipt self-test.
- Expanded configuration (camera, GIO D-Bus, GStreamer bridge, Qualcomm, FastCV, QNN engine,
  JSON loaders, Zvec): 162/162 on 2026-09-20, including S04 App Manager, evidence transport,
  metadata, lazy enrollment and no-data backend-factory tests.

Both run the SDK AArch64 compiler under SDK QEMU against the target sysroot. This is
logic/wiring evidence only. It validates neither device DMA completion nor Qualcomm plugins,
FW transport, model accuracy, performance or board compatibility. Board `.98` native
results are separate; see [QSC6490 target](qsc6490_board.md).

## Reproduce (neutral)

```bash
source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux
lacai_neutral_build="$(mktemp -d /tmp/lacai-esdk-neutral.XXXXXX)"
cmake -S . -B "$lacai_neutral_build" -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
  -DVQEC_VISION_AI_ENABLE_ZVEC=OFF \
  "-DCMAKE_CROSSCOMPILING_EMULATOR=/home/a/Workspace/eSDK/tmp/sysroots/x86_64/usr/bin/qemu-aarch64;-L;$SDKTARGETSYSROOT"
cmake --build "$lacai_neutral_build" -j4
ctest --test-dir "$lacai_neutral_build" --output-on-failure
```

The expanded command is in the repository [README](../../README.md#build-và-test).

## Historical evidence

Dated raw runs are retained verbatim and are not updated retroactively:

- The 2026-09-09 neutral run changed from 45/47 to 47/47 after fixture corrections.
  Raw transient CTest output is not retained in the documentation tree; current counts
  belong in implementation status and reproducible CI evidence.

## Limits and next work

- Emulation cannot validate device DMA completion, Qualcomm plugins, FW transport, model
  accuracy, performance or board compatibility.
- A missing SDK runner is recorded as not-run, not as a green configuration.

## See also

- [eSDK configuration and evidence matrix](esdk_configuration_matrix.md)
- [QCS6490 board test target](qsc6490_board.md)
