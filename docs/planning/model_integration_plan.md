# Model Integration plan — M0 → M4 (YOLOv8n-person first)

Status: proposed backlog to follow. The device-free basecode is frozen at the architecture
level; this plan adds model packages, not runtime redesign. Reference:
[device-free basecode plan](../development/LACAI_DEVICE_FREE_BASECODE_PLAN.md) (done) and
[capability matrix](../development/capability_matrix.md).

First target model: **YOLOv8n-person** from
`/home/a/Workspace/AI APPLICATIONS/QCS6490/yolov8n_person_qnn_src`. Board evidence already
shows the owned QNN engine executes it on HTP with outputs byte-identical to `qnn-net-run`.

## 1. Frozen basecode (do not redesign)

Already closed and relied on by this plan: per-source session worker isolation, bounded
inference primitives, recovery/backoff, health/fault channel, QoS mailbox, secondary
scheduler foundation, dense decoder base, reference tracker/feature/output, artifact SHA-256
+ resolver, production fake composition E2E, and real QNN HTP execution with runtime parity.
CMake is modular with `-Wall -Wextra -Wpedantic -Werror`, ASan/UBSan option and four fuzzers.

Not a reason to touch the scheduler/runtime per model: a new model must be
`artifact + manifest + preprocess spec + decoder + golden tests`.

## Progress

| Step | Status | Note |
|---|---|---|
| MI-00 | **done** | `preprocess_spec`, `model_io_manifest`, `tensor_layout` on `tensor_spec` + validators + tests. Catalog/resolver/processor wiring still to do in MI-01/MI-02. |
| MI-01 | **done** | `resolved_model_package` + pure resolver (`vqec_vision_ai_core_mpkg_resolve`): catalog identity, graph agreement, single-input base, preprocess validity, trusted-path identity. Trusted path/digest resolution reuses the existing `mreg` artifact resolver at MI-02/MI-07. |
| MI-02 | **done** | YOLOv8n-person kit metadata under `manifests/models/yolov8n_person/` (identity + SHA-256, IO manifest, preprocess spec, decoder contract, labels, golden placeholder). Loaders land with MI-05. |
| MI-03 | partial | reference processor now honors `preprocess_spec` (color matrix/range, pad, interpolation, normalization formula, channel order) with spec-driven tests; golden comparison against the reference pipeline pending model-team data. |
| MI-04 | partial | `yolov8_decoder` implemented and tested device-free (channel-first xywh dequant, inverse letterbox, clip, per-class NMS, threshold, bound, missing/mismatch/dtype/NaN negatives). Confirmed box format/space from the model team. Golden decoded parity pending reference detections. `dense_decoder` is not reused (layout differs). |
| MI-05 | **done** | `vqec_vision_model_runner` (board tool): reads the package io_manifest/preprocess/decoder JSON, opens the owned QNN engine, cross-checks declared input identity against the graph, preprocesses an NV12 fixture, executes, decodes with `yolov8_decoder` and prints/emits a JSON report. Runtime validation on board is MI-06. |
| MI-06 | partial | Board: runner ran the real package end to end, and raw outputs through the real preprocess path are byte-identical to `qnn-net-run` (2/2). M2 preprocess golden and M4 decoded golden still need the model team reference data; M1 declared-vs-graph identity check is implemented in the runner. |
| MI-07 | todo | Qualcomm production platform owner; `--platform qualcomm` still fails closed. Reordered after MI-08. |
| MI-08 | **done (device-free)** | `reference_platform` owner (`--platform reference`) wires the real `reference_tracker` and `reference_zone_feature` behind the neutral ports with the shared `fixture_detector`. Integration exposed and fixed two bugs: the reference tracker left detections untracked on a source gap, and the reference feature stamped events with the monotonic step clock instead of the source PTS domain. `service_production_reference_smoke` asserts routing, at least one delivered zone event and a clean stop. See [reference platform](../architecture/reference_platform.md). |
| MI-09..MI-11 | todo | see commit order |

## 2. Gate 0 — three contract fixes before the first model

These are the only architecture changes before model integration.

### G0-1 · Authoritative preprocess contract

The current `model_catalog_entry` preprocess fields are incomplete and one is misleading.

Current fields: `tensor_width_`, `tensor_height_`, `input_type_`, `channel_order_`,
`placement_`, `mean_`, `sigma_`.

Missing / to fix:

- input tensor name, layout identity, exact rank/dims, input quantization contract;
- color matrix (BT.601 / BT.709) and range (limited / full) as data;
- resize mode, interpolation, pad value, rotation;
- normalization formula made explicit;
- coordinate convention (normalized vs tensor pixels; xywh vs xyxy; letterbox inverse).

