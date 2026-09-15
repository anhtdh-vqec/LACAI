# Model package verification checklist

One checklist to hand to the model team and to close M0 for any model. It is generic: copy
the table for each new model package and fill it from golden/reference data. Values marked
**assumed** are not verified and must not be treated as acceptance. A digest match is not
signature verification.

## How to verify

- Every "assumed" field needs a golden input tensor and raw outputs from the model team's
  reference pipeline, plus the expected decoded result with an explicit tolerance.
- Golden data is permitted-use and privacy-safe; it lives outside Git (see each package's
  `golden/README.md`). No model binary, biometric sample or real embedding in Git.
- Record provenance (artifact SHA-256, byte size, QAIRT/model-generator version, graph name,
  export/training reference) for the exact delivered revision.
- Cross-check the runtime-reported ABI (owned QNN engine probe) against `io_manifest.json`
  before trusting any threshold or ordering.

## Generic per-model fields

| Field | Check |
|---|---|
| Artifact identity | SHA-256, byte size, exact file name/revision delivered |
| Toolchain provenance | QAIRT/runtime version, model-generator version, export/training reference |
| Graph name / variant | matches the catalog `graph_name`; correct model variant |
| Input ABI | name, dtype, rank, dims, layout, per-tensor quantization |
| Output ABI | count, names, dtype, rank, dims, layout, per-tensor quantization |
| Preprocess | color matrix/range, RGB/BGR, resize mode, interpolation, pad value, normalization formula/offset/scale, coordinates |
| Decode semantics | score activation, box format/units/order, anchor ordering/offset, grid, landmark ordering/units, thresholds, NMS policy, overflow policy |
| Cadence / resource envelope | inference cadence, resident bytes, tensor bytes |
| Golden parity | input tensor, raw outputs, decoded result within tolerance |

## Face chain instance (M0)

### SCRFD-500M-KPS (`manifests/models/scrfd_500m_bnkps/`)

| Field | Current value | Status | How to verify |
|---|---|---|---|
| Artifact SHA-256 / bytes | `2f315dcd…03d9` / 1075944 | observed on `.48` | confirm same revision delivered |
| Runtime | QAIRT 2.43.0 | observed | pin exact runtime used for export |
| Input | `input_1` `[1,640,640,3]` NHWC uint16, scale `3.03988327e-05`, zp `32768` | observed | model team confirms export ABI |
| Outputs | 9 tensors `score_{8,16,32}`, `bbox_{8,16,32}`, `kps_{8,16,32}` with per-tensor scale/zp in `io_manifest.json` | observed | confirm names/quantization |
| Score semantics | probability (assumed sigmoid already applied) | **assumed** | golden score vs reference |
| Bbox format | left/top/right/bottom distances in **stride units** | **assumed** | golden bbox vs reference |
| Anchor ordering / offset | 2 anchors/cell, centre offset `0.5` | **assumed** | golden decode vs reference |
| Grids / strides | 80×80@8, 40×40@16, 20×20@32 | observed (counts) | confirm grid order |
| Landmarks | 5 points × (x,y) in stride units; order eye/nose/mouth | **assumed** | confirm point order vs reference |
| Thresholds | confidence `0.5`, NMS IoU `0.4`, per-class | **assumed** | calibrate on golden |
| Preprocess | NV12, BT.709 limited, RGB, letterbox, bilinear, pad `0`, `(x-127.5)/128` | **assumed** | golden input tensor parity |
| Overflow | `max_candidates = 4096` (decoder ceiling) | open policy | decide fault vs top-score truncation |

### EdgeFace-S gamma 0.05 (`manifests/models/edgeface_s_gamma_05/`)

| Field | Current value | Status | How to verify |
|---|---|---|---|
| Artifact SHA-256 / bytes | `6faf62d1…4815` / 4725832 | observed on `.48` | confirm same revision delivered |
| Input | `input` `[1,112,112,3]` NHWC uint16, scale `3.05180438e-05`, zp `32768` | observed | confirm export ABI |
| Output | `embedding` `[1,512]` uint16, scale `4.08594024e-05`, zp `12899` | observed | confirm dimension/quantization |
| Normalization | `(x-127.5)/128` (assumed) | **assumed** | golden input tensor parity |
| Alignment template | 5-point reference points, 112×112 destination, similarity transform | **open (M4)** | model team provides template; verify warp parity |
| Embedding postprocess | L2 normalization, finite/dimension checks, model-version binding | **open (M5)** | golden embedding parity and norm |
| Golden | aligned crop, input tensor, raw embedding, normalized embedding | missing | permitted-use references |

## Definition of done for a model

Package verification is **stage 1 only**; it is necessary but not sufficient. Closable
stages, each with its own evidence and owner (see ADR 0005 section 6):

1. **Package verification** — every row below is observed-and-confirmed or golden-verified,
   and the package parses through the strict loaders;
2. **Board model execution** — the exact artifact executes on QCS6490 with recorded
   parity/limits;
3. **FD→FR correlation** — the retained frame maps to the correct face/embedding per epoch,
   frame and track;
4. **Accuracy/calibration** — thresholds calibrated and measured on a representative
   dataset;
5. **Attendance/usecase acceptance** — enrollment, matching, delivery, restart recovery and
   delete accepted end to end.

Only stage 5 is usecase acceptance. Metadata alone, or a single successful execution, never
closes a later stage.
