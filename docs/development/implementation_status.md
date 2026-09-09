# Implementation status — 2026-09-09

Current source inventory, checked against `src/`, public headers, test sources and
`CMakeLists.txt`. This replaces the incremental delivery log: earlier slice limitations
must not be interpreted as the current missing-feature list.

## Evidence level

Source and CMake/CTest declarations exist for the components below. The recorded baseline
has no executed C++ build/test or live FW/Qualcomm/board qualification report. This
documentation refresh performed static inspection only; it adds no execution evidence.
A test source, fake port or successful metadata validation is not a hardware completion,
zero-copy, throughput, model-accuracy or release-compatibility result.

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
| `src/core/`, `include/vqec/vision/ai/contracts/` | Status, explicit frame/tensor metadata, inference/source-binding validation, submission ledger, observation validation, output policy, preview and encoder contracts | Portable detection decoder, tracker and feature algorithms; device completion evidence |
| `include/vqec/vision/ai/contracts/vqec_vision_model_decoder.hpp` | Neutral model-decoder port keeps model output identity and expected frame key at the tensor-to-observation boundary; contract test source is registered in CMake | Concrete detector decoders, model-specific geometry/NMS and tensor-to-observation implementation |
| `src/runtime/lifecycle/vqec_vision_deployment_config.cpp` | Optional strict bounded deployment JSON loader; schemas/examples; pure deployment validation in core | Authenticated configuration activation and service lifecycle |
| `src/runtime/model_registry/` | Optional model catalog/output manifest loaders and bounded OpenSSL SHA-256 stream comparison; core cross-validation and transactional plan composition | Signature verification, trusted immutable artifact/path resolution, decoder lookup and production model loading composition |
| `src/runtime/admission/vqec_vision_activation_snapshot.cpp` | Fixed numeric source/model indices tied to immutable deployment/catalog revisions; assignment/context counts and resident estimate | Measured board-wide accelerator/memory/encoder/thermal admission and owner construction |
| `src/adapters/camera/` | Strict 104-byte legacy wire decoder; SOCK_SEQPACKET/SCM_RIGHTS receiver; session-owned ACK; Start/Stop reconciliation; optional GIO D-Bus client; source lifecycle and bounded RAW-reference resolver | Authenticated FW registry RPC, live transport validation, sync/recovery sign-off and automatic source restart |
| `include/vqec/vision/ai/ports/` | Neutral RAW-source and inference-graph interfaces; source carries shared frame owner and native handle | Additional platform implementations |
| `src/adapters/qualcomm/` | Private plugin graph with caller-supplied runtime factory probing, FD/GstMemory bridge, ordered FLOAT32 tensor extraction, submission primitive and neutral graph adapter | Board model/caps/sync validation, native multi-dtype/multi-graph QNN support and concrete BSP recovery |
| `src/app/vqec_vision_camera_graph_pump.cpp`, `vqec_vision_camera_session.cpp` | Portable single-model receive/submit/result progress and validate/start/drain/release lifecycle | Executable composition, live FW/model integration and automatic recovery |
| `src/runtime/scheduler/vqec_vision_model_cadence.cpp` | Fixed 16-slot rational cadence, sequence-gap accounting and numeric due masks | Measured workload policies, ROI/temporal scheduling |
| `src/app/vqec_vision_multi_model_pump.cpp`, `vqec_vision_multi_model_session.cpp` | Receive once/share owner across due graphs; busy-skip; round-robin results; validate all graphs before one FW acquisition; partial-start rollback and all-graph drain before source release | Trusted activation-to-owner construction and live multi-model validation |
| `src/app/vqec_vision_multi_source_supervisor.cpp` | Binds 1..16 borrowed sessions; round-robin progress, per-source fault isolation, snapshots and latched global stop | Service executors, automatic restart/backoff, epoch replacement and BSP recovery execution |
| `src/core/vqec_vision_preview_surface.cpp`, `vqec_vision_preview_pool.cpp` | Writable-to-sealed CPU NV12 ownership and preallocated 1..4-surface pool; final-reader reuse | Real pixel copy/overlay renderer and hardware allocation/import |
| `src/core/vqec_vision_encoder_window.cpp`, `vqec_vision_encoder_contract.cpp` | Bounded reservation/commit/one-shot submission; frame/PTS/generation checks; independent input/output completion, event preflight/application and fault retention | Concrete encoder backend and device conformance |
| `src/app/vqec_vision_encoder_preparation.cpp` | Reserve/acquire/rollback/cancel, sealed backend envelope, guarded port submission and one-step combined backend/ledger drain | Pixel population, hardware encoder and process job-owner/event-loop composition |
| `src/core/vqec_vision_encoded_output.cpp`, `vqec_vision_output_generation.cpp` | Immutable owned H264/SPS/PPS and monotonic non-reused output binding IDs | Runtime-wide generation ownership and live reconnect lifecycle |
| `src/outputs/vqec_vision_encoded_dispatch.cpp` | Authorized synchronous AU dispatch; one-event backend polling/validation; preflight/dispatch/ledger handling with separate delivery status | Trusted rendered-scope binding, per-job context storage, event loop and event/evidence routing |
| `src/adapters/fw_output/vqec_vision_ring_sink.cpp` | Optional pinned FW SDK wrapper, released header/open-options mapping, generation/demand validation and synchronous payload write | Safe ring open/recovery/single-writer supervision, encoder wiring and live RTSP/UI qualification |
| `src/core/vqec_vision_output_gate.cpp` | Pure revisioned feature/attribute/source authorization and invalidation; used by encoded dispatch | Signed grants, full feature manager, activation enforcement and renderer integration |

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
Factory probing reports availability from the target GStreamer registry without choosing a
fallback backend; deployment policy remains responsible for selecting an admitted path.

