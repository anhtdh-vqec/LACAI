# AArch64 logic tests through eSDK

Configuration/evidence option matrix: [esdk_configuration_matrix.md](esdk_configuration_matrix.md).
A configuration whose SDK or runner is unavailable is not-run, never green.

The eSDK contains qemu-aarch64 8.2.7 at
`/home/a/Workspace/eSDK/tmp/sysroots/x86_64/usr/bin/qemu-aarch64`. It becomes available
after sourcing the SDK environment. Checking only the initial host PATH incorrectly
suggested that emulation was unavailable.

## Current result

- Neutral configuration (Zvec OFF): 76/76.
- Expanded configuration (camera, GIO D-Bus, GStreamer bridge, Qualcomm, FastCV, QNN engine,
  JSON loaders, Zvec): 123/123.

Both run the SDK AArch64 compiler under SDK QEMU against the target sysroot. This is
logic/wiring evidence only. It validates neither device DMA completion nor Qualcomm plugins,
FW transport, model accuracy, performance or board compatibility. Board `.98` native
results are separate; see [QSC6490 target](qsc6490_board.md).

## Reproduce (neutral)

```bash
source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux
cmake -S . -B build-esdk-neutral -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
  -DVQEC_VISION_AI_ENABLE_ZVEC=OFF \
  "-DCMAKE_CROSSCOMPILING_EMULATOR=/home/a/Workspace/eSDK/tmp/sysroots/x86_64/usr/bin/qemu-aarch64;-L;$SDKTARGETSYSROOT"
cmake --build build-esdk-neutral -j4
ctest --test-dir build-esdk-neutral --output-on-failure
```

The expanded command is in the repository [README](../../README.md#build-và-test).

## Historical evidence

Dated raw runs are retained verbatim and are not updated retroactively:

- [neutral run 2026-09-09](esdk_neutral_ctest_2026_09_09.txt) (45/47 before fixture fixes).
- [neutral corrected run 2026-09-09](esdk_neutral_ctest_corrected_2026_09_09.txt) (47/47).
