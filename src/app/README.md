# app

Application composition: binds admitted sessions, modules and owner factories to the neutral
ports and drives them one bounded step at a time. Feature business rules stay out of `main`.

- **Status:** source-delivered — reference/fake paths run under QEMU; Qualcomm person flow is board-smoked through compatibility FW services
- **Naming registry:** `appl` (`cgpmp`, `mmump`, `mmses`, `camsn`, `mssup`, `prstg`, `prfac`, `spfac`, `ftfan`, `mmrrt`, `mmfpl`, `acomp`, `rtexe`, `svcmn`, `enprp`, `rcfac`, `pdplt`)
- **Depends on:** neutral ports in `include/vqec/vision/ai/ports/`, `src/core/`, `src/perception/`, `src/runtime/`
- **Used by:** `vqec_ai_vision_applications` executable

## Responsibility

- Own one acquisition lifecycle through validate/start/pump/drain/release with explicit deadlines.
- Fan one frame owner out to every due model graph and route results by stable model slot.
- Keep one latest-wins preview frame per source so output cadence is independent of model cadence.
- Reconstruct source identity from retained submission tickets, then compose decode/track/feature.
- Provide the take-once delivery slot so the executor cannot run ahead of an unconsumed result.

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
| `vqec_vision_production_platform.cpp` | Resolves dependency-activated catalog identities and composes FW RAW source, owned QNN graphs, primary perception, secondary cascade binding and optional Qualcomm encoded output |
| `vqec_vision_encoder_preparation.cpp` | Portable encoder admission + CPU pool handoff and combined backend/ledger drain |

## Limits and next work

- Decoder implementations are borrowed; tracker ownership is per binding.
- Multi-model features still need an explicit bounded temporal join.
- Authenticated FW registry RPC and deployment-time peer-name provisioning remain open.
- FR service mode requires explicit `--fr-feature-id` and
  `--fr-identity-attribute`; recognized labels are emitted only when the output gate
  authorizes that source/feature/attribute scope. Enrollment control is optional GIO
  D-Bus and never carries image bytes or embeddings.
- The Qualcomm path is synchronous and still copies into its output DMA surface; released-FW
  interoperability, hardware-completion evidence and long-run performance remain open.
- The current production service loop is serialized. The `.48` integration run sustained
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
