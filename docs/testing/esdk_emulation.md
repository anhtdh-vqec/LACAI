# AArch64 logic tests through eSDK

The eSDK contains qemu-aarch64 8.2.7 at
`/home/a/Workspace/eSDK/tmp/sysroots/x86_64/usr/bin/qemu-aarch64`.
It becomes available after sourcing the SDK environment. Checking only the initial
host PATH incorrectly suggested that emulation was unavailable.

Reproduce the neutral configuration in a fresh build directory:

```bash
source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux
cmake -S . -B build-esdk -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
  "-DCMAKE_CROSSCOMPILING_EMULATOR=/home/a/Workspace/eSDK/tmp/sysroots/x86_64/usr/bin/qemu-aarch64;-L;$SDKTARGETSYSROOT"
cmake --build build-esdk -j4
ctest --test-dir build-esdk --output-on-failure
```

Baseline source revision: 180f0c6. All optional adapters/loaders are OFF in this fresh
configuration. Debug leaves assertions enabled. All binaries are compiled with the
SDK AArch64 compiler and executed by SDK QEMU against the target sysroot.

2026-09-09 result: 45/47 passed. `multi_model_frame_fanout` reports one failed check;
`legacy_camera_wire` reports 16 failed checks. Full output is preserved in
[CTest output](esdk_neutral_ctest_2026_09_09.txt). These baseline failures were subsequently traced to stale fixtures (see below).
This validates neither device DMA completion nor Qualcomm plugins, FW transport,
model accuracy, performance or board compatibility. Optional JSON loader tests are
not present in this configuration.

After fixture corrections, the same neutral Debug configuration passes 47/47 tests.
The wire fixture now supplies explicit geometry/allocation limits and verifies that
unset policy preserves output on rejection. The fan-out fixture expects source epoch 7,
matching its supplied frame instead of inventing epoch 1. Production code is unchanged.
[Corrected CTest output](esdk_neutral_ctest_corrected_2026_09_09.txt) records this run.

An expanded configuration with Camera, GIO D-Bus, the GStreamer frame bridge and the
Qualcomm adapter enabled builds 57 tests. The corresponding binaries pass 57/57 under
SDK QEMU with an isolated target-sysroot GStreamer registry, and also pass 57/57 when
executed natively on the QCS6490 target; see [board smoke evidence](qsc6490_board.md).
The board result is preferred over emulation for the GStreamer lifecycle fixture because
the eSDK sysroot has no `gst-plugin-scanner`. It still does not replace live model/FW,
DMA completion, performance or recovery qualification.
