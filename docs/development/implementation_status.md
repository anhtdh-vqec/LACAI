# Implementation status — 2026-09-15

Current source inventory, checked against `src/`, public headers, test sources and
`CMakeLists.txt`. This replaces the incremental delivery log: earlier slice limitations
must not be interpreted as the current missing-feature list.

## Current summary and evidence authority

The current cascade/runtime source passes the expanded eSDK QEMU suite (104/104 on
2026-09-15); later commits must record their own validation rather than inherit this count.
Native Zvec and retained-frame synthetic tests passed on .48. Person compatibility flow
has measured 30 AI results/s; approximately 44.5% process CPU remains above the requested
15–25% target. These are historical measured runs, not a new test run for this docs update.
The source now composes primary FD, retained-frame alignment, secondary EdgeFace execution
and embedding decode without model-name branches. Live FD-to-embedding parity, matching,
gallery recovery and attendance are unfinished. Generic backend selection, DMA completion,
released-FW acceptance and allocation/copy optimization remain open.
See [alignment issues](architecture_alignment_review.md) and [capability matrix](capability_matrix.md).

## Dated evidence history (not the current capability list)

2026-09-12 update (model-agnostic optimization S01/S04/S08): `qneng_probe_capabilities`
now advertises only implemented operations (synchronous single job with graph-native
output; async, shared/registered memory, artifact update and multi-model domains report
unsupported), tensor identity is resolved and validated once in `prepare` and re-checked
in full before execute, and unsupported model classes (multi-input, dynamic shape, stateful
sequence, artifact update, batch) are rejected at activation before hardware acquisition.
Neutral 53/53 and expanded 71/71 pass under eSDK QEMU. Board qualification, async/shared
execution and pooled output allocation remain open; see
[model_agnostic_optimization_plan](../planning/model_agnostic_optimization_plan.md) section 8.

2026-09-14 board update: the QCS6490 target came online. The historical native suite ran
81/81 test binaries, and QNN DSP validation passed on Hexagon V68. The LACAI-owned QNN
engine composes, finalizes and executes SCRFD-500M-KPS and YOLOv8n-person on HTP. The
Qualcomm production owner now also runs the live person path through compatibility FW
camera/RTSP services on `.48`; visual inspection confirmed correct color and visible boxes.
Board evidence and limits are in [QCS6490 target](../testing/qsc6490_board.md). Accuracy,
async/shared memory, released-FW DMA completion, zero-copy and performance remain unqualified.

2026-09-15 live update: the production person path sustains 30 AI results/s and 30 encoded
frames/s with package labels rendered as `person`. FastCV preprocessing and QNN HTP are
active. Steady process CPU measured about 44.5%; profiling attributes the remaining cost
mainly to FastCV color/resize, the compatibility NV12-to-QTI render-surface copy and QNN
client-buffer staging. The service also clips decoded edge boxes to the exact preview
contract and no longer terminates on float rounding at the image boundary. SCRFD and
EdgeFace execution/tensor probes are recorded in
[cascade inference](../architecture/cascade_inference.md). The legacy secondary scheduler
does not retain source pixels and is not used by the delivered FD-to-embedding cascade.

2026-09-15 `.99` update: the production SCRFD-to-EdgeFace path completed live embeddings
through FastCV alignment and QNN HTP. A multi-face frame exposed that dependent ROI jobs
share source frame ID and PTS; the inference contract now selects either unique full-frame
submission or repeated tasks for the exact same source frame. After the fix, a short run
routed 445 primary results, completed five embeddings and reported no cascade failures.
That post-fix scene did not exercise multiple accepted faces, so live multi-face validation
remains open. Process CPU measured 29.53% over 15 seconds with encoded output disabled;
this compatibility-source sample is not a production acceptance result.

The neutral recognition policy now aggregates multiple index records by opaque subject,
applies configured minimum similarity and cross-subject margin, and emits deterministic
known/unknown/ambiguous decisions. Backend failure remains an unavailable condition owned
by the caller. Feature processor wiring, calibration, temporal track state and attendance
are still open.

