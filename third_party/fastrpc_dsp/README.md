# third_party/fastrpc_dsp

Legacy FastRPC client stub, RPC codes, geometry/postprocessing types, and host-only C reference
implementations for communicating with the deployed Hexagon cDSP skeleton library
`libvqec_dsp_skel.so` on Qualcomm QCS6490.

This directory is a migration input, not an accepted generic adapter or a complete vendored
package. Production integration is owned by `src/adapters/qualcomm`.

- **Status:** source-delivered — legacy sources exist; provenance and generic ABI work remain open
- **Depends on:** FastRPC runtime and deployed cDSP skeleton
- **Used by:** private Qualcomm adapter and host regression tests

## Responsibility

- Preserve the exact legacy wire client and reference behavior needed during migration.
- Do not define the neutral execution contract or act as the source for new model kernels.

## Contents

- `vqec_dsp.h`: QAIC-generated client interface header defining `vqec_dsp_open`, `vqec_dsp_close`,
  `vqec_dsp_set_clocks`, `vqec_dsp_preprocess_*`, `vqec_dsp_postprocess_*`, and `vqec_dsp_compose`.
- `vqec_dsp_stub.c`: QAIC-generated FastRPC client stub marshalling calls over `libcdsprpc.so`.
- `vqec_dsp_types.h`: Geometry constants, quantized threshold filtering conventions, and box layout formats.
- `vqec_dsp_codes.h`: Error codes returned by `libvqec_dsp_skel.so`.
- `post_common.h` / `post_common.c`: Common candidate push, quantized thresholding, and greedy NMS logic.
- `post_person_yolov8n.h` / `post_person_yolov8n.c`: YOLOv8 person detection candidate dequantization and postprocessing.
- `post_face_scrfd.h` / `post_face_scrfd.c`: SCRFD face detection candidate dequantization and postprocessing.

## Provenance boundary

The committed `vqec_dsp.h` and `vqec_dsp_stub.c` are byte-identical to QAIC-generated files
found in the legacy FW application at the time of the 2026-09-18 review. LACAI does not yet
contain the input IDL, exact QAIC/Hexagon SDK version, regeneration command or per-file
provenance receipt. The remaining C files are host regression fixtures and custom algorithm
sources; they are not Qualcomm SDK source merely because this directory is named `third_party`.

Do not add model kernels here or copy more files from the sibling repository. The replacement
must use project-owned, versioned operation descriptors and kernels, with generated artifacts
separated from authored source. See the architecture plan before changing this wire ABI.

## License

The previous blanket `BSD-3-Clause / Qualcomm Technologies, Inc.` statement was not supported
by per-file headers or a provenance manifest and has been removed. Release/distribution approval
is blocked until AI APP and BSP record the license and generator provenance for every retained
file. No Qualcomm ownership claim is made for the custom reference algorithms.

## See also

- [FastRPC adapter architecture](../../docs/architecture/qualcomm_fastrpc_adapter.md)
- [DSP optimization plan](../../docs/planning/architecture_improvement/dsp_multiplatform_optimization_plan.md)
