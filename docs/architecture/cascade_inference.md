# Cascade inference for detection and embeddings

Execution plan: [SCRFD + EdgeFace + Zvec completion](../planning/face_recognition_completion_plan.md).

Status: primary decoder production selection, typed contracts and retained-frame primitive
are source-delivered. Model execution probes pass; secondary alignment/graph composition,
gallery recovery and attendance remain open. The primitive is not yet connected to the pump.

## Why the current full-frame fan-out is insufficient

`multi_model_pump` is correct for independent models that consume the same full source
frame. Face recognition is dependent work: face detection first produces a box and five
landmarks, then recognition consumes one aligned 112x112 crop for each admitted face.
Configuring the recognition graph as another full-frame model would lose alignment,
repeat useless work and break source-frame correlation.

The existing `secondary_inference_scheduler` bounds and prioritizes opaque tasks, but its
request carries only an ROI and source identity. By the time a primary result is decoded,
the corresponding RAW owner may already have been released. A numeric frame ID cannot be
used to recover pixels. Therefore the scheduler is not yet a usable FD-to-FR composition
boundary and production code must not look up a later preview frame as a substitute.

## Required neutral flow

```text
one retained source frame
  -> primary preprocess -> face detector -> boxes + five landmarks
  -> tracker association
  -> bounded cascade admission per source/frame
  -> landmark alignment + crop through image_processor_port
  -> embedding graph
  -> typed embedding result associated with source epoch/frame/track
  -> authorized attendance processor
```

Qualcomm implementations use FastCV/QTI conversion and QNN HTP behind adapters. Other
vendors provide the same neutral ports. No Gst, QNN, FastCV or native allocator type may
enter cascade, perception, feature or attendance contracts.

## Ownership and correlation

A bounded cascade frame store retains the exact `raw_frame` owner under
`source_slot/source_epoch/source_frame_id/source_pts_ns`. The primary submission ticket
and decoded result must match that key. Each accepted secondary task shares the retained
owner; eviction is allowed only when no queued, executing or result consumer can access
the pixels. Timeout and cancellation are task states and do not establish device
completion.

The store has deployment-controlled limits for frames, bytes, faces per frame and maximum
age. When full, admission drops secondary work according to configured priority and emits
a metric; it never replaces a live allocation or creates an unbounded backlog. An epoch
change cancels queued tasks and drains submitted work before releasing the old epoch.

Landmarks use a bounded typed pixel-coordinate structure with an explicit point count and
schema identity. They are not serialized into an opaque observation string. The secondary
request now binds the landmark set (`has_landmarks_` + `observation_landmarks`), ROI, source
key, model slot, optional track ID and, when the task needs the exact pixels, a cascade
retention binding (`requires_retained_frame_` + `retention_ticket_`). The alignment adapter
returns the exact tensor transform so downstream evidence can be mapped back to the source
frame. Transform provenance is still contract-only; the alignment adapter is not implemented.

## Model-package and graph composition

Each catalog model resolves its own package/artifact via the model package registry.
Production supports explicit primary decoder selection. Catalog role/dependency handling
for secondary embedding graphs remains open. Digest/selection validation alone is not
proof of signed authenticity or TOCTOU-safe artifact loading.

Continuous primary graphs remain in `multi_model_session`. Secondary graphs are owned by
one cascade execution domain and invoked only from admitted primary results. They do not
participate in full-frame cadence masks. A shared QNN context is an optimization gate;
independent verified contexts remain valid until multi-graph context lifecycle and memory
budgets have target evidence.

## QCS6490 model evidence

On 2026-09-15 the LACAI-owned synchronous QNN engine composed, finalized and executed the
provided QNN model libraries on the QCS6490 HTP backend. Zero-valued inputs were used to
verify execution and tensor ABI, not accuracy.

| Role | Input | Output | Iterations | min / average / max |
|---|---|---|---:|---:|
| face detector | UINT16 NHWC 1x640x640x3 | 9 UINT16 tensors: score, bbox and 5-point landmarks at strides 8/16/32 | 20 | 3.801 / 4.405 / 4.744 ms |
| face embedding | UINT16 NHWC 1x112x112x3 | UINT16 1x512 embedding | 50 | 2.377 / 2.918 / 3.709 ms |

The detector output counts are 12800, 3200 and 800, which correspond to two anchors per
grid location at strides 8, 16 and 32. Exact tensor names, shapes and quantization must be
copied into each reviewed model package from the runtime-reported ABI. Model binaries and
biometric outputs are never committed to this repository.

