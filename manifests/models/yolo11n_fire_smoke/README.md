# YOLO11n Fire and Smoke Detection Package

Status: approved

## Purpose

Defines the offline package metadata for the quantized YOLO11n ReplayMix model targeting smoke and fire detection on Qualcomm QCS6490 HTP.

## Specifications

- Model ID: `yolo11n_fire_smoke`
- Input: `1x320x320x3` NHWC uint16 (quantized scale `1.5259021893143654e-05`, offset `0`)
- Outputs:
  - `boxes_out`: `1x4x2100` uint16 (quantized scale `0.013475208543241024`, offset `-2783`)
  - `conf_out`: `1x2x2100` uint16 (quantized scale `1.52587890625e-05`, offset `0`)
- Classes: `smoke` (0), `fire` (1)
- Decoder contract: `yolo.v8.fire_smoke`
- Graph Name: `yolo11n_replaymix_w8a16`
- Target: Qualcomm QCS6490 HTP (W8A16)