Known concrete bug: the service harness declares `source_color_profile::bt709_limited`
while `reference_image_processor` hardcodes BT.601-limited coefficients
(`luma = 1.164*(Y-16)`, `R = luma + 1.596*V`, `G = luma - 0.391*U - 0.813*V`,
`B = luma + 2.018*U`). This silently degrades accuracy. Colorimetry must come from data.

Rename the misleading `mean_`/`sigma_` semantics: the reference processor computes
`(pixel - mean) * sigma`, i.e. an affine `real = (pixel - offset) * scale`. Use explicit
`offset`/`scale` (or `mean`/`std` with the exact formula) so no integrator reads `sigma` as
standard deviation.

Target shape (names may change; every assumption must become data):

```text
preprocess_spec {
  source_pixel_format
  color_matrix            // bt601 | bt709 | ...
  color_range             // limited | full
  resize_mode             // letterbox | stretch | crop
  interpolation           // nearest | bilinear | ...
  placement               // centre | top_left | ...
  pad_value[3]
  channel_order           // rgb | bgr
  normalization_formula   // explicit: real = (pixel - offset) * scale
  offset[3], scale[3]
  coordinate_convention   // xywh_tensor_px | xyxy_tensor_px | normalized ...
}
```

### G0-2 · Symmetric input/output tensor manifest

Output identity is already validated. Input identity is currently taken straight from the
graph (`graph.get_input_specs(inputs); target_specs_[slot] = inputs[0];`), which is good for
execution but not for package/artifact authentication.

Introduce a symmetric contract and reference it from the catalog:

```text
model_io_manifest {
  inputs[]  { name, rank, dims, layout, dtype, quantization{scale, zero_point} }
  outputs[] { name, rank, dims, layout, dtype, quantization{scale, zero_point} }
}
```

Activation must check `package-declared IO == actual QNN graph metadata` and fail closed on
any mismatch (tensor name, rank, dims, layout, dtype or quantization). Do not accept "one
input and the shape looks right".

Bump the catalog schema now, before any production package exists. `model_catalog_entry`
gains `io_manifest_ref` (and `preprocess_ref` / graph name); do not stuff IO identity into
the catalog entry itself.

### G0-3 · Production composition loads a real model package

Today `service_main` builds `reference_inference_graph`, synthesizes
`vqec_vision_ai_appl_svcmn_synthetic_outputs`, and hardcodes model/backend/system paths.
`--mode production --platform qualcomm` fails closed because no Qualcomm owner exists.

Target owners:

```text
model_catalog_entry
      ↓
model_package_resolver     // verify artifact, load IO manifest + preprocess spec,
      │                    //   trusted path + decoder contract resolution
      ▼
resolved_model_package
      ▼
platform_model_factory     // image_processor + inference_graph + decoder
      ▼
runtime_model_owner        // neutral; service_mains only wires this
```

`service_main` must not know `libQnnHtp.so`, `libQnnSystem.so`, a decoder class or a `.so`
path. Those belong to the platform/model package owner.

## 3. First model — YOLOv8n-person (concrete facts)

Artifact: `yolov8n_person_w8a16.so` (weights embedded), built W8A16.
Source net metadata: `yolov8n_person_w8a16_net.json`.

| Tensor | Rank/dims (QNN physical) | QNN dtype | Quantization |
|---|---|---|---|
| `images` (input) | `[1,640,640,3]` (ONNX NCHW `[1,3,640,640]`) | UFIXED_POINT_16 (uint16) | scale `1.5259021893143654e-05`, offset `0` |
| `boxes_out` | `[1,4,8400]` | UFIXED_POINT_16 (uint16) | scale `0.01038312166929245`, offset `0` |
| `conf_out` | `[1,1,8400]` | UFIXED_POINT_16 (uint16) | scale `1.52587890625e-05`, offset `0` |

- `8400 = 80² + 40² + 20²` → anchor-free strides 8/16/32, one class (person).
- The graph contains DFL (`dfl_Softmax`) and a `dfl_conv`, so `boxes_out` is already
  DFL-decoded 4-value boxes; `conf_out` is already sigmoid probabilities.
- **No NMS in the graph** (8400 raw anchors) → the decoder must threshold + NMS.
- **Open question to settle at MI-04:** whether `boxes_out` is `x,y,w,h` or
  `x1,y1,x2,y2`, and in which pixel space (letterbox 640), by dumping golden outputs and
  comparing a known detection. Do not assume.
- Input quantization: `real = (stored - zero_point) * scale` with `zero_point = -offset = 0`,
  i.e. `stored = round(real / scale)`. This matches an Ultralytics-style normalization to
  `[0,1]` followed by `/scale` (scale ≈ 1/65536).
- Pad value: Ultralytics default is 114 (gray); confirm with the model team and make it
  data in `preprocess_spec.pad_value`, never a literal.

## 4. Gates M0 → M4

