# cmake

Toolchain files, SDK discovery and target-scoped build helpers.

- **Status:** top-level `CMakeLists.txt` still owns most targets; module CMake files are being added incrementally
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
| `src/adapters/reference/CMakeLists.txt` | `vqec_vision_ai_reference` |
| `src/runtime/scheduler/CMakeLists.txt` | `vqec_vision_ai_scheduler`, `vqec_vision_ai_inference_worker` |

Remaining modules (app, other adapters, perception, features, outputs, runtime, tests) still
live in the root file and will move the same way.

## See also

- [eSDK emulation](../docs/testing/esdk_emulation.md)
- [Device-free basecode progress](../docs/development/LACAI_DEVICE_FREE_BASECODE_PROGRESS.md)
