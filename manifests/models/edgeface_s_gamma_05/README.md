# EdgeFace-S (gamma 0.05) embedding package (M0)

Neutral metadata for the secondary face embedding model. The `.so` artifact stays outside
Git; this directory holds only metadata and (later) small privacy-safe golden references.

| File | Purpose |
|---|---|
| `model_metadata.json` | identity, graph name, artifact ref + SHA-256 (metadata, not signature verification) |
| `io_manifest.json` | declared input/output tensor identity from the QCS6490 runtime probe |
| `preprocess.json` | declared color/normalization of the aligned 112x112 tensor |
| `decoder.json` | embedding decoder contract: output tensor/dimension, landmark alignment template, destination color policy |
| `golden/` | aligned crop, input tensor and embedding reference (pending) |

Artifact: `libedgeface_s_gamma_05_w8a16_ada_w8a16.so`, SHA-256
`6faf62d17323002ce028d984826abf8d52f06e222e8a6c4c52cea3be53e54815`, 4725832 bytes, W8A16,
board path `/var/roothome/ai_app_dsp/models/libedgeface_s_gamma_05_w8a16_ada_w8a16.so`
(QAIRT 2.43.0).

## Runtime-reported ABI (QCS6490 HTP probe)

- input `input` `[1,112,112,3]` NHWC, `uint16`, scale `3.05180438e-05`, zero_point `32768`;
- output `embedding` `[1,512]`, `uint16`, scale `4.08594024e-05`, zero_point `12899`,
  1024 bytes.

## Confirmed by the model team

- Source: Camera FW RAW is **NV12 / BT.709 limited**; the model tensor is **RGB uint16 NHWC**.
- Input (112x112): `q = round(normalized / input_scale) + 32768`, with `input_scale =
  3.05180438e-05` (input zero_point `32768`).
- Output: uint16 UFXP16, 512 dimensions, `float_embedding = (q - 12899) * 4.08594024e-05`
  (zero_point `12899`, scale `4.08594024e-05`).
- Offline JPEG/PNG path: BGR/RGB image -> RGB -> letterbox 640x640 -> float normalization ->
  quantize to uint16 by scale/zero_point.

These match the recorded ABI and the `fastcv_aligner` RGB + quantize and the
`embedding_decoder` dequantize paths.

## Not usable yet (M4/M5)

- `decoder.json` now declares the embedding kind (output tensor `embedding`, dimension 512,
  min norm, 5-point `face.5pt` alignment template to 112×112, RGB/BT.709-limited), but
  production does not yet prepare an embedding graph/decoder or consume the template; do not
  bind this model in a production catalog until secondary composition lands.
- Alignment (5-point similarity transform to 112x112) is delivered in
  `fastcv_aligner`; the aligned RGB must still be quantized to this model's uint16 input.
- Embeddings are sensitive biometric data. Keep them out of logs, Git and CI artifacts.

## Intended catalog entry (M5, schema v2)

When the embedding decoder lands, this model is a **secondary** catalog entry that depends
on the SCRFD primary identity (it never joins the full-frame submit mask):

```json
{
  "model_id": "edgeface_s_gamma_05",
  "role": "secondary",
  "depends_on": [
    { "model_id": "scrfd_500m_bnkps", "model_version": "1.0", "target_id": "qcs6490" }
  ]
}
```

## Policy

No model binary, biometric data or private SDK in Git. A digest match is not artifact
authentication.

## See also

- [Cascade inference](../../../docs/architecture/cascade_inference.md),
  [face recognition plan](../../../docs/planning/face_recognition_completion_plan.md)