## Historical evidence detail

Source and CMake/CTest declarations exist for the components below. On 2026-09-09 the
current tree cross-compiled all configured targets to 100% with the eSDK AArch64 compiler,
Camera, GIO D-Bus, GStreamer bridge and Qualcomm adapter enabled. Optional JSON loaders,
artifact digest and FW ring were disabled; the eSDK sysroot does not currently provide
the required nlohmann_json 3.12.0 CMake package. A subsequent neutral Debug configuration
ran 47 AArch64 tests through SDK QEMU: all 47 passed after correcting two stale fixtures;
see [emulation evidence](../testing/esdk_emulation.md). The expanded configuration built
65 unit/contract binaries, and all 65 passed natively on the QCS6490 target; see
[board smoke evidence](../testing/qsc6490_board.md). `gst-inspect-1.0` also loaded the
installed `qtimlqnn` and `qtimlvconverter` factories, and the opt-in adapter probe
validated required properties plus NULL-state graph configuration. There is no live
FW/model or hardware-completion qualification report. A
cross-build or fake port is not hardware completion, zero-copy, throughput, model-accuracy
or release-compatibility evidence.

## Current architecture

AI Camera and AI Box expose the same FW RAW NV12/FD lease boundary. Deployment supplies
unique opaque `raw_source_ref` values and exact dimensions/rational FPS. Sensor/ISP,
RTSP discovery/credentials/demux/decode and decoder allocations belong to FW.

Application orchestration depends on `raw_source_port`, `inference_graph_port` and
`source_session_port`. One `multi_model_session` acquires one source and fans one frame
owner out to its due graphs. `multi_source_supervisor` advances pre-composed sessions;
it does not construct them. Arrays support 1..16 sources and 1..16 models per source as
software ceilings. Actual admission must account for lower backend limits, including
one outstanding job per Qualcomm graph and four slots per graph-retention domain.

AI owns private preview pixels, overlay, H264 encoding and FW ring production. The current
Qualcomm path uses an AI-owned QTI DMA pool, `qtivoverlay`, `v4l2h264enc` and the released
ring layout. A bounded latest-wins preview mailbox decouples camera output cadence from
each model's configured inference cadence. FW owns
RTSP/UI/recording and persistent evidence/search. Released preview routing remains limited
to detect0/detect1; multi-source inference does not imply 16 independent preview outputs.

## Source delivered and remaining integration

Paths in this table are relative to the repository root; source stems use `vqec_vision_`.

