# app

Application composition: binds admitted sessions, modules and owner factories to the neutral
ports and drives them one bounded step at a time. Feature business rules stay out of `main`.

- **Status:** source-delivered — reference/fake paths run under QEMU; Qualcomm person flow is board-smoked through compatibility FW services
- **Naming registry:** `appl` (`cgpmp`, `mmump`, `mmses`, `camsn`, `mssup`, `prstg`, `prfac`, `spfac`, `ftfan`, `mmrrt`, `mmfpl`, `acomp`, `rtexe`, `svcmn`, `svopt`, `enprp`, `rcfac`, `pdplt`)
- **Depends on:** neutral ports in `include/vqec/vision/ai/ports/`, `src/core/`, `src/perception/`, `src/runtime/`
- **Used by:** `vqec_ai_vision_applications` executable

## Responsibility

- Own one acquisition lifecycle through validate/start/pump/drain/release with explicit deadlines.
- Fan one frame owner out to every due model graph and route results by stable model slot.
- Keep one latest-wins preview frame per source so output cadence is independent of model cadence.
- Reconstruct source identity from retained submission tickets, then compose decode/track/feature.
- Provide the take-once delivery slot so the executor cannot run ahead of an unconsumed result.
- Resolve an authenticated startup usecase snapshot before package/graph preparation.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_camera_graph_pump.cpp` | Connects a started `raw_source_port` to a running `inference_graph_port`, one bounded step per call |
| `vqec_vision_camera_session.cpp` | One-model validate/start/drain/release lifecycle with RPC reconciliation |
| `vqec_vision_multi_model_pump.cpp` | Receive once, update the bounded preview mailbox, cadence-select, share owner and submit per binding |
| `vqec_vision_multi_model_session.cpp` | Preflight all graphs, one FW acquisition, partial-start rollback, all-graph drain |
| `vqec_vision_cascade_graph_session.cpp` | Starts, drains and unloads one secondary tensor graph outside full-frame cadence |
| `vqec_vision_multi_source_supervisor.cpp` | Bind 1..16 borrowed sessions, round-robin progress, per-source fault isolation, latched stop |
| `vqec_vision_perception_result_stage.cpp` | Correlate tensor PTS with retained source identity, decode + track transactionally |
| `vqec_vision_multi_model_result_router.cpp` | Select a stage by immutable model slot, keep independent per-slot progress |
| `vqec_vision_perception_stage_factory.cpp`, `vqec_vision_source_perception_factory.cpp` | Construct owned decoder/tracker/result bundles per source/model binding |
| `vqec_vision_feature_fanout.cpp`, `vqec_vision_multi_model_feature_pipeline.cpp` | Stable-slot feature fan-out with per-feature isolation; route results to direct consumers |
| `vqec_vision_application_composition.cpp` | Bind admitted sessions and drive the supervisor with a one-result slot |
| `vqec_vision_runtime_composition_factory.cpp` | Build the admission snapshot and compose catalog-bound sessions/perception groups |
| `vqec_vision_runtime_executor.cpp` | Round-robin driver that rebuilds pump reports and routes results through decode/track/feature |
| `vqec_vision_service_main.cpp` | Required `vqec_ai_vision_applications` executable; runs harness and reference/fake/Qualcomm production selections |
| `vqec_vision_service_options.cpp` | Cold-path `parsed_arguments` and CLI parsing for the executable |
| `vqec_vision_cascade_coordinator.cpp` | Bounded per-frame cascade task admission over the frame-lease + alignment ports |
| `vqec_vision_source_session_worker.cpp` | One bounded worker per source session for `--source-execution threaded` |
| `vqec_vision_single_image_inference.cpp` | Synchronous single owned-image inference runner |
| `vqec_vision_fake_platform.cpp`, `vqec_vision_reference_platform.cpp`, `vqec_vision_fixture_detector.cpp` | Device-free platform owners and fixture decoder |
| `vqec_vision_production_platform.cpp` | Resolves dependency-activated catalog identities and composes FW RAW source, owned QNN graphs, primary perception, secondary cascade binding and optional Qualcomm encoded output |
| `vqec_vision_face_enrollment_image_pipeline.cpp` | Advances one authorized JPEG enrollment through dedicated detector/embedding graphs and commits one template |
| `vqec_vision_encoder_preparation.cpp` | Portable encoder admission + CPU pool handoff and combined backend/ledger drain |

## Limits and next work

- Decoder implementations are borrowed; tracker ownership is per binding.
- Multi-model features still need an explicit bounded temporal join.
- Authenticated FW registry RPC and deployment-time peer-name provisioning remain open.
- `--usecase-snapshot <json>` filters the maximum deployment to effective usecase roots.
  An empty result keeps the process idle without preparing graphs or acquiring a source;
  live D-Bus full-generation replacement is wired through `--usecase-dbus` or
  `--usecase-dbus-session` with explicit service/object/peer names, timeout and callback
  budget. Old runtime drains before candidate construction; publication waits for source
  session readiness. All-off remains D-Bus responsive. Desired plans are process-local.
- FR service mode requires explicit `--fr-feature-id` and
  `--fr-identity-attribute`; recognized labels are emitted only when the output gate
  authorizes that source/feature/attribute scope. Enrollment control is optional GIO
  D-Bus and never carries image bytes or embeddings. File enrollment requires explicit
  allow-listed roots, JPEG byte/time limits and Qualcomm element selection; its graph
  lifecycle is isolated from live camera inference.
- Production FR also requires explicit `--fr-gallery-path` beneath a service-owned mode-0700 tmpfs parent for
  the disposable Zvec collection and AI-owned protected-store settings: `--fr-protected-directory`,
  `--fr-gallery-file`, `--fr-key-file`, `--fr-lock-file`, `--fr-gallery-id`,
  `--fr-preprocess-revision` and `--fr-store-max-bytes`. The directory must already exist,
  be owned by the service UID and have mode 0700. AI creates mode-0600 key/gallery/lock
  files, authenticates the snapshot, then rebuilds Zvec before recognition becomes ready.
- Each Qualcomm graph call is synchronous and the renderer still copies into its output DMA
  surface. `--model-execution parallel` runs independent graph owners on persistent bounded
  workers; `--source-execution threaded` separately moves each source session off the
  control/output thread. Defaults remain serialized and unsupported QoS fails closed.
  `--runtime-step-interval-us` explicitly controls the service polling/pacing interval;
  it must be positive and defaults to 1000 microseconds. Lower values trade more control
  loop wakeups for lower multi-step scheduling latency and require workload measurement.
  Completed model slots transfer their retained frame to the one result consumer so stale
  per-slot owners cannot fill a bounded camera producer. The cascade reuses its configured
  quantization/input workspace, avoiding a per-face graph-input deep copy. Released-FW
  interoperability, hardware-completion evidence and sustained thermal performance remain open.
- The `.98` integration run sustained
  29.1 encoded FPS with 1 FPS inference. A later 30/1 cadence run using the Qualcomm
  FastCV image-processor adapter sustained 30 AI results/s and 30.1 RTSP FPS; multi-source,
  percentile latency and thermal limits remain unqualified.

## See also

- [Camera graph pump](../../docs/architecture/camera_graph_pump.md), [multi-model pump](../../docs/architecture/multi_model_pump.md)
- [Multi-model session](../../docs/architecture/multi_model_session.md), [multi-source supervisor](../../docs/architecture/multi_source_supervisor.md)
- [Runtime executor](../../docs/architecture/runtime_executor.md), [application composition](../../docs/architecture/application_composition.md)

Primary detector composition accepts explicit anchor_distance packages (see
docs/architecture/cascade_inference.md) and legacy YOLO packages. A shared decoder requires
equal source dimensions; decoder placement follows catalog preprocessing placement.
Production prepares and cross-validates a neutral embedding cascade binding for one active
source. The service owns secondary graph start/drain/unload and binds the coordinator to
the catalog-derived primary slot before activating that source. Live target parity,
multi-source secondary ownership and asynchronous cascade scheduling remain pending.

Runtime/enrollment test evidence and release gates: [FR validation](../../docs/testing/face_recognition_production_validation.md).
