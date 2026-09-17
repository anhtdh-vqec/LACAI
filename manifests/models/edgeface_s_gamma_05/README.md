# EdgeFace-S (gamma=0.5, artifact token `gamma_05`) embedding package (M0)

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

- **Status:** metadata delivered; artifact/golden not in repository

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
- The EdgeFace reference preprocessing is `ToTensor()` followed by
  `Normalize(mean=0.5, std=0.5)`, equivalent to `pixel / 127.5 - 1.0` for an RGB8 pixel.

The tensor ABI matches the `fastcv_aligner` RGB output, the cascade quantization path and
the `embedding_decoder` dequantization path. Golden tensor and embedding parity remain
required for this converted artifact.

## Integration status (M4/M5)

- `decoder.json` declares the embedding kind (output tensor `embedding`, dimension 512,
  min norm, 5-point `face.5pt` alignment template to 112×112, RGB/BT.709-limited).
- The model is a dependency-activated secondary catalog entry. Production prepares its QNN
  graph, embedding decoder and FastCV alignment adapter, while `cascade_graph_session` owns
  graph start/drain/unload and the runtime executor invokes the coordinator only for the
  declared primary dependency.
- The delivered cascade path performs similarity alignment, RGB8-to-model-input
  normalization/quantization from validated package and graph metadata, synchronous model
  submission, embedding decode and exact retained-frame completion.
- This is source integration, not model acceptance. Golden crop/input/embedding parity and
  an end-to-end camera run on the target remain required before enabling recognition output.
- Embeddings are sensitive biometric data. Keep them out of logs, Git and CI artifacts.

## Catalog entry (schema v2)

This model is a **secondary** catalog entry that depends on the SCRFD primary identity. It
never joins the full-frame submit mask:

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
