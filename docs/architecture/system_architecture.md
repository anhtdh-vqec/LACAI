# LACAI system architecture

This document is the current architecture direction for LACAI: the layer boundaries,
ownership, data flow, contract roles and resource rules for the AI APP that replaces
`ai_app` in FW. Unified baseline: 2026-09-20.

**Status:** accepted — the current Qualcomm workload passed its declared five-minute AI APP
30 FPS / average-CPU gate on the recorded QCS6490 target; external release gates remain listed
below.
**Layer:** reference. **Source:** `n/a`.

## Responsibility

- One AI APP replaces `ai_app` in FW: it receives a RAW lease, runs configured models and
  usecases, uses Qualcomm hardware through adapters and stays portable to other vendors.
  The executable is `vqec_ai_vision_applications`.
- AI APP owns scheduler, model integration, perception, feature rules, lifecycle,
  entitlement enforcement, output schema and performance measurement, plus enrollment,
  matching, protected gallery, key lifecycle and index synchronization through neutral
  storage/index ports.
- This is the AI APP-required boundary. BSP+FW producer sign-off and released-FW integration
  verification remain acceptance gates.

## Boundaries and principles

- Unified FW RAW Source Service belongs to FW BSP/FW software; AI APP is a consumer under
  the same lease contract on AI Camera and AI Box. AI Box RTSP demux/decode belongs to FW.
- AI Model delivers a model integration package, not only a binary.
- AI APP owns the usecase App Manager, signed entitlement/package verification, immutable
  content store, inventory, configuration and desired-state contract. Backend follows the
  AI-owned D-Bus facade. FW owns base-system launch/health integration, not usecase inventory.
- FW owns RTSP/UI/recording and persistent evidence media. AI APP owns event/evidence command
  semantics, authorization, durable handoff and AI metadata/query state.
- BSP owns driver/ISP/SDK, memory interoperability, cache/fence/reset contracts.
- No OpenCV. QNN/FastCV/GStreamer only in adapter or benchmark tools.
- One ai service process at v1; module by dependency, not process per usecase.
- Internal trusted plugins; controlled restart on update; no hot unload yet.

## Backend decisions

Backend decision: ADR 0002 establishes reuse of Qualcomm plugins; ADR 0003 adds an owned
QNN adapter for the needed capability/dtype. Both keep neutral ports. Prefer reusing
verified vendor implementations; not every step must use a plugin when an SDK adapter has
a reason and evidence. Production uses the neutral image port with generic FastRPC v1 cDSP
preprocessing plus owned QNN; fully capability/config-driven backend selection remains an
unfinished goal.

ADR 0004 chooses Zvec for the FR index. AI owns matching, protected gallery, encryption,
key lifecycle and recovery; FW only sends authorized enrollment/remove commands. This is
a deliberate ownership change from the initial proposal where gallery/search belonged to
FW.

## Data flow

The diagrams define intended ownership. Current production composition uses
multi_source_supervisor/session/pump, generic v1 cDSP preprocessing, owned QNN, generic dense
or portable anchor-distance decoding, reference tracking and Qualcomm preview output.
Production pairs cDSP/FastCV preprocessing and QNN HTP with cDSP overlay compose, V4L2 hardware
encode and ring output. The FD -> exact-frame alignment -> EdgeFace -> typed embedding
flow is connected at source through neutral ports. The app composition root may include
concrete adapters; orchestration and neutral contracts depend only on ports. The secondary
cascade is source-composed and logic-tested, while live model/golden evidence remains an
integration target.

Mandatory released preview path (in addition to the feature/event design below).
The diagram shows one source; runtime repeats the source-owned state for 1..16 admitted
sources while sharing only proven-compatible model resources:

```text
FW RAW NV12/FD -> source scheduling -> preprocess/inference -> tracks
                              |                                     |
                              v                                     v
                     AI-owned preview surface <---------- overlay commands
                              |
                         overlay renderer -> H264 encoder -> FW v5 ring sink
                                                                  |
                                                released RTSP -> MediaMTX -> UI

Backend D-Bus -> AI-owned App Manager -> complete snapshot -> runtime reconciliation
Legacy desired-only D-Bus -----------------------------> compatibility adapter
```

AI APP owns burned-in preview overlay, encoding and ring production. FW retains
camera capture, main/sub encoding, RTSP/UI, recording and persistent evidence.
No viewer means no preview frame submissions, not automatically no inference.
Create/attach the ring before the first frame so viewer registration can trigger output.
Do not gate preview availability on an existing encoded frame. Do not couple video
cadence to the single outstanding inference job; scheduling separates these demands.
Ring SDK types stay private in adapters/fw_output; overlay commands and encoded-AU
ports stay neutral. The production cDSP-overlay/V4L2-encoder/ring compatibility flow exists.
General output-port composition and released-FW conformance must be validated separately.