| Area / source owner | Delivered source behavior | Remaining boundary |
|---|---|---|
| `src/core/`, `include/vqec/vision/ai/contracts/` | Status, explicit frame/tensor metadata, inference/source-binding validation, source-frame-correlated submission ledger, typed landmark/embedding and observation/feature-event validation, output policy, preview and encoder contracts | Additional decoder/tracker/feature algorithms; device completion evidence |
| `include/vqec/vision/ai/contracts/vqec_vision_model_decoder.hpp` | Neutral model-decoder port keeps model output identity and expected frame key at the tensor-to-observation boundary; contract test source is registered in CMake | Concrete detector decoders, model-specific geometry/NMS and tensor-to-observation implementation |
| `src/perception/detection/vqec_vision_model_decode_stage.cpp` | Transactional portable decoder stage validates decoded observations, geometry binding and exception containment before publication; the package-configured YOLOv8 decoder is live on QCS6490 | Anchor-distance/landmark decoder and additional model contracts |
| `src/perception/detection/vqec_vision_model_decoder_registry.cpp` | Bounded activation-time mapping from catalog decoder contracts to non-owning decoder ports; validates output-manifest identity through the selected decoder | Trusted decoder loading, lifecycle ownership and concrete model implementations |
| `src/perception/detection/vqec_vision_tensor_reader.cpp` | Bounded tensor lookup, manifest shape/value-count validation and shared typed scalar/dequantization for model decoders | Model-specific tensor semantics and postprocess |
| `include/vqec/vision/ai/ports/vqec_vision_tracker.hpp`, `src/perception/tracking/vqec_vision_tracker_registry.cpp` | Neutral serialized tracker port plus bounded activation-time factory registry with distinct per-source/model owners | Concrete association/tracking implementation |
| `src/perception/tracking/vqec_vision_tracking_stage.cpp` | Transactional detection-to-tracker coordinator with monotonic-time enforcement, epoch reset and ambiguous-failure isolation | Concrete association/tracking implementation and replay qualification |
| `src/perception/embedding/vqec_vision_exact_embedding_index.cpp`, `src/adapters/zvec/vqec_vision_zvec_embedding_index.cpp` | Neutral revision-pinned embedding index port, bounded exact cosine reference backend and optional Zvec C API adapter source with model-version isolation, cosine-distance conversion and faulted-mutation gating | Durable encrypted gallery adapter, Zvec restart recovery and workload qualification, enrollment and calibrated FR policy |
| `src/perception/attributes/vqec_vision_attribute_reader.cpp` | Exact tracked-attribute lookup with schema/version identity, bounded values, source-clock freshness and borrowed-result lifetime | Concrete typed attribute producers, temporal fusion and calibration |
| `include/vqec/vision/ai/contracts/vqec_vision_feature_event.hpp`, `include/vqec/vision/ai/ports/vqec_vision_feature_processor.hpp` | Config-bounded neutral feature events and serialized algorithm port with source/frame/config/schema provenance | Effective-state manager, concrete rules, output router, replay and delivery qualification |
| `include/vqec/vision/ai/contracts/vqec_vision_feature_catalog.hpp`, `src/core/vqec_vision_feature_catalog.cpp` | Versioned feature integration metadata validates processor/configuration contracts, model roles, attribute freshness and bounded temporal/event resources against the model catalog | Authenticated loader, per-source desired/entitled activation, processor registry and concrete feature packages |
| `src/runtime/feature_manager/vqec_vision_feature_stage.cpp` | Activation-validated transactional processor coordinator with monotonic time, strictly increasing epoch reset and ambiguous-failure isolation | Concrete package factories, output router and replay qualification |
| `src/runtime/feature_manager/vqec_vision_feature_processor_registry.cpp` | Bounded compiled-in factory registry resolves processor contracts, validates schema/revision-bound configuration and creates distinct transactional owners | Concrete package factories, authenticated configuration and activation owner graph |
| `src/runtime/feature_manager/vqec_vision_feature_catalog.cpp` | Optional strict bounded JSON loader preserves output on parse/validation failure and feeds the neutral feature catalog validator | Authenticated feature package/config resolution and activation owner graph; eSDK sysroot currently lacks nlohmann_json 3.12.0 for this optional target |
| `src/runtime/feature_manager/vqec_vision_feature_activation_manager.cpp` | Validated cold-path reconciliation of desired, entitlement, resource and model-dependency gates; explicit effective states, per-association processor/stage ownership and slot-to-catalog mapping accessor | Runtime fan-out/pipeline owner composition, authenticated catalog/configuration source, concrete package factories and output policy |
| `src/outputs/vqec_vision_feature_event_dispatch.cpp` | Validates one feature event, derives exact field scopes, rechecks captured policy revision and synchronously dispatches through a neutral sink | Bounded durable queue/retry, FW transport, dedup persistence and evidence service integration |
| `src/runtime/lifecycle/vqec_vision_deployment_config.cpp` | Optional strict bounded deployment JSON loader; schemas/examples; pure deployment validation in core | Authenticated configuration activation and service lifecycle |
| `src/runtime/model_registry/` | Optional model catalog/output manifest/package-registry loaders and bounded OpenSSL SHA-256 stream comparison; the package registry gives every catalog model an exact package/artifact binding | Signature verification, trusted immutable path opening and decoder lookup |
| `src/runtime/admission/vqec_vision_activation_snapshot.cpp` | Fixed numeric source/model indices tied to immutable deployment/catalog revisions; assignment/context counts and resident estimate | Measured board-wide accelerator/memory/encoder/thermal admission and owner construction |
| `src/adapters/camera/` | Strict 104-byte legacy wire decoder; SOCK_SEQPACKET/SCM_RIGHTS receiver; session-owned ACK; Start/Stop reconciliation; optional GIO D-Bus client; source lifecycle and bounded RAW-reference resolver | Authenticated FW registry RPC, live transport validation, sync/recovery sign-off and automatic source restart |
| `include/vqec/vision/ai/ports/` | Neutral RAW-source, inference-graph and image-processor interfaces; source carries shared frame owner and native handle; processor turns a borrowed NV12 view into the exact model input tensor | Additional platform implementations and pipeline tensor wiring |
| `include/vqec/vision/ai/ports/vqec_vision_image_processor.hpp`, `src/adapters/reference/vqec_vision_reference_processor.cpp`, `src/adapters/qualcomm/vqec_vision_fastcv_processor.cpp` | Neutral image-processor port, device-free CPU baseline and production Qualcomm pipeline using `qtivtransform(engine=fcv)` plus `qtimlvconverter(engine=fcv)`; exact contract validation and UINT8-to-UFIXED16 NEON packing stay private to the adapter | Golden tensor parity, released-FW DMA-BUF evidence, reusable QNN registered input memory and additional dtype/layout semantics |
| `src/adapters/qualcomm/` | Private FastCV preprocessing, plugin graph, FD/GstMemory bridge, typed tensor extraction, owned QNN engine and QTI DMA/overlay/H.264 ring renderer; the compatibility flow sustained 30 AI results/s and a 30 FPS RTSP stream on `.48`; SCRFD and EdgeFace execute probes pass on HTP | Released-FW camera/ring/RTSP acceptance, direct input/output DMA import, registered QNN memory, multi-graph QNN, cascade crop/alignment, thermal qualification and BSP recovery |
| `src/adapters/qualcomm/vqec_vision_qnn_engine.cpp`, `vqec_vision_qnn_inference_graph.cpp`, `vqec_vision_backend_factory.cpp` | Private optional LACAI-owned QNN engine: dlopen backend/system, backend/device, capability probe, context + single-graph model-lib compose, typed tensor metadata, synchronous client-buffer execute and an `inference_graph_port` binding with tensor submission; a factory builds the owned engine+graph bundle from resolved paths and fails closed on an unsupported policy; compiles against vendored QAIRT with the eSDK compiler | Async/shared-memory/LoRA execution wiring, production composition/service selection and board qualification |
| `src/app/vqec_vision_camera_graph_pump.cpp`, `vqec_vision_camera_session.cpp` | Portable single-model receive/submit/result progress and validate/start/drain/release lifecycle | Executable composition, live FW/model integration and automatic recovery |
| `src/runtime/scheduler/vqec_vision_model_cadence.cpp` | Fixed 16-slot rational cadence, sequence-gap accounting and numeric due masks | Measured workload policies, ROI/temporal scheduling |
| `src/app/vqec_vision_multi_model_pump.cpp`, `vqec_vision_multi_model_session.cpp` | Receive once/share owner across due graphs; one latest-wins preview mailbox independent of model cadence; busy-skip; round-robin results; validate all graphs before one FW acquisition; partial-start rollback and all-graph drain before source release | Trusted activation-to-owner construction and live multi-model validation |
| `src/app/vqec_vision_perception_result_stage.cpp`, `vqec_vision_multi_model_result_router.cpp` | Correlates tensor pipeline PTS with retained source identity, routes by stable model slot, derives independent per-model gaps, then composes decode and tracking transactionally | Concrete decoders/trackers, multi-model temporal fusion and replay qualification |
| `src/app/vqec_vision_perception_stage_factory.cpp`, `vqec_vision_source_perception_factory.cpp` | Transactionally construct one source/model chain or a 1..16-slot source group with distinct tracker ownership; exact manifest reference, model/version/digest/decoder identity, tensor bounds and decoder-specific output schema are validated before each tracker is created | Authenticated package/manifest resolution, tracker-contract selection and concrete decoder/tracker packages |
| `src/app/vqec_vision_runtime_composition_factory.cpp` | Builds the validated admission snapshot, composes catalog-bound plans/cadence/output metadata, enforces application-wide graph/cycle uniqueness, constructs source sessions/perception groups and returns a validated application composition without acquiring hardware | Authenticated artifact/path/evidence resolution, platform owner factories, measured admission and service executor activation |
| `src/app/vqec_vision_runtime_executor.cpp` | Drives the composition round robin, rebuilds the pump report from source session progress, routes each tensor result through the per-source perception/feature pipeline, and retains one take-once output slot | Concrete package factories, output dispatch and a threaded service loop |
| `src/adapters/reference/vqec_vision_reference_source.cpp`, `vqec_vision_reference_graph.cpp` | Device-free synthetic NV12 source and zero-tensor graph implementing the neutral ports and the complete graph lifecycle | Any board, model, accuracy, zero-copy or DMA-completion claim |
| `src/app/vqec_vision_service_main.cpp` | Required executable `vqec_ai_vision_applications`: loads catalogs, selects reference or Qualcomm production owners, runs QNN inference, caches latest per-model observations and optionally renders every preview frame into the FW ring | Full authorization-scope renderer binding, process supervision and IPK packaging |
| `src/app/vqec_vision_feature_fanout.cpp`, `vqec_vision_multi_model_feature_pipeline.cpp` | Bounded stable-slot feature fan-out with per-feature isolation; activation-time mapping routes each tracked model result only to its direct feature consumers | Multi-model temporal joins, effective-state manager and concrete feature rules |
| `src/app/vqec_vision_multi_source_supervisor.cpp` | Binds 1..16 borrowed sessions; round-robin progress, per-source fault isolation, snapshots and latched global stop | Service executors, automatic restart/backoff, epoch replacement and BSP recovery execution |
| `src/core/vqec_vision_preview_surface.cpp`, `vqec_vision_preview_pool.cpp` | Writable-to-sealed CPU NV12 ownership and preallocated 1..4-surface pool; final-reader reuse | Real pixel copy/overlay renderer and hardware allocation/import |
| `src/core/vqec_vision_encoder_window.cpp`, `vqec_vision_encoder_contract.cpp` | Bounded reservation/commit/one-shot submission; frame/PTS/generation checks; independent input/output completion, event preflight/application and fault retention | Concrete encoder backend and device conformance |
| `src/app/vqec_vision_encoder_preparation.cpp` | Reserve/acquire/rollback/cancel, sealed backend envelope, guarded port submission and one-step combined backend/ledger drain | Pixel population, hardware encoder and process job-owner/event-loop composition |
| `src/core/vqec_vision_encoded_output.cpp`, `vqec_vision_output_generation.cpp` | Immutable owned H264/SPS/PPS and monotonic non-reused output binding IDs | Runtime-wide generation ownership and live reconnect lifecycle |
| `src/outputs/vqec_vision_encoded_dispatch.cpp` | Authorized synchronous AU dispatch; one-event backend polling/validation; preflight/dispatch/ledger handling with separate delivery status | Trusted rendered-scope binding, per-job context storage, event loop and event/evidence routing |
| `src/adapters/fw_output/vqec_vision_ring_sink.cpp` | Optional pinned FW SDK wrapper, released header/open-options mapping, generation/demand validation and synchronous payload write | Safe ring open/recovery/single-writer supervision, encoder wiring and live RTSP/UI qualification |
| `src/core/vqec_vision_output_gate.cpp` | Pure revisioned feature/attribute/source authorization and invalidation; used by encoded dispatch | Signed grants, full feature manager, activation enforcement and renderer integration |
| `src/outputs/vqec_vision_overlay_preparation.cpp` | Authorized, transactional observation-to-overlay metadata preparation before a renderer adapter | Concrete pixel renderer and Qualcomm overlay integration |

