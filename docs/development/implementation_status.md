# Implementation status — 2026-09-10

Current source inventory, checked against `src/`, public headers, test sources and
`CMakeLists.txt`. This replaces the incremental delivery log: earlier slice limitations
must not be interpreted as the current missing-feature list.

## Evidence level

Source and CMake/CTest declarations exist for the components below. On 2026-09-09 the
current tree cross-compiled all configured targets to 100% with the eSDK AArch64 compiler,
Camera, GIO D-Bus, GStreamer bridge and Qualcomm adapter enabled. Optional JSON loaders,
artifact digest and FW ring were disabled; the eSDK sysroot does not currently provide
the required nlohmann_json 3.12.0 CMake package. A subsequent neutral Debug configuration
ran 47 AArch64 tests through SDK QEMU: all 47 passed after correcting two stale fixtures;
see [emulation evidence](../testing/esdk_emulation.md). The expanded configuration built
56 unit/contract binaries, and all 56 passed natively on the QCS6490 target; see
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

AI owns private preview pixels, overlay, H264 encoding and FW ring production. FW owns
RTSP/UI/recording and persistent evidence/search. Released preview routing remains limited
to detect0/detect1; multi-source inference does not imply 16 independent preview outputs.

## Source delivered and remaining integration

Paths in this table are relative to the repository root; source stems use `vqec_vision_`.

