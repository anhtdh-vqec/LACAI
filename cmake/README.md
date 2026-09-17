# cmake

Toolchain files, SDK discovery and target-scoped build helpers.

- **Status:** split by module — the root is an option/summary file (~170 lines) that add_subdirectory-s each module
- **Rule:** no global include/link directory settings

## Responsibility

- Provide target-scoped SDK discovery and toolchain files.
- Keep host build possible without a vendor SDK; target build uses the pinned eSDK sysroot.
- Move each module's target definition next to its source and `add_subdirectory` it from the
  root, without renaming targets.

## Current module files

| File | Targets |
|---|---|
| `src/core/CMakeLists.txt` | `vqec_vision_ai_core` |
| `src/perception/CMakeLists.txt` | detection, tracking, attributes |
| `src/runtime/CMakeLists.txt` | feature runtime, loaders, model registry, admission |
| `src/outputs/CMakeLists.txt` | encoded dispatch, feature-event dispatch, overlay preparation |
| `src/app/CMakeLists.txt` | orchestration targets and the service executable |
| `tests/CMakeLists.txt` | all unit/contract/board test targets |
| `src/adapters/camera/CMakeLists.txt` | camera wire/control/dbus/camera |
| `src/adapters/qualcomm/CMakeLists.txt` | gst frame bridge, qualcomm, qnn engine + smoke |
| `src/adapters/fw_output/CMakeLists.txt` | FW ring sink |
| `src/adapters/fw_control/CMakeLists.txt` | usecase-control D-Bus, enrollment D-Bus, image-path authorizer |
| `src/adapters/storage/CMakeLists.txt` | encrypted face-gallery store |
| `src/adapters/zvec/CMakeLists.txt` | Zvec embedding index |
| `src/adapters/reference/CMakeLists.txt` | `vqec_vision_ai_reference` |
| `src/runtime/scheduler/CMakeLists.txt` | `vqec_vision_ai_scheduler`, `vqec_vision_ai_inference_worker` |

Warning flags (-Wall -Wextra -Wpedantic, -Werror under VQEC_VISION_AI_WERROR) are set once at
the root before the subdirectories so every module target inherits them.

## See also

- [eSDK emulation](../docs/testing/esdk_emulation.md)
- [Implementation status](../docs/development/implementation_status.md)
