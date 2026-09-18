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

Read-only comparison on 2026-09-18 found the legacy FW IDL at
`application/ai_app/dsp/inc/vqec_dsp.idl` (SHA-256
`25ecda0aacc8ed873ebb3803168dc10b6aa269ecda1765153c575171b968762a`). The legacy
`dsp/build.sh` invokes a Hexagon SDK 5.5.7.0 build and copies generated host files from its
build output. This is a historical recipe, not an approved LACAI toolchain or a reproduced build.
The IDL, exact generator executable/version, licensing receipt and per-file author approval are
still absent from LACAI.

| LACAI file(s) | Read-only comparison with legacy FW `application/ai_app/dsp/` | Disposition |
|---|---|---|
| `vqec_dsp.h`, `vqec_dsp_stub.c` | Byte-identical to `generated/`; SHA-256 `bf408bca…`, `f039c9ea…` | QAIC output; retain only for legacy ABI; regenerate from reviewed IDL before release |
| `vqec_dsp_types.h`, `vqec_dsp_codes.h` | Byte-identical to `inc/`; SHA-256 `4c82a65b…`, `d5b17782…` | Authored wire definitions; license/owner review pending |
| `post_common.c`, `post_common.h` | Byte-identical to `src/`; SHA-256 `9cd32bce…`, `21304541…` | Authored legacy reference algorithm; license/owner review pending |
| `post_person_yolov8n.c`, `.h` | Byte-identical to `src/`; SHA-256 `3d3969be…`, `5f53759a…` | Model-specific reference; replace with operation descriptor backend |
| `post_face_scrfd.c`, `.h` | Byte-identical to `src/`; SHA-256 `f357dfd8…`, `696f1965…` | Model-specific reference; replace with operation descriptor backend |
| `pre.c`, `pre.h` | Deliberately **not** byte-identical to `src/`; SHA-256 `23f7da04…`, `587c1cb1…` | LACAI host scalar fixture; not the deployed FastCV cDSP kernel |

The abbreviated SHA-256 values identify this snapshot; run `sha256sum` on the individual files
for full digests before a release receipt. No claim is made that the historical recipe produced
the deployed skeleton, or that the modified host `pre.c` is numerically equivalent to FastCV.

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
