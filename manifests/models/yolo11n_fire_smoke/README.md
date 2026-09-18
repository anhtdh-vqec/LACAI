# YOLO11n fire and smoke model package

This directory carries the reviewed metadata envelope for the quantized fire/smoke model.

- **Status:** source-delivered — metadata is present; golden parity and quality acceptance
  remain open
- **Depends on:** model-integration and tensor contracts
- **Used by:** production composition and model validation tools

## Responsibility

Define the offline package metadata for the quantized YOLO11n ReplayMix model targeting
smoke and fire detection on Qualcomm QCS6490 HTP. Model quality, runtime ownership and
accelerator acceptance remain outside this metadata directory.

## Contents

- Model ID: `yolo11n_fire_smoke`
- Input: `1x320x320x3` NHWC uint16 (quantized scale `1.5259021893143654e-05`, offset `0`)
- Outputs:
  - `boxes_out`: `1x4x2100` uint16 (quantized scale `0.013475208543241024`, offset `-2783`)
  - `conf_out`: `1x2x2100` uint16 (quantized scale `1.52587890625e-05`, offset `0`)
- Classes: `smoke` (0), `fire` (1)
- Decoder contract: `yolo.v8.fire_smoke`
- Graph Name: `yolo11n_replaymix_w8a16`
- Target: Qualcomm QCS6490 HTP (W8A16)

## Limits and next work

- The current generic CPU decoder supports the two-class head. The legacy cDSP dense decoder
  accepts only a 640-square, 8400-prediction, one-class envelope and therefore must not be
  selected for this 320-square, 2100-prediction, two-class model.
- The legacy cDSP preprocess wire does not describe color matrix/range or interpolation.
  The declared BT.709-limited bilinear transform requires model-team golden tensor parity
  before accelerator acceptance.
- Visual boxes and output FPS do not establish model accuracy, preprocessing parity, memory
  safety or the product CPU target.

## See also

- [Model integration contract](../../../docs/contracts/model_integration.md)
- [DSP and multiplatform optimization plan](../../../docs/planning/architecture_improvement/dsp_multiplatform_optimization_plan.md)
