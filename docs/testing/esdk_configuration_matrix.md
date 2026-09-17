# eSDK configuration and evidence matrix

This document records the eSDK option sets LACAI is built and tested under, the test counts
verified for each, and the rule that a green result is only valid for the exact set that
produced it. It prevents a missing SDK or runner from being reported as a passing
configuration.

**Status:** logic-tested — 76/76 neutral and 123/123 expanded under eSDK QEMU; board-only
QNN engine evidence is separate. **Layer:** reference. **Source:** `n/a`.

The base builds under several option sets. A green result is only valid for the exact set
that produced it; a configuration whose SDK or runner is unavailable is **not-run**, never
reported as passing. All rows below use the approved eSDK AArch64 compiler and SDK
`qemu-aarch64`.

## Responsibility

- Defines the supported CMake option sets and the evidence status of each.
- Records a configuration whose SDK or runner is unavailable as not-run with the exact
  blocker.
- Must not present a missing private SDK, sysroot or runner as a skipped-then-green result,
  and must not equate emulation with device/BSP acceptance.

## Verified configurations

| Configuration | Options | Tests | Evidence |
|---|---|---|---|
| Neutral | `VQEC_VISION_AI_ENABLE_ZVEC=OFF`, all else OFF | 76/76 | eSDK QEMU; reference backend, runtime executor, service harness, bounded inference worker and tensor pool |
| Expanded | CAMERA, CAMERA_DBUS, GST_FRAME_BRIDGE, QUALCOMM, FASTCV, QNN_ENGINE, MODEL_MANIFEST, MODEL_CATALOG, DEPLOYMENT_CONFIG, FEATURE_CATALOG, BUILD_MANIFEST_CHECK, ARTIFACT_DIGEST, FACE_ENROLLMENT_DBUS, USECASE_CONTROL_DBUS, ZVEC | 123/123 | eSDK QEMU; adds camera/D-Bus/GStreamer/Qualcomm/FastCV/JSON/digest, the owned QNN engine and the Zvec index |

`VQEC_VISION_AI_ENABLE_QNN_ENGINE=ON` requires `VQEC_VISION_AI_QAIRT_ROOT` (default
`third_party/qairt`, a symlink to the installed private SDK). The QNN engine has no
registered host/QEMU test; enabling it changes compiled coverage but not the test count. Its
behavioural evidence comes from the board-only `vqec_vision_ai_qnn_engine_smoke` tool
(compose/finalize/execute + parity/latency) on QCS6490; see
[QNN board validation](qnn_board_validation.md) and [QCS6490 target](qsc6490_board.md).

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
| `VQEC_VISION_AI_ENABLE_ZVEC` | derived embedding index (default ON) | pinned public ARM64 SDK via `tools/vqec_vision_prepare_zvec.sh` |
| `VQEC_VISION_AI_ENABLE_FASTCV` | Qualcomm FastCV preprocessing/alignment | build environment |
| `VQEC_VISION_AI_ENABLE_FW_RING` | FW shared-memory ring wrapper | pinned FW SDK; not built |

`VQEC_VISION_AI_WERROR=ON` (default) treats project warnings as errors.

## CI jobs

The workflow runs unconditionally: `structure` (source-layout check), `host-sanitizers`
(Clang ASan+UBSan over the neutral configuration with Zvec OFF), advisory `clang-tidy`, and
a scheduled `fuzz` job (libFuzzer over the wire/decoder/parser inputs). The `esdk-neutral`
(Zvec OFF) and `esdk-expanded` (Zvec bootstrapped) jobs are gated on the `ESDK_ROOT`
repository variable and use a self-hosted runner; host jobs are not a substitute for target
evidence. Zvec is disabled in the neutral/host configurations because its SDK is not tracked.

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

## Limits and next work

- QNN engine coverage is board-only; there is no host/QEMU test, so it changes compiled
  coverage rather than the test count.
- Board/live results remain preferred where the sysroot lacks a component, but they are
  still not device/BSP acceptance.

## See also

- [QNN model and engine board validation](qnn_board_validation.md)
- [QCS6490 board test target](qsc6490_board.md)
- [Actions from the optimization plan](../planning/model_agnostic_optimization_plan.md)
