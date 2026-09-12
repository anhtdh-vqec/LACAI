# eSDK configuration and evidence matrix

The base builds under several option sets. A green result is only valid for the exact set
that produced it; a configuration whose SDK or runner is unavailable is **not-run**, never
reported as passing. All rows below use the approved eSDK AArch64 compiler and SDK
`qemu-aarch64`.

## Verified configurations

| Configuration | Options (all others OFF) | Tests | Evidence |
|---|---|---|---|
| Neutral | none | 53/53 | eSDK QEMU; reference backend, runtime executor and service harness |
| Expanded | CAMERA, CAMERA_DBUS, GST_FRAME_BRIDGE, QUALCOMM, MODEL_MANIFEST, MODEL_CATALOG, DEPLOYMENT_CONFIG, FEATURE_CATALOG, BUILD_MANIFEST_CHECK, ARTIFACT_DIGEST, QNN_ENGINE | 71/71 | eSDK QEMU; adds camera/D-Bus/GStreamer/Qualcomm/JSON/digest and the owned QNN engine library |

`VQEC_VISION_AI_ENABLE_QNN_ENGINE=ON` requires `VQEC_VISION_AI_QAIRT_ROOT` (default
`third_party/qairt`, a symlink to the installed private SDK). The QNN engine has no dedicated
test target yet, so enabling it changes compiled coverage but not the registered test count;
its behavioural evidence is board-gated (see [QNN board validation](qnn_board_validation.md)).

## Option inventory

| Option | Enables | Availability |
|---|---|---|
| `VQEC_VISION_AI_ENABLE_CAMERA` | Linux legacy FD receiver | build environment |
| `VQEC_VISION_AI_ENABLE_CAMERA_DBUS` | GIO D-Bus control transport | build environment |
| `VQEC_VISION_AI_ENABLE_GST_FRAME_BRIDGE` | Linux FD to GstMemory bridge | build environment |
| `VQEC_VISION_AI_ENABLE_QUALCOMM` | private GStreamer Qualcomm adapter | build environment |
| `VQEC_VISION_AI_ENABLE_QNN_ENGINE` | LACAI-owned QNN engine library | private QAIRT SDK |
| `VQEC_VISION_AI_ENABLE_MODEL_MANIFEST` | bounded output-manifest loader | build environment |
| `VQEC_VISION_AI_ENABLE_MODEL_CATALOG` | bounded model-catalog loader | build environment |
| `VQEC_VISION_AI_ENABLE_DEPLOYMENT_CONFIG` | bounded deployment loader | build environment |
| `VQEC_VISION_AI_ENABLE_FEATURE_CATALOG` | bounded feature-catalog loader | build environment |
| `VQEC_VISION_AI_BUILD_MANIFEST_CHECK` | optional metadata checker executable | build environment |
| `VQEC_VISION_AI_ENABLE_ARTIFACT_DIGEST` | SHA-256 artifact comparison | build environment |
| `VQEC_VISION_AI_ENABLE_FW_RING` | FW shared-memory ring wrapper | pinned FW SDK; not built |

`VQEC_VISION_AI_WERROR=ON` (default) treats project warnings as errors.

## Rules

- A missing private SDK, sysroot or runner is recorded as not-run with the exact blocker;
  it is not a skipped-then-green result.
- Board/live results are preferred over emulation where the sysroot lacks a component
  (for example `gst-plugin-scanner`), but emulation is never device/BSP acceptance.
- Every report pins the LACAI commit, eSDK, image/kernel, plugin/QNN binary, firmware and
  package digest; see the [metrics section](../planning/model_agnostic_optimization_plan.md)
  of the optimization plan.
- Logs must not contain secrets, credentials or biometric data.

## Reproduce

```bash
source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux

cmake -S . -B build-esdk-neutral -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
  "-DCMAKE_CROSSCOMPILING_EMULATOR=/home/a/Workspace/eSDK/tmp/sysroots/x86_64/usr/bin/qemu-aarch64;-L;$SDKTARGETSYSROOT"
cmake --build build-esdk-neutral -j4
ctest --test-dir build-esdk-neutral --output-on-failure
```

The expanded command is in the repository [README](../../README.md#build-và-test).
