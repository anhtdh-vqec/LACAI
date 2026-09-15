# EdgeFace-S (gamma 0.05) embedding package (M0)

Neutral metadata for the secondary face embedding model. The `.so` artifact stays outside
Git; this directory holds only metadata and (later) small privacy-safe golden references.

| File | Purpose |
|---|---|
| `model_metadata.json` | identity, graph name, artifact ref + SHA-256 (metadata, not signature verification) |
| `io_manifest.json` | declared input/output tensor identity from the QCS6490 runtime probe |
| `preprocess.json` | declared color/normalization of the aligned 112x112 tensor |
| `decoder.json` | **not present yet** — the embedding decoder contract is M5 work |
| `golden/` | aligned crop, input tensor and embedding reference (pending) |

Artifact: `libedgeface_s_gamma_05_w8a16_ada_w8a16.so`, SHA-256
`6faf62d17323002ce028d984826abf8d52f06e222e8a6c4c52cea3be53e54815`, 4725832 bytes, W8A16,
board path `/var/roothome/ai_app_dsp/models/libedgeface_s_gamma_05_w8a16_ada_w8a16.so`
(QAIRT 2.43.0).

## Runtime-reported ABI (QCS6490 HTP probe)

- input `input` `[1,112,112,3]` NHWC, `uint16`, scale `3.05180438e-05`, zero_point `32768`;
- output `embedding` `[1,512]`, `uint16`, scale `4.08594024e-05`, zero_point `12899`,
  1024 bytes.

## Not usable yet (M4/M5)

- No `decoder.json`: the typed embedding decoder (dimension/finite checks, L2 normalization,
  model/version binding, stale-result rejection) is not implemented. Do not bind this model
  in a production catalog until M5 lands.
- Alignment (5-point similarity transform to 112x112) and the landmark template are M4
  work; `preprocess.json` declares only the color/normalization of the aligned tensor.
- Embeddings are sensitive biometric data. Keep them out of logs, Git and CI artifacts.

## Policy

No model binary, biometric data or private SDK in Git. A digest match is not artifact
authentication.

## See also

- [Cascade inference](../../../docs/architecture/cascade_inference.md),
  [face recognition plan](../../../docs/planning/face_recognition_completion_plan.md)
