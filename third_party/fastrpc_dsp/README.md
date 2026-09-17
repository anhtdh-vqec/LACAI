# third_party/fastrpc_dsp

Qualcomm FastRPC client stub, RPC codes, geometry/postprocessing types, and C reference
implementations for communicating with the Hexagon cDSP skeleton library `libvqec_dsp_skel.so`
on Qualcomm QCS6490 (Qualcomm Linux 1.8 / Hexagon v68).

## Contents

- `vqec_dsp.h`: QAIC-generated client interface header defining `vqec_dsp_open`, `vqec_dsp_close`,
  `vqec_dsp_set_clocks`, `vqec_dsp_preprocess_*`, `vqec_dsp_postprocess_*`, and `vqec_dsp_compose`.
- `vqec_dsp_stub.c`: QAIC-generated FastRPC client stub marshalling calls over `libcdsprpc.so`.
- `vqec_dsp_types.h`: Geometry constants, quantized threshold filtering conventions, and box layout formats.
- `vqec_dsp_codes.h`: Error codes returned by `libvqec_dsp_skel.so`.
- `post_common.h` / `post_common.c`: Common candidate push, quantized thresholding, and greedy NMS logic.
- `post_person_yolov8n.h` / `post_person_yolov8n.c`: YOLOv8 person detection candidate dequantization and postprocessing.
- `post_face_scrfd.h` / `post_face_scrfd.c`: SCRFD face detection candidate dequantization and postprocessing.

## License

BSD-3-Clause / Qualcomm Technologies, Inc.
