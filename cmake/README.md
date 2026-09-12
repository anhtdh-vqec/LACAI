# cmake

Toolchain files, SDK discovery and target-scoped build helpers.

- **Status:** planned helpers — current targets are declared in the top-level `CMakeLists.txt`
- **Rule:** no global include/link directory settings

## Responsibility

- Provide target-scoped SDK discovery and toolchain files.
- Keep host build possible without a vendor SDK; target build uses the pinned eSDK sysroot.

## See also

- [eSDK emulation](../docs/testing/esdk_emulation.md)
