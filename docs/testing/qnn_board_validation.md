# QNN model and engine board validation

This document defines how to validate the AI-team model artifacts and the LACAI-owned QNN
engine on the QCS6490 target before model/usecase integration. It records the inputs to
pin, the validation steps, the evidence checklist and the dated board result.

**Status:** board-smoke — 2026-09-14 board run passed runtime sanity, model smoke and owned
engine execution with engine-versus-runtime parity; accuracy and latency acceptance are not
claimed. **Layer:** reference. **Source:** `n/a`.

Purpose: validate the AI-team model artifacts and the LACAI-owned QNN engine on the
QCS6490 target before model/usecase integration. This produces evidence; it is not
acceptance and does not replace the FW release gates. The board is reachable as
`lacai-qsc6490` per [board target](qsc6490_board.md) when online.

## Responsibility

- Defines the board procedure for QNN runtime sanity, model smoke, numeric checks and the
  LACAI-owned engine.
- Records artifact/digest and parity evidence for the agreed workload.
- Must not be read as model accuracy, released-FW, performance or thermal acceptance.

## Inputs to pin

| Input | Value observed | Note |
|---|---|---|
| Target | QCS6490 RB3 Gen2 Vision Kit, Qualcomm Linux 1.8, HTP v68 | user-confirmed |
| QAIRT | 2.43.0.260128 (`third_party/qairt`) | model generation also seen at 2.35.0.250530; pin the runtime that matches the artifact when possible |
| SCRFD | `libscrfd_500m_bnkps_w8a16.so` | 2.43 build exists in the legacy app; 2.35 build in `/home/a/Downloads/qnn_qcs6490` |
| EdgeFace | `libedgeface_xxs_w8a16_ada.so` | 2.35 build in `/home/a/Downloads/recognition/qnn_so` |
| Inputs | `input.1` `[1,3,640,640]` SCRFD; `input` `[1,3,112,112]` EdgeFace | W8A16 exposes UFIXED_POINT_16 I/O; QNN applies NHWC layout |

Models are not committed. Copy the `.so` (weights are embedded) plus a raw input list for
the smoke test. Build/convert provenance must be recorded with the artifact.

## Step 1 — runtime sanity

```bash
export QAIRT_ROOT=/root/qairt/2.43.0.260128
export QNN_TARGET=aarch64-oe-linux-gcc11.2
export LD_LIBRARY_PATH=$QAIRT_ROOT/lib/$QNN_TARGET:$LD_LIBRARY_PATH
export ADSP_LIBRARY_PATH="$QAIRT_ROOT/lib/hexagon-v68/unsigned;/usr/lib/rfsa/adsp;/lib/rfsa/adsp;/dsp"
qnn-platform-validator --backend htp
```

Required: HTP backend reports supported. A missing skel path or a runtime/library version
mismatch must be fixed here before any engine work.

## Step 2 — model smoke with qnn-net-run

Use [tools/vqec_vision_qnn_board_smoke.sh](../../tools/vqec_vision_qnn_board_smoke.sh):

```bash
tools/vqec_vision_qnn_board_smoke.sh \
  --qairt "$QAIRT_ROOT" \
  --model /path/libscrfd_500m_bnkps_w8a16.so \
  --input-list /path/eval_inputs/input_list.txt \
  --output-dir /tmp/lacai_qnn_smoke/scrfd
```

Expected: SCRFD writes `score_8/16/32`, `bbox_8/16/32`, `kps_8/16/32` raw tensors; EdgeFace
writes `embedding.raw`. Shape/dtype/quantization must match the model `net.json`
(`UFIXED_POINT_16`, scale/offset per tensor).

## Step 3 — numeric check

- SCRFD: dequantize with the tensor's scale/offset, run the anchor decode and NMS, and
  compare a few boxes/scores/landmarks against the Python reference on the same raw input.
- EdgeFace: dequantize `embedding` (scale ~1.3505e-05, offset -27404), L2-normalize and
  compare cosine similarity for same/different identity pairs.
- Record the exact raw input, digest, and resulting tensors. A successful `qnn-net-run`
  alone is not a quality result.

## Step 4 — LACAI engine

Cross-build with the eSDK and the engine enabled:

```bash
source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux
cmake -S . -B build-esdk-qnn -DVQEC_VISION_AI_ENABLE_QNN_ENGINE=ON \
  -DVQEC_VISION_AI_QAIRT_ROOT=/opt/qcom/aistack/qairt/2.43.0.260128
cmake --build build-esdk-qnn -j"$(nproc)"
```

On the board, point the adapter at the image runtime and the copied model library. The owned
engine creates the backend/device, composes and finalizes a single-graph model library,
executes synchronously (client buffers or ION-registered outputs), supports `release_model`
reload, and is bound to the runtime through `inference_graph_port`; the
`vqec_ai_vision_applications` service runs it on `.98`. Async/shared execution and released-FW
DMA completion remain open. Validate a model path with step 2, or the full service with the
production command in [QSC6490 target](qsc6490_board.md).

## Troubleshooting

- `contextCreateFromBinary`/compose failure: runtime older or newer than the model build;
  align QAIRT or regenerate the model.
- Missing `libQnnHtpV68Skel.so`: fix `ADSP_LIBRARY_PATH`.
- Unexpected dtype: W8A16 graphs expose `UFIXED_POINT_16`; the neutral mapping is
  `uint16` plus explicit quantization. Do not treat raw values as float.
- Layout: QNN reports physical NHWC dimensions; use the graph tensor metadata, not the
  ONNX NCHW document.

## Evidence checklist

- Board image/commit, QAIRT version, `qnn-platform-validator` output.
- Model artifact ref + digest, convert/build script revision.
- Raw input digest, expected/actual tensor names/shape/dtype/quantization.
- Dequantized golden comparison and tolerances.
- Copies/latency/memory for the agreed workload once the engine executes.

## 2026-09-14 result (board online)

- Step 1: `qnn-platform-validator --backend dsp --testBackend` → unit test **Passed**,
  Hexagon V68. Image QAIRT runtime is 2.43.0.
- Step 2: `qnn-net-run` on SCRFD-500M-KPS and YOLOv8n-person wrote the expected tensors.
- Step 4: the LACAI-owned engine executed both models on HTP via
  `vqec_vision_ai_qnn_engine_smoke` (backend `/usr/lib/libQnnHtp.so`, system
  `/usr/lib/libQnnSystem.so`). This required a source fix: call `graphFinalize` after
  `composeGraphs` and before `graphExecute`.
- Step 3 (partial): engine-versus-runtime parity on the same native input is byte-identical
  (SCRFD 9/9, YOLOv8n 2/2). Labelled accuracy, async/shared/update, latency/memory and a
  live FW stream are still not done.

## Limits and next work

- Labelled accuracy, async/shared/update, latency/memory capture and a live FW stream remain
  undone; byte-identical engine-versus-runtime parity is not labelled accuracy.
- Async/shared execution and released-FW DMA completion remain open.

## See also

- [QCS6490 board test target](qsc6490_board.md)
- [eSDK configuration and evidence matrix](esdk_configuration_matrix.md)
