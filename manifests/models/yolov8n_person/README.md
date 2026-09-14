# YOLOv8n-person approved model kit (MI-02)

Neutral metadata for the first model integration target. The `.so` artifact stays outside
Git; this directory holds only metadata and (later) small golden references.

| File | Purpose |
|---|---|
| `model_metadata.json` | identity, graph name, artifact ref + SHA-256 (metadata, not signature verification) |
| `io_manifest.json` | declared input/output tensor identity (name, rank, dims, layout, dtype, quantization) |
| `preprocess.json` | authoritative preprocess spec (color matrix/range, resize, pad, normalization, coordinates) |
| `decoder.json` | decoder contract, thresholds, tensor names, open format questions |
| `labels.txt` | class labels (one `person`) |
| `golden/` | expected input tensor and raw outputs from the reference pipeline (pending) |

Artifact: `libyolov8n_person_w8a16.so`, SHA-256
`99ee47cdf02f024bafefe066234d9b7e9e04e87e38ef8f3afd0eb77495370956`, 3666216 bytes,
built W8A16.

Facts taken from `yolov8n_person_w8a16_net.json` and confirmed on QCS6490 (`qnn-net-run`
and the owned QNN engine are byte-identical for the same native input):

- input `images` `[1,640,640,3]` (QNN physical NHWC), `uint16`, scale `1.5259021893143654e-05`, zero_point `0`;
- output `boxes_out` `[1,4,8400]`, `uint16`, scale `0.01038312166929245`, zero_point `0`;
- output `conf_out` `[1,1,8400]`, `uint16`, scale `1.52587890625e-05`, zero_point `0`;
- `8400 = 80² + 40² + 20²` (strides 8/16/32), one class, DFL present, no NMS in graph.

## Confirmed by the model team

- `boxes_out` is `xywh` (`centre_x,centre_y,width,height`), channel-first `[1,4,8400]`, in
  the 640×640 letterbox tensor space. The decoder applies inverse letterbox to the source.
- `conf_out` is a sigmoid probability.

## Open questions before MI-03 closes

- Preprocess color matrix (`bt709` assumed), range, pad value (114 assumed), interpolation
  and RGB vs BGR must be confirmed against a golden reference.
- Runtime/compiler version: models observed built with QAIRT 2.35 and 2.43; pin the runtime
  that matches the artifact and record it.

## Policy

No model binary, biometric data or private SDK in Git. Golden files must be small and
privacy-safe. A digest match is not artifact authentication.