| Area / source owner | Delivered source behavior | Remaining boundary |
|---|---|---|
| `src/core/`, `include/vqec/vision/ai/contracts/` | Status, explicit frame/tensor metadata, inference/source-binding validation, source-frame-correlated submission ledger, observation/feature-event validation, output policy, preview and encoder contracts | Concrete decoder/tracker/feature algorithms; device completion evidence |
| `include/vqec/vision/ai/contracts/vqec_vision_model_decoder.hpp` | Neutral model-decoder port keeps model output identity and expected frame key at the tensor-to-observation boundary; contract test source is registered in CMake | Concrete detector decoders, model-specific geometry/NMS and tensor-to-observation implementation |
| `src/perception/detection/vqec_vision_model_decode_stage.cpp` | Transactional portable decoder stage validates decoded observations, geometry binding and exception containment before publication | Concrete detector geometry/NMS semantics and model implementations |
| `src/perception/detection/vqec_vision_model_decoder_registry.cpp` | Bounded activation-time mapping from catalog decoder contracts to non-owning decoder ports; validates output-manifest identity through the selected decoder | Trusted decoder loading, lifecycle ownership and concrete model implementations |
| `src/perception/detection/vqec_vision_tensor_reader.cpp` | Bounded tensor lookup and manifest shape/value-count validation for model decoders | Model-specific tensor semantics and postprocess |
| `include/vqec/vision/ai/ports/vqec_vision_tracker.hpp`, `src/perception/tracking/vqec_vision_tracker_registry.cpp` | Neutral serialized tracker port plus bounded activation-time factory registry with distinct per-source/model owners | Concrete association/tracking implementation |
| `src/perception/tracking/vqec_vision_tracking_stage.cpp` | Transactional detection-to-tracker coordinator with monotonic-time enforcement, epoch reset and ambiguous-failure isolation | Concrete association/tracking implementation and replay qualification |
| `src/perception/attributes/vqec_vision_attribute_reader.cpp` | Exact tracked-attribute lookup with schema/version identity, bounded values, source-clock freshness and borrowed-result lifetime | Concrete typed attribute producers, temporal fusion and calibration |
| `include/vqec/vision/ai/contracts/vqec_vision_feature_event.hpp`, `include/vqec/vision/ai/ports/vqec_vision_feature_processor.hpp` | Config-bounded neutral feature events and serialized algorithm port with source/frame/config/schema provenance | Effective-state manager, concrete rules, output router, replay and delivery qualification |
| `include/vqec/vision/ai/contracts/vqec_vision_feature_catalog.hpp`, `src/core/vqec_vision_feature_catalog.cpp` | Versioned feature integration metadata validates processor/configuration contracts, model roles, attribute freshness and bounded temporal/event resources against the model catalog | Authenticated loader, per-source desired/entitled activation, processor registry and concrete feature packages |
| `src/runtime/feature_manager/vqec_vision_feature_stage.cpp` | Activation-validated transactional processor coordinator with monotonic time, strictly increasing epoch reset and ambiguous-failure isolation | Concrete package factories, output router and replay qualification |
| `src/runtime/feature_manager/vqec_vision_feature_processor_registry.cpp` | Bounded compiled-in factory registry resolves processor contracts, validates schema/revision-bound configuration and creates distinct transactional owners | Concrete package factories, authenticated configuration and activation owner graph |
| `src/runtime/feature_manager/vqec_vision_feature_catalog.cpp` | Optional strict bounded JSON loader preserves output on parse/validation failure and feeds the neutral feature catalog validator | Authenticated feature package/config resolution and activation owner graph; eSDK sysroot currently lacks nlohmann_json 3.12.0 for this optional target |
| `src/runtime/feature_manager/vqec_vision_feature_activation_manager.cpp` | Validated cold-path reconciliation of desired, entitlement, resource and model-dependency gates; explicit effective states and per-association processor/stage ownership | Authenticated catalog/configuration source, concrete package factories, runtime recovery and output policy |
| `src/outputs/vqec_vision_feature_event_dispatch.cpp` | Validates one feature event, derives exact field scopes, rechecks captured policy revision and synchronously dispatches through a neutral sink | Bounded durable queue/retry, FW transport, dedup persistence and evidence service integration |
| `src/runtime/lifecycle/vqec_vision_deployment_config.cpp` | Optional strict bounded deployment JSON loader; schemas/examples; pure deployment validation in core | Authenticated configuration activation and service lifecycle |
| `src/runtime/model_registry/` | Optional model catalog/output manifest loaders and bounded OpenSSL SHA-256 stream comparison; core cross-validation and transactional plan composition | Signature verification, trusted immutable artifact/path resolution, decoder lookup and production model loading composition |
| `src/runtime/admission/vqec_vision_activation_snapshot.cpp` | Fixed numeric source/model indices tied to immutable deployment/catalog revisions; assignment/context counts and resident estimate | Measured board-wide accelerator/memory/encoder/thermal admission and owner construction |
| `src/adapters/camera/` | Strict 104-byte legacy wire decoder; SOCK_SEQPACKET/SCM_RIGHTS receiver; session-owned ACK; Start/Stop reconciliation; optional GIO D-Bus client; source lifecycle and bounded RAW-reference resolver | Authenticated FW registry RPC, live transport validation, sync/recovery sign-off and automatic source restart |
| `include/vqec/vision/ai/ports/` | Neutral RAW-source and inference-graph interfaces; source carries shared frame owner and native handle | Additional platform implementations |
| `src/adapters/qualcomm/` | Private plugin graph with caller-supplied runtime factory probing, FD/GstMemory bridge, ordered FLOAT32 tensor extraction, submission primitive and neutral graph adapter; standard-GStreamer lifecycle/ownership fixtures pass on QCS6490 | Live board model/caps/sync validation, native multi-dtype/multi-graph QNN support and concrete BSP recovery |
| `src/app/vqec_vision_camera_graph_pump.cpp`, `vqec_vision_camera_session.cpp` | Portable single-model receive/submit/result progress and validate/start/drain/release lifecycle | Executable composition, live FW/model integration and automatic recovery |
| `src/runtime/scheduler/vqec_vision_model_cadence.cpp` | Fixed 16-slot rational cadence, sequence-gap accounting and numeric due masks | Measured workload policies, ROI/temporal scheduling |
| `src/app/vqec_vision_multi_model_pump.cpp`, `vqec_vision_multi_model_session.cpp` | Receive once/share owner across due graphs; busy-skip; round-robin results; validate all graphs before one FW acquisition; partial-start rollback and all-graph drain before source release | Trusted activation-to-owner construction and live multi-model validation |
| `src/app/vqec_vision_perception_result_stage.cpp`, `vqec_vision_multi_model_result_router.cpp` | Correlates tensor pipeline PTS with retained source identity, routes by stable model slot, derives independent per-model gaps, then composes decode and tracking transactionally | Concrete decoders/trackers, multi-model temporal fusion and replay qualification |
| `src/app/vqec_vision_perception_stage_factory.cpp` | Transactionally constructs one source/model result chain from decoder and tracker registries with distinct tracker ownership and safe stage lifetime | Authenticated tracker-contract selection and concrete decoder/tracker packages |
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