```text
FW RAW Source Service -- frame descriptor + handles --> source adapter
                                                   |
                                         source acquisition manager
                                                   |
                                         bounded scheduler/admission
                                                   |
                         neutral image processor
                    Qualcomm: FastRPC v1 -> cDSP FastCV
                    other vendors: capability adapter
                                                   |
                                        AI-owned tensor buffer pool
                                                   |
                                         Qualcomm QNN engine
                                                   |
                               decoder -> detections/pose/embedding/OCR
                                                   |
                                  per-source tracking + attribute cache
                                                   |
                           feature rules / temporal windows / relations
                                                   |
                                 output router + entitlement check
                                                   |
                                FW events / evidence / live data; AI gallery index

FW config + entitlement --> feature manager --> dependency graph + admission
Health / effective state / metrics -------------------------------> FW
```

Commercial enable/disable is resolved before platform preparation. The usecase resolver
derives the effective root-model deployment from installed, entitled, desired, supported,
compatible and admitted gates; only that filtered deployment may load vendor graphs. See
[usecase activation](usecase_activation.md) and the
[FW contract](../contracts/fw_usecase_control.md).

The AI-owned App Manager persists signed entitlement, package inventory, configuration and
desired state, then publishes a complete revisioned snapshot. The runtime can change only
execution state; trusted gates remain independent and fail closed. Startup pre-load filtering is
wired, all-off retains only control and no accelerator is loaded before the first valid frame.
Accepted ADR 0012 applies compatible changes through derived shared-dependency reference counts
and per-slot activation delta instead of replacing unrelated owners. Detailed per-app runtime
cost attribution and real backend conformance remain open; see [App Manager](app_manager.md) and
[incremental activation](incremental_app_activation.md).

Dependent ROI models use the bounded design in
[cascade inference](cascade_inference.md). They retain the exact source frame through
secondary completion and do not enter the full-frame multi-model cadence fan-out.

Release the FW RAW frame lease after ALL image jobs reading that frame complete.
Inference usually uses AI-owned tensors and does not hold a RAW frame lease.
Temporal windows keep small budgeted crops/tensors, not a sequence of 4K frames.
Do not infer non-copy-tensor from the FW RAW input being a DMA-BUF.

## Source tree and dependency

| Layer | Responsibility | Depends on |
|---|---|---|
| contracts + plugin | neutral descriptors/interfaces/versioned ABI | std C++ / C types |
| core | status, clock, lease primitives, bounded queue, geometry | contracts, std |
| runtime | lifecycle, graph, scheduling, admission, registry | contracts, core |
| perception | decode, tracker, typed attrs, pose, embedding, OCR | contracts, core |
| features | rules for the stable S01–S18 catalog and traffic extensions | contracts, core, perception |
| adapters | camera/FW transport/vendor implementation | contracts, core, SDK private |
| outputs | routing, serialization, delivery policy | contracts, core |
| app | composition root, wiring concrete components | all above |

No cyclic dependency. Runtime does not include concrete Qualcomm headers; app injects
factory/port. Shared public headers are in include/vqec/vision/ai; private headers sit
beside the implementation. One CMake target per module with an independent boundary.
No giant common; no ../ pulling headers into the firmware repository.

## Contract roles and status

frame_descriptor, frame_lease, buffer_handle, buffer_view, tensor_descriptor,
tensor_view, image_transform, model_spec, inference_job, job_completion,
observation, track, entity, attribute, relation, feature_event, aggregate.

Ports: frame_source, buffer_manager, image_processor, inference_engine,
model_decoder, feature, entitlement_provider, event_sink, evidence_client, clock.
These are architectural roles, not a list of existing API names. The actual API is in
include/vqec/vision/ai and the naming registry; an unsupported capability must reject
activation.

- Image transform: ROI in source pixels, rotation, letterbox and inverse.
- Buffer view does not own; the job keeps the owner alive until completion.
- Tensor dtype/quantization per tensor; multiple input/output, explicit graph_name.
- Track identity = source + epoch + track id; not the same as person identity.
- Attribute has schema id/version, value/confidence, quality, timestamp/expiry,
  model version and known/unknown/not_observable; do not force a label when quality is
  poor.
- FR/embedding is ordinary metadata under current product storage policy, but live/query/export
  output still requires its access-domain entitlement.
- Coordinates always record frame/space/unit; do not mix normalized box with pixels.

The accepted P2 metadata subsystem is one AI APP-owned service with bounded live state, a
transactional catalog/outbox, time/source detail shards, correction-aware rollups and an optional
manifest-published cold tier. It does not place every detection in one ever-growing database.
Footprints use frame-correlated local tracklets plus revisioned cross-camera associations and
report their retained resolution, gaps and error bounds. See
[spatiotemporal metadata](spatiotemporal_metadata.md); ADR 0009 and its exact composed target
workload/fault gates are accepted for the version 1 baseline.

## Thread model and lifecycle

The App Manager worker serializes entitlement, package, configuration and desired-state
mutations. The runtime control loop serializes snapshot reconciliation and start/stop; input
threads receive frames, bounded workers call the backend, one state executor advances each source
and output workers remain separate. Do not default to multiple threads/contexts per model; SDK
thread safety must be measured.

Feature state: installed -> eligible -> loading -> ready -> running;
branches disabled, unsupported, denied, resource_limited, degraded, faulted.
desired_enabled differs from effective_state; reason_code must always be returnable to FW.