## Qualcomm implementation boundary

User-confirmed target: QCS6490 / Qualcomm Linux 1.8. [ADR 0002](../adr/0002_qualcomm_plugin_backend.md)
supports the private plugin inference graph:

```text
appsrc -> qtimlvconverter (engine=fcv) -> tensor capsfilter -> qtimlqnn -> appsink
```

The current production service uses the owned QNN backend and a separate image-processor
adapter: `appsrc -> qtivtransform(engine=fcv) -> qtimlvconverter(engine=fcv) -> appsink`,
then submits the exact owned tensor through `inference_graph_port`. Backend selection and
preprocess semantics come from activation metadata rather than model identity.

The graph validates factories/properties/plans, configures transactionally, loads to READY,
requires source binding before PLAYING, submits through `frame_submission`, correlates
internal ticket PTS and polls results/input completion independently. EOS plus job drain
gates unload. Armed destruction retains the graph in its reserved domain slot; restoration
allows late reconciliation, not DMA cancellation or safe process termination.

Input is a single linear NV12 image; model input is batch1 NHWC RGB/BGR with any reviewed
element type (INT8..FLOAT32); non-FLOAT32 input requires identity normalization and only
FLOAT32 accepts explicit plugin mean/sigma. Output extraction is dtype-generic across the
same set, sizes bytes by element type and copies exact packed bytes into owned typed blobs.
Genuine per-tensor mixed dtype and non-FLOAT32 output are limited by the single-type caps
and by the installed plugin's FLOAT32 output workaround, so they are rejected rather than
reinterpreted; native output dtype requires the direct-SDK backend. FD duplication/shared
ownership does not establish end-to-end zero-copy or hardware completion.
Factory and property probing report availability and mutability from the target GStreamer
registry without choosing a fallback backend; deployment policy remains responsible for
selecting an admitted path.

