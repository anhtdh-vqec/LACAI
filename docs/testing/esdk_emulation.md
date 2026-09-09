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
[CTest output](esdk_neutral_ctest_2026_09_09.txt). These failures remain unresolved.
This validates neither device DMA completion nor Qualcomm plugins, FW transport,
model accuracy, performance or board compatibility. Optional JSON loader tests are
not present in this configuration.
