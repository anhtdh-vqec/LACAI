# app

Application composition: binds admitted sessions, modules and owner factories to the neutral
ports and drives them one bounded step at a time. Feature business rules stay out of `main`.

- **Status:** source-delivered — service harness runs device-free under QEMU; production owners and threads missing
- **Naming registry:** `appl` (`cgpmp`, `mmump`, `mmses`, `camsn`, `mssup`, `prstg`, `prfac`, `spfac`, `ftfan`, `mmrrt`, `mmfpl`, `acomp`, `rtexe`, `svcmn`, `enprp`, `rcfac`)
- **Depends on:** neutral ports in `include/vqec/vision/ai/ports/`, `src/core/`, `src/perception/`, `src/runtime/`
- **Used by:** `vqec_ai_vision_applications` executable

## Responsibility

- Own one acquisition lifecycle through validate/start/pump/drain/release with explicit deadlines.
- Fan one frame owner out to every due model graph and route results by stable model slot.
- Reconstruct source identity from retained submission tickets, then compose decode/track/feature.
- Provide the take-once delivery slot so the executor cannot run ahead of an unconsumed result.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_camera_graph_pump.cpp` | Connects a started `raw_source_port` to a running `inference_graph_port`, one bounded step per call |
| `vqec_vision_camera_session.cpp` | One-model validate/start/drain/release lifecycle with RPC reconciliation |
| `vqec_vision_multi_model_pump.cpp` | Receive once, cadence-select, share owner, submit per binding (raw-frame or tensor) |
| `vqec_vision_multi_model_session.cpp` | Preflight all graphs, one FW acquisition, partial-start rollback, all-graph drain |
| `vqec_vision_multi_source_supervisor.cpp` | Bind 1..16 borrowed sessions, round-robin progress, per-source fault isolation, latched stop |
| `vqec_vision_perception_result_stage.cpp` | Correlate tensor PTS with retained source identity, decode + track transactionally |
| `vqec_vision_multi_model_result_router.cpp` | Select a stage by immutable model slot, keep independent per-slot progress |
| `vqec_vision_perception_stage_factory.cpp`, `vqec_vision_source_perception_factory.cpp` | Construct owned decoder/tracker/result bundles per source/model binding |
| `vqec_vision_feature_fanout.cpp`, `vqec_vision_multi_model_feature_pipeline.cpp` | Stable-slot feature fan-out with per-feature isolation; route results to direct consumers |
| `vqec_vision_application_composition.cpp` | Bind admitted sessions and drive the supervisor with a one-result slot |
| `vqec_vision_runtime_composition_factory.cpp` | Build the admission snapshot and compose catalog-bound sessions/perception groups |
| `vqec_vision_runtime_executor.cpp` | Round-robin driver that rebuilds pump reports and routes results through decode/track/feature |
| `vqec_vision_service_main.cpp` | Required `vqec_ai_vision_applications` executable; `--mode harness` runs, `--mode production` fails closed |
| `vqec_vision_encoder_preparation.cpp` | Portable encoder admission + CPU pool handoff and combined backend/ledger drain |

## Limits and next work

- Decoder implementations are borrowed; tracker ownership is per binding.
- Multi-model features still need an explicit bounded temporal join.
- Authenticated FW registry RPC, trusted artifact resolution and production platform owners remain missing.
- No thread, per-frame RPC or hardware completion is added here; production backend tests are open.

## See also

- [Camera graph pump](../../docs/architecture/camera_graph_pump.md), [multi-model pump](../../docs/architecture/multi_model_pump.md)
- [Multi-model session](../../docs/architecture/multi_model_session.md), [multi-source supervisor](../../docs/architecture/multi_source_supervisor.md)
- [Runtime executor](../../docs/architecture/runtime_executor.md), [application composition](../../docs/architecture/application_composition.md)