User-confirmed target: QSC6490 / Qualcomm Linux 1.8. [ADR 0002](../adr/0002_qualcomm_plugin_backend.md)
selects a private GStreamer graph:

```text
appsrc -> qtimlvconverter (engine=fcv) -> tensor capsfilter -> qtimlqnn -> appsink
```

The graph validates factories/properties/plans, configures transactionally, loads to READY,
requires source binding before PLAYING, submits through `frame_submission`, correlates
internal ticket PTS and polls results/input completion independently. EOS plus job drain
gates unload. Armed destruction retains the graph in its reserved domain slot; restoration
allows late reconciliation, not DMA cancellation or safe process termination.

Input is a single linear NV12 image; model input is batch1 NHWC RGB/BGR, UINT8 identity or
FLOAT32 explicit plugin coefficients. Output extraction checks ordered FLOAT32 metadata
and performs bounded CPU copies. Multiple application graph instances do not add native
multi-graph/mixed-dtype support to the QNN wrapper. FD duplication/shared ownership does
not establish end-to-end zero-copy or hardware completion.
Factory and property probing report availability and mutability from the target GStreamer
registry without choosing a fallback backend; deployment policy remains responsible for
selecting an admitted path.

## Build and test inventory

- CMake declares portable core, camera wire/control, orchestration, cadence, admission,
  encoded dispatch and encoder preparation libraries.
- Optional flags enable Linux camera transport, GIO D-Bus, standard GStreamer bridge,
  Qualcomm graph, FW ring SDK, deployment/model/feature JSON loaders and artifact digest.
- JSON loaders require locally provided nlohmann_json 3.12.0; digest requires OpenSSL 3.0
  Crypto. FW ring requires an existing version-pinned SDK target. CMake does not acquire a
  sibling source tree or download these dependencies.
- The optional `vqec_vision_ai_manifest_check` executable checks metadata only. There is
  no `vqec_ai_vision_applications` service entrypoint/target, installed IPK or active CI workflow.
- Unit/contract test sources and CTest registrations cover validators, loaders, cadence,
  fake-port sessions/supervision, Linux receiver fixtures, standard GStreamer lifecycle/
  memory fixtures and output ownership/dispatch. Some require optional flags/dependencies.
  The configured AArch64 targets cross-build; 47 neutral tests pass under SDK QEMU and
  all 56 binaries from the expanded adapter configuration pass natively on QCS6490.
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
   graph, retention-domain and session owners; measured admission before activation.
2. Concrete tensor decoders, tracking/attributes and the 13 commercial features/traffic.
   Perception/features and alternate vendor directories currently contain READMEs only.
3. Trusted overlay renderer, concrete encoder, retained per-job output context/event loop
   and safe ring startup/recovery to complete preview end to end.
4. Full feature/entitlement manager, legacy AI D-Bus compatibility server, service main,
   process supervision, packaging/update integration and observability.
5. Automatic source/BSP recovery, live DMA/SDK fault and golden tests, performance/soak
   coverage and release workload qualification.

Current contract details: [system architecture](../architecture/system_architecture.md),
[multi-model session](../architecture/multi_model_session.md),
[encoder preparation](../architecture/encoder_preparation.md),
[encoded dispatch](../architecture/encoded_dispatch.md),
[FW release compatibility](../contracts/fw_release_compatibility.md).

Application composition now wires admitted sessions to supervisor activation, progress
and stop, with one pending tensor/report slot and session-derived recovery reporting.
See [composition contract](../architecture/application_composition.md). Runtime service
construction, live FW/model execution and owner review remain pending.