## Implementation sequence

1. **Delivered:** typed landmark and embedding contracts validate count, finite values,
   source geometry, identity and configured size ceilings, with contract tests.
2. **Delivered:** the package registry resolves each catalog identity to its own package
   and artifact, and production lookup no longer assumes source-slot order equals catalog order.
3. **Decoder core delivered:** configurable anchor-distance detector decoder supporting typed
   quantized tensors, per-stage stride/anchor count, distance boxes, five landmarks,
   inverse source transform and NMS.
4. Integrate the delivered bounded cascade frame store and extend secondary requests with exact frame
   retention and alignment input. Test epoch changes, cancellation, overload and drain.
5. Implement Qualcomm landmark alignment/crop with the available hardware converter
   behind the neutral image processor boundary; verify tensor parity against golden crops.
6. Bind the embedding graph as secondary work, normalize embeddings in portable
   perception code and return them only through an authorization-aware sensitive-data
   contract.
7. Implement attendance matching and temporal rules separately from model execution:
   gallery revision, threshold/calibration, liveness/quality gate, track-level debounce,
   enter/exit policy and duplicate suppression are configuration, not constants.
8. Measure end-to-end p50/p95/p99, accepted/dropped crops, CPU, HTP load, memory, thermal
   and accuracy before selecting face-per-frame and cadence budgets for deployment.

Attendance acceptance additionally requires a consent/entitlement decision and an owner
for encrypted gallery and event persistence. A face detector plus nearest embedding match
alone is not a completed attendance usecase.


The anchor-distance core uses activation-reserved candidate storage and fixed suppression
storage, with deterministic score ties, finite-value checks and transactional publication.
The YOLOv8 decoder now reuses an activation-bounded candidate/order/suppression workspace
across decode calls (`yolov8_decoder_limits::g_max_candidates`). Output observation
landmark/string vectors still allocate under the current batch contract; primary production
selection exists; pooled output ownership and golden model parity remain required.
Live FD-to-FR cascade composition remains open.

## Primary decoder package boundary

`decoder.json` is parsed by the strict loader `vqec_vision_decoder_package` into the neutral
`decoder_package` contract. Every policy field the runtime consumes is required; unknown
keys, wrong types, out-of-range values, duplicate keys and malformed JSON fail the load.
No model-specific default is supplied for any field, and the package `decoder_contract`
must equal the catalog contract for every kind. Companion schemas are
`config/schemas/yolov8_decoder.schema.json` and
`config/schemas/anchor_distance_decoder.schema.json`.

An anchor-distance package sets kind to "anchor_distance". Required keys are class_id,
landmark_schema_id, landmark_schema_version, landmark_count, anchor_offset_cells,
confidence_threshold, iou_threshold, max_candidates and stages. Each stage requires
score_tensor, box_tensor, landmark_tensor, stride, grid_width, grid_height and
anchors_per_cell, and no tensor name may repeat across stages.

A YOLO package omits kind or sets "yolov8". Required keys are decoder_contract,
class_count, confidence_threshold, iou_threshold, box_tensor and score_tensor; inline
labels and labels_ref are mutually exclusive. Informational metadata (strides, grids,
anchors, box layout metadata) is shape-checked but not consumed by the runtime.

Source geometry comes from sources assigning this model; all such sources must currently
have equal dimensions because the production owner holds one decoder per model.
Tensor geometry and placement come from the model catalog, not decoder.json.
Unknown explicit kinds fail; this is not a fallback for a failed anchor-distance parse.
This boundary enables primary FD; embedding models must still await secondary composition.

## Retained-frame primitive

The serial cascade_frame_store uses activation-sized frame/task storage and a byte budget.
Exact camera/channel/epoch/frame/PTS keys prevent cross-frame lookup. Acquire returns an
owned raw_frame and a unique completion ticket; workers must retain that copy until actual
hardware completion. Retire closes admission; outstanding tickets keep the slot charged.
Tickets carry a store domain: a completion from a retired store is rejected instead of
releasing a replacement store's task, and a zero frame/task/byte budget fails closed.
Completion tickets cannot be replayed within the store lifetime. No automatic timeout or
epoch eviction exists. Callers retire old keys and drain their jobs explicitly.
The store must outlive orchestration; destroying it does not cancel submitted hardware,
whose workers must still own their frame copies. This primitive is not yet pump-integrated.
