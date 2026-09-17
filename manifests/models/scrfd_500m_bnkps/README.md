# SCRFD-500M-KPS face detector package (M0)

Neutral metadata for the primary face detector. The `.so` artifact stays outside Git; this
directory holds only metadata and (later) small privacy-safe golden references.

| File | Purpose |
|---|---|
| `model_metadata.json` | identity, graph name, artifact ref + SHA-256 (metadata, not signature verification) |
| `io_manifest.json` | declared input/output tensor identity from the QCS6490 runtime probe |
| `preprocess.json` | declared preprocess (letterbox, RGB, [-1,1] normalization) |
| `decoder.json` | `anchor_distance` decoder package: tensors, strides, anchors, landmarks |
| `golden/` | expected input tensor and raw outputs from the reference pipeline (pending) |

Artifact: `libscrfd_500m_bnkps_w8a16.so`, SHA-256
`2f315dcd1996b0280c162e9bf79f0e1e64123675d4be2553ef698328dea103d9`, 1075944 bytes, W8A16,
board path `/opt/lacai/models/libscrfd_500m_bnkps_w8a16.so` (QAIRT 2.43.0).

- **Status:** metadata delivered; artifact/golden not in repository

## Runtime-reported ABI (QCS6490 HTP probe)

- input `input_1` `[1,640,640,3]` NHWC, `uint16`, scale `3.03988327e-05`, zero_point `32768`;
- `score_8/16/32` `[1,{12800,3200,800},1]`, `uint16`, scale `1.52587891e-05`, zero_point `0`;
- `bbox_8` `[1,12800,4]` scale `7.81242052e-05` zp `1722`; `bbox_16` `[1,3200,4]` scale
  `9.55337309e-05` zp `0`; `bbox_32` `[1,800,4]` scale `8.25099996e-05` zp `20`;
- `kps_8` `[1,12800,10]` scale `9.88173124e-05` zp `31030`; `kps_16` `[1,3200,10]` scale
  `0.000124453567` zp `30927`; `kps_32` `[1,800,10]` scale `0.000104254454` zp `32151`;
- two anchors per cell at strides 8/16/32 (`12800 = 80²·2`, `3200 = 40²·2`, `800 = 20²·2`).

## Open questions before M1 closes

- Golden decode parity on real tensors (input tensor, raw scores/bboxes/kps and expected
  detections) is not available yet; this package is metadata only.
- The upstream SCRFD inference reference establishes top-left zero padding,
  `anchor_offset_cells` `0.0`, two anchors per cell, stride-scaled bbox/keypoint distances,
  and `(pixel - 127.5) / 128.0` normalization. Score semantics, landmark ordering,
  thresholds (confidence `0.5`, NMS IoU `0.4`) and exact tensor parity still require golden
  confirmation for this converted artifact.
- `max_candidates` is set to the decoder ceiling `4096`; the overflow policy for crowded
  scenes (fault vs bounded top-score selection) is still open (M1).
- The model binary stays outside Git. A digest match is not artifact authentication.

## See also

- [Cascade inference](../../../docs/architecture/cascade_inference.md),
  [face recognition plan](../../../docs/planning/face_recognition_completion_plan.md)