The SDK loader now selects the first QNN interface provider whose core API version matches
the headers the adapter was compiled against (major equal, minor not older) instead of
assuming `providers[0]` is compatible; an incompatible set returns `unsupported`. The
QCS6490 board smoke re-verified SCRFD composition and execution on HTP after the change
(`vqec_vision_ai_qnn_engine_smoke`, 9 outputs, exit 0).

The production platform now builds each model backend through `qnn_backend_bundle`
(`vqec_vision_ai_qcom_bfact_create`) instead of constructing `qnn_engine` directly, so the
engine open, capability probe and execution-policy validation happen before a graph
binding is handed to orchestration. `vqec_vision_model_runner` uses the same factory; the
board run of the YOLOv8n-person package through it passed preprocess, QNN execute and
decode (exit 0). FastCV preprocessing selection is still a direct adapter construction.

## Build and test inventory

- CMake declares portable core, camera wire/control, orchestration, cadence, admission,
  encoded dispatch and encoder preparation libraries.
- Optional flags enable Linux camera transport, GIO D-Bus, standard GStreamer bridge,
  Qualcomm graph, FW ring SDK, deployment/model/feature JSON loaders and artifact digest.
- JSON loaders require locally provided nlohmann_json 3.12.0; digest requires OpenSSL 3.0
  Crypto. FW ring requires an existing version-pinned SDK target. CMake does not acquire a
  sibling source tree or download these dependencies.