### M0 — Package completeness
A model package exists with artifact identity, IO manifest, preprocess spec, decoder
contract, labels and golden references. Acceptance: activation rejects a wrong input name
(`images → input_0`), a wrong rank/dims, and a changed quantization (`0.01038 → 0.01040`)
**before** load/submit, with a clear reason.

### M1 — Load + metadata
Owned QNN engine composes + finalizes the graph; actual graph metadata is compared to the
declared IO manifest. Acceptance: match loads; any mismatch fails activation. Board:
`soc_id`, backend/system/library versions, artifact digest recorded.

### M2 — Preprocess golden
`NV12 fixture → reference_image_processor → tensor` compared to the model team's expected
input tensor. Acceptance: tensor digest (or tolerance) matches for landscape, portrait,
letterbox, odd aspect ratio and border objects. This makes the CPU processor the oracle for
the later Qualcomm/FastCV implementation.

### M3 — Raw output parity
`REAL SOURCE IMAGE → LACAI preprocess → LACAI QNN → raw outputs` compared to the reference
pipeline. Acceptance: raw tensors match within the agreed tolerance; the earlier
`qnn-net-run` byte-identical parity remains a sanity check.

### M4 — Decoded output parity
A model-specific decoder turns saved raw outputs into `observation_batch`; golden detections
match (class, confidence, bbox, reverse-mapped coordinates, NMS, threshold). Negative cases:
missing tensor, wrong tensor name, wrong dtype, wrong shape, NaN/Inf, too many candidates,
bbox outside frame.

## 5. Commit order to follow

```text
MI-00  preprocess/input contract v2 (preprocess_spec + model_io_manifest + catalog v2)
MI-01  model_package_resolver + resolved_model_package (trusted paths, digest, IO)
MI-02  approved YOLOv8n-person model kit metadata + golden references (binary stays out of Git)
MI-03  preprocess golden tests (M2)
MI-04  yolov8_decoder (or reuse dense_decoder only if shapes/format actually match) + golden (M4)
MI-05  vqec_vision_model_runner tool (fixture NV12 → preprocess → QNN → decoder → JSON)
MI-06  M0–M4 board acceptance on QCS6490
MI-07  Qualcomm production platform owner; `--mode production --platform qualcomm` works
MI-08  tracker + first feature (M5)
MI-09  live FW camera (M6 input path)
MI-10  optimized Qualcomm preprocess
MI-11  multi-camera / performance
```

MI-05 runner signature (concept; fixture only, no camera/FW ring/tracking/feature/encoder):

```text
vqec_vision_model_runner \
  --model-package person_detector.json \
  --input frame_001.nv12 \
  --output detections.json
```

Report sections: `preprocess` (PASS, bytes, latency), `qnn` (PASS, tensor names, raw
parity), `decoder` (PASS, detections).

## 6. Model kit layout (MI-02)

```text
yolov8n-person/
  model_metadata.json        // model_id, version, graph name, artifact ref + SHA256
  io_manifest.json           // inputs[] / outputs[] identity (G0-2)
  preprocess.json            // preprocess_spec (G0-1)
  decoder.json               // decoder contract + class labels + thresholds
  labels.txt
  golden/
    frame_001.nv12
    expected_input_tensor.bin
    raw_boxes_out.bin
    raw_conf_out.bin
    expected_detections.json
```

Model binary stays outside Git (repo policy). Golden files must be small and free of
personal data.

## 7. Non-goals for M0–M4

Do not wait for or require: QNN async, registered/shared memory, perfect tensor-pool
wiring, FastCV optimization, secondary inference, hardware overlay/encoder, 16-camera
scaling, thermal or 24h soak. The synchronous QNN correctness baseline is enough to lock
model semantics, preprocess, parity and decoder correctness first.

## 8. Parallel fix (does not block M0–M4)

Camera release path: EAGAIN now retains the token for retry, but the final frame destructor
still reaches `send()` indirectly via `release_completion() → flush_releases()`. Also
`release_completion()` is `noexcept` while `std::deque::push_back()` can allocate/throw
(terminate on OOM), and overflow currently `clear()`s all pending tokens. Replace with a
fixed-capacity preallocated release mailbox before live FW. Track separately; not a model
integration blocker.

## 9. Risks / open questions

- `boxes_out` format and pixel space are unconfirmed (see MI-04); resolve with golden data.
- Preprocess pad value, RGB vs BGR and exact normalization must be confirmed with the model
  team; the plan treats them as data, so a wrong value is a config fix, not code.
- Models built with QAIRT 2.35 vs the board runtime 2.43: pin the runtime that matches the
  artifact and record it in the package.
- `dense_decoder` only matches a per-stride grid `cx,cy,w,h` layout; the flat `[1,4,8400]`
  `boxes_out` likely needs a dedicated `yolov8_decoder`.
