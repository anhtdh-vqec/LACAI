# Cascade inference for detection and embeddings

Status: contract and composition design; model execution probes plus neutral typed
landmark/embedding contracts are complete, while decoder production, frame retention and
secondary preprocessing remain open.

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

Landmarks use a fixed-capacity, typed pixel-coordinate structure with an explicit point
count and transform provenance. They are not serialized into an opaque observation string.
The secondary request binds the landmark set, ROI, source key, model slot and optional
track ID. The alignment adapter returns the exact tensor transform so downstream evidence
can be mapped back to the source frame.

## Model-package and graph composition

Every model slot resolves its own authenticated package directory, model artifact and
decoder contract. The current Qualcomm production owner accepts one package/library pair
and reuses it for all catalog entries; that compatibility path must be replaced before a
multi-model FD/FR deployment is valid. Resolution is keyed by immutable catalog identity
and artifact reference, with containment and digest checks before graph creation.

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
2. Add a package registry so each model catalog entry resolves its own package/artifact;
   remove the single-package production assumption.
3. Implement a package-configured anchor-distance detector decoder supporting typed
   quantized tensors, per-stage stride/anchor count, distance boxes, five landmarks,
   inverse source transform and NMS.
4. Add the bounded cascade frame store and extend secondary requests with exact frame
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