- The optional `vqec_vision_ai_manifest_check` executable checks metadata only. The required
  `vqec_ai_vision_applications` service target has reference and Qualcomm production
  composition; installed IPK packaging and an active CI workflow remain open.
- Unit/contract test sources and CTest registrations cover validators, loaders, cadence,
  fake-port sessions/supervision, Linux receiver fixtures, standard GStreamer lifecycle/
  memory fixtures and output ownership/dispatch. Some require optional flags/dependencies.
  The current configured AArch64 targets cross-build and 93 tests pass through the SDK
  QEMU wrapper. Historical board runs include 65 expanded adapter binaries on QCS6490.
- Golden, replay and live FW/model integration suites remain planned scaffolding. The
  executed board smoke result covers existing unit/contract binaries only.
- `tools/vqec_vision_check_source_layout.ps1` checks physical filenames, quoted include
  existence and CMake source paths. It is not an AST naming, ABI or ownership checker.
  Its include-root list now follows the current CMake-exported app, adapter, runtime and
  perception directories. PowerShell was unavailable during this refresh, so the `.ps1`
  entrypoint could not execute; a read-only equivalent check passed all filenames, CMake
  paths and quoted includes. This is not a target-specific compiler visibility check.

## Not delivered and next integration work

1. Authenticated deployment/catalog/output/artifact resolution into long-lived source,
   graph and retention-domain owners; the neutral runtime factory builds admitted
   sessions/perception/feature pipelines and the reference backend runs them end to end,
   while the trusted resolver, real platform owner factories and measured admission remain.