## Build and test inventory

- CMake declares portable core, camera wire/control, orchestration, cadence, admission,
  encoded dispatch and encoder preparation libraries.
- Optional flags enable Linux camera transport, GIO D-Bus, standard GStreamer bridge,
  Qualcomm graph, FW ring SDK, JSON loaders and artifact digest.
- JSON loaders require locally provided nlohmann_json 3.12.0; digest requires OpenSSL 3.0
  Crypto. FW ring requires an existing version-pinned SDK target. CMake does not acquire a
  sibling source tree or download these dependencies.
- The optional `vqec_vision_ai_manifest_check` executable checks metadata only. There is
  no `vqec_ai_vision_applications` service entrypoint/target, installed IPK or active CI workflow.
- Unit/contract test sources and CTest registrations cover validators, loaders, cadence,
  fake-port sessions/supervision, Linux receiver fixtures, standard GStreamer lifecycle/
  memory fixtures and output ownership/dispatch. Some require optional flags/dependencies.
- Golden, replay, integration and board test directories remain planned scaffolding.
  No executed test result is inferred from CTest registration.
- `tools/vqec_vision_check_source_layout.ps1` checks physical filenames, quoted include
  existence and CMake source paths. It is not an AST naming, ABI or ownership checker.
  Static inspection found its include-root list omits `src/runtime/scheduler`,
  `src/runtime/admission` and `src/runtime/lifecycle`, although CMake exports them.
  This can falsely flag cadence, activation-snapshot and deployment-loader test includes.
  PowerShell was unavailable during this refresh; the script itself was not executed.
  A Python static check confirmed filenames/CMake paths and resolved quoted includes with
  those three additional roots; this is not a target-specific compiler visibility check.

## Not delivered and next integration work

1. Authenticated deployment/catalog/output/artifact resolution into long-lived source,
   graph, retention-domain and session owners; measured admission before activation.
2. Concrete tensor decoders, tracking/attributes and the 13 commercial features/traffic.
   Perception/features and alternate vendor directories currently contain READMEs only.
3. Trusted overlay renderer, concrete encoder, retained per-job output context/event loop
   and safe ring startup/recovery to complete preview end to end.
4. Full feature/entitlement manager, legacy AI D-Bus compatibility server, service main,
   process supervision, packaging/update integration and observability.
5. Automatic source/BSP recovery, executed host tests, board ownership/fault/golden tests
   and release workload qualification.

Current contract details: [system architecture](../architecture/system_architecture.md),
[multi-model session](../architecture/multi_model_session.md),
[encoder preparation](../architecture/encoder_preparation.md),
[encoded dispatch](../architecture/encoded_dispatch.md),
[FW release compatibility](../contracts/fw_release_compatibility.md).