Start: validate snapshot -> resolve dependencies -> resource admission -> acquire source -> wait
for the first valid frame -> load model + warmup -> run -> publish readiness.
Stop: reject new work -> drop safely unsubmitted work -> drain submitted jobs ->
close outputs at revision boundary -> release leases -> release model/context.
Timeout drain: report fault/quarantine; coordinate BSP reset/quiesce; do not self-ACK
falsely.
Camera/source reset: increment epoch; clear tracker/temporal state, mark output gap;
do not emit a new person crossing the line just because an ID reset.

## Compute sharing and resource admission

Key sharing criteria include source/profile, model artifact/hash, preprocess, input
shape/dtype, ROI policy, cadence and quality requirements. Share only when every consumer
meets them. Feature refcount is not enough if FPS/ROI differ; the manager reconciles
aggregate demand.

Admission takes the number of sources, exact resolution/FPS of each source, model
latency/memory, crop rates, temporal
windows, pool capacity, concurrent FW encode/record. Reject with a reason if exceeded.
Each queue has max depth, job age, priority, drop strategy, metrics.
Temporal features handle missing frames per model contract; do not arbitrarily leak.
FR/attribute runs by quality/cooldown/track change with max ROI per frame,
not every person x every model x every frame without bound.
For every profile, packed NV12 = width * height * 3/2; the real allocation also has
stride/padding. 3840x2160 packed = 12,441,600 bytes/frame and about 311 MB/s at
25 FPS is only a sizing example before additional copy/DDR, not a runtime default or
board benchmark.

## Output, entitlement and packaging

Live tracks may be lossy; alarm has bounded durable delivery retry + dedup;
counter checkpoint/window; heatmap bucket; AI FR owns gallery/search semantics,
Zvec behind embedding_index_port and persistence through an AI-owned protected-store
adapter. An alarm contains event_id, source/epoch/timestamp, feature/config/model
versions, track refs, geometry, evidence request id; do not copy video encode inside a
feature.

effective = installed AND licensed AND desired AND supported AND compatible
AND resource_admitted. Disabling a feature removes only dependencies with no remaining
consumer. On entitlement revoke: block sensitive output immediately by revision, drain
compute; do not publish old results after revoke. Offline revocation has agreed limits.

The commercial unit is one declarative `.vqapp`; it is not a process and cannot carry install
scripts or native plug-ins. App Manager verifies its Ed25519 signature, entitlement, exact
component size/digest and semantic compatibility before atomically committing immutable content
and inventory. Each catalog model maps exactly to deployment metadata/artifact through
[model package registry](model_package_registry.md); runtime does not use one implicit path for
all models. Incompatible artifact changes use controlled generation replacement or restart; they
are never hot-unloaded from active vendor execution.

## Observability

Metrics: captured/accepted/dropped frames + reason; queue age; per-stage p50/p95/p99;
camera hold time; pool use; FD count; RSS; SDK jobs; thermal throttling; model warmup;
feature effective state; event retries/loss; source/model/config epochs.
Logs have source/job/model/feature/correlation id, no per-frame INFO or biometrics.

## Limits and next work

- Independent model golden/quality approval, recognition/attendance, hardware-backed gallery key
  qualification, generic backend factory, released-FW DMA completion and performance
  acceptance remain incomplete.
- Fully capability/config-driven backend selection remains an unfinished goal; production
  currently binds generic FastRPC v1 cDSP preprocessing and owned QNN through neutral ports.
- Signed provisioning, durable operation receipts and complete runtime snapshots are delivered.
  Production key rotation, real backend conformance and detailed per-app cost/health attribution
  remain open.
- Not yet included: graph editor, arbitrary third-party plugins, hotload, universal
  optimizer, writing our own vector database engine, or guaranteeing the same workload on
  4 vendors. Zvec is the chosen FR dependency, not a self-developed DB engine. Traffic
  extension uses entity/track/attribute/relations + OCR/calibration contracts.
- Every production claim must come with board image + model + workload + dataset report.

## See also

- [end-to-end system operation and team boundaries](system_operation_flow.md)
- [implementation status](../development/implementation_status.md)
- [documentation map](../../README.md)
- [architecture alignment review](../development/architecture_alignment_review.md)
- [FW release compatibility](../contracts/fw_release_compatibility.md)
- [multi-source configuration](multi_source_configuration.md)
- [FR completion plan](../planning/face_recognition_completion_plan.md)
- [usecase activation](usecase_activation.md)
- [FW usecase control](../contracts/fw_usecase_control.md)
- [cascade inference](cascade_inference.md)
- [model package registry](model_package_registry.md)
- [FR validation](../testing/face_recognition_production_validation.md)
- [ADR 0002 — Qualcomm plugin backend](../adr/0002_qualcomm_plugin_backend.md)
- [ADR 0003 — owned QNN engine](../adr/0003_owned_qnn_engine.md)
- [ADR 0004 — FR gallery and vector index](../adr/0004_fr_gallery_and_vector_index.md)