2. Additional tensor decoders, production tracking/attributes and the commercial
   features/traffic. The package-configured YOLOv8 decoder is live; the anchor-distance
   face decoder, retained-frame cascade and typed embeddings are source-delivered, while
   live parity, recognition matching and the attendance package remain open.
3. Trusted overlay renderer, concrete encoder, retained per-job output context/event loop
   and safe ring startup/recovery to complete preview end to end. Feature events can be
   authorized and delivered to a bound sink; bounded durable queue/retry and FW transport
   remain.
4. Full feature/entitlement manager and legacy AI D-Bus compatibility server. A service
   harness now exists, but process supervision, packaging/update integration,
   observability and the compatibility server remain.
5. Automatic source/BSP recovery, live DMA/SDK fault and golden tests, performance/soak
   coverage and release workload qualification.

Current contract details: [system architecture](../architecture/system_architecture.md),
[multi-model session](../architecture/multi_model_session.md),
[encoder preparation](../architecture/encoder_preparation.md),
[encoded dispatch](../architecture/encoded_dispatch.md),
[FW release compatibility](../contracts/fw_release_compatibility.md).

Application composition wires admitted sessions to supervisor activation, progress and
stop, with one pending tensor/report slot and session-derived recovery reporting. See
[composition contract](../architecture/application_composition.md). `runtime_executor`
now drives that composition, routes results through the per-source perception/feature
pipeline, and backs the required `vqec_ai_vision_applications` executable with a device-free
reference backend. See [runtime executor](../architecture/runtime_executor.md). Live
FW/model execution, real package registration and owner review remain pending.

Zvec v0.7.0 real-library integration: eSDK-compiled adapter/test passed QEMU and native
QCS6490 .48 on 2026-09-15. Pinned public ARM64 SDK is under third_party/zvec; bootstrap
verifies the release checksum. This is synthetic index evidence, not live FR acceptance.

Anchor-distance FD decoder core is source-delivered: quantized tensor reads, inverse
placement, NMS and typed landmarks, with malformed-score/bounds/atomic-output tests.
Candidate workspace is preallocated. Production registration, pooled observation output
and real model golden parity remain open before the camera cascade is accepted.

Primary anchor-distance packages now select their decoder in production_platform by
explicit decoder.json kind and exact catalog contract. Geometry/placement bind to the
assigned source/catalog; shared models with unequal source geometry are rejected.
The schema is config/schemas/anchor_distance_decoder.schema.json. Live primary FD and
secondary FR still require model-package deployment/golden acceptance.

`decoder.json` now loads through the strict `vqec_vision_decoder_package` loader for both
YOLO and anchor-distance packages: every policy field is required (the YOLO defaults of
0.25/0.45/class_count 1/`boxes_out`/`conf_out` are gone), unknown/duplicate keys, wrong
types, out-of-range values and cross-stage tensor-name reuse are rejected, and the package
contract must equal the catalog contract. The loader unit test covers these cases under
eSDK QEMU; real-model golden parity is still required.

M0 metadata packages for the face chain are recorded from the board runtime ABI:
`manifests/models/scrfd_500m_bnkps/` (anchor-distance decoder, catalog/registry examples)
and `manifests/models/edgeface_s_gamma_05/` (embedding decoder and alignment contract).
The `.so` artifacts, thresholds and preprocessing still need golden parity against the
approved model reference; metadata is not model acceptance.

