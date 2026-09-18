# app

Application composition: binds admitted sessions, modules and owner factories to the neutral
ports and drives them one bounded step at a time. Feature business rules stay out of `main`.

- **Status:** board-smoke — eSDK tests and the 2026-09-18 `.98` native candidate/service run pass
- **Layer:** app
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
| `cascade/` | Secondary graph task admission, execution and single-image enrollment workflows |
| `composition/` | Top-level owner factories and dependency assembly; no service CLI parsing |
| `pipeline/` | Bounded frame/result/feature/media progress steps; no platform construction |
| `platform/` | Fake, reference and production platform owner bundles |
| `service/` | Executable entry point and cold-path CLI/configuration parsing |
| `session/` | One-source and multi-model acquisition/graph lifecycle ownership |
| `supervision/` | Multi-source fairness, worker ownership and runtime execution loop |
| `CMakeLists.txt` | Declares the application targets and explicit private include boundaries |

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
- [Repository source layout](../../docs/development/source_layout.md)

Primary detector composition accepts explicit anchor_distance packages (see
docs/architecture/cascade_inference.md) and legacy YOLO packages. A shared decoder requires
equal source dimensions; decoder placement follows catalog preprocessing placement.
Production prepares and cross-validates a neutral embedding cascade binding for one active
source. The service owns secondary graph start/drain/unload and binds the coordinator to
the catalog-derived primary slot before activating that source. Live target parity,
multi-source secondary ownership and asynchronous cascade scheduling remain pending.

Runtime/enrollment test evidence and release gates: [FR validation](../../docs/testing/face_recognition_production_validation.md).