Model catalog schema v2 adds a required `role` (primary/secondary) and a validated
`depends_on` of immutable primary identities; schema v1 documents are migrated to primary.
A secondary model cannot be a full-frame deployment assignment. Loader and validator tests
cover the accepted and rejected cases; runtime composition derives cascade roots from these
dependencies without adding secondary models to the full-frame submit mask.

Cascade frame retention primitive: activation-sized frame/task storage, full-key lookup,
domain-scoped completion tickets and byte accounting through drain. The store is wired into
the pump/session and logic tests pass eSDK QEMU and QCS6490 .48; this is not dependent-device
completion evidence. The runtime invokes the coordinator with the ticket-reconstructed
source key and retires the exact frame on decode failure or dependent-free stop drain.

`image_alignment_port` is now a defined, unit-tested contract (template/request/result/
transform/capabilities with fail-closed capability gating). The FastCV aligner delivers
board-verified luma geometry and RGB color on `.48`; the generic embedding decoder
(`embedding_decoder_port`, package-configured output/dimension/min-norm, L2 normalization)
is delivered and unit-tested. The EdgeFace package supplies the embedding contract and
alignment template; production secondary graph lifecycle and coordinator wiring are
source-delivered. Golden crop/input/embedding parity remains M5 acceptance work.

Cascade retention slices 1-3a are delivered: the pump retains
cascade-root frames, `multi_model_session` owns a `cascade_frame_store` and delays FW source
release until `store.bytes() == 0`, and composition derives `cascade_root_` from the catalog
`role`/`depends_on` with an optional per-source `cascade` deployment budget. Slice 3b adds the
standalone `cascade_coordinator` (bounded per-frame task admission over
`cascade_frame_lease_port` + `image_alignment_port`, per-task fault isolation). The runtime
executor invokes it only for the catalog-derived dependency root, and the service starts
and drains the resolved secondary graph outside full-frame cadence. The coordinator runs
the secondary pipeline
(quantize aligned RGB to the model input, submit, poll, embedding decode) when a secondary
graph/decoder are configured, covered by the coordinator unit test with fakes. Review fixes:
the frame-lease ticket is separate from the alignment ticket; the secondary input blob uses
the full model input spec (name/dims/dtype/quantization) from the loaded graph; alignment
completion is polled and a synchronous single-inflight embedding graph is required; and the
FastCV aligner validates full luma/chroma plane bounds with overflow-safe arithmetic and
converts only the sampled source ROI. A `.48` plugin probe records the offload options:
`qtivtransform` (engine gles/fcv, crop/destination, no arbitrary affine), `qtimlvconverter`
`roi-batch-*` (hardware ROI crop + tensor batch via ROI meta), `qtivcomposer`, `qtiobjtracker`
(ByteTrack) and `v4l2h264enc`; the recommended alignment offload and the `engine-param`
verification requirement are in `docs/architecture/image_alignment_port.md`. EdgeFace
golden parity, post-fix live multi-face validation, released-FW execution and asynchronous
scheduling remain M5 work.

The cascade coordinator arms its secondary graph once per configured source epoch with an
explicit cycle identity, job timeout and repeated-task sequence policy. The latter preserves
the exact source PTS while allowing multiple face ROIs from the same frame; full-frame
graphs retain strict unique-frame ordering. The coordinator rejects an epoch change until
graph lifecycle restart, rather than submitting a new epoch into an existing submission
window.

Qualcomm full-frame preprocessing now validates the composed preprocess plus tensor
quantization over every RGB8 channel value. It accepts direct UINT8 output or UINT16
full-range widening with at most one quantized LSB of affine-rounding error, including the
face packages' nonzero zero point, and rejects any other mapping before plugin execution.

Production platform preparation now resolves dependency-activated secondary models, opens
their QNN backend owner, validates the embedding decoder and alignment template against the
catalog/package, constructs the FastCV aligner behind `image_alignment_port`, and exposes a
neutral cascade binding. Detection decoders remain registered only for primary models.
The service owns secondary graph lifecycle and binds the coordinator to the catalog-derived
primary slot before primary activation. The current production cascade owner explicitly
supports one active source and one secondary model per source and fails closed otherwise.
