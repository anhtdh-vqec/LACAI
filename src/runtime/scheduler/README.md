# scheduler

Allocation-free per-source model cadence selection and bounded future job scheduling.

- **Status:** source-delivered cadence + cascade frame store, both tested; `inference_worker` is a reserved unwired module (ADR 0006)
- **Layer:** runtime
- **Naming registry:** `sched` (`mdcad`, `jobsc`)
- **Depends on:** source/model activation slots
- **Used by:** `multi_model_pump` and `multi_model_session`

## Responsibility

- Provide fixed 16-slot rational cadence selection for 1..16 model slots.
- Account sequence gaps and provide numeric due masks with checked arithmetic.
- Keep per-frame frame-selection policy allocation-free.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_model_cadence.cpp` | Rational cadence, sequence-gap accounting and numeric due masks |
| `vqec_vision_inference_worker.{hpp,cpp}` | Reserved bounded worker pool for blocking-backend offload (ADR 0006; not production-wired) |
| `vqec_vision_cascade_frame_store.hpp` | Retained primary frames, domain-scoped completion tickets and byte accounting for the cascade |

## Limits and next work

- Measured workload policies, ROI and temporal scheduling remain open.
- `vqec_vision_job_scheduler.cpp` is reserved but not implemented.

## See also

- [Model cadence](../../../docs/architecture/model_cadence.md)

`vqec_vision_cascade_frame_store.hpp` retains primary frames for dependent tasks.
Retire prevents new tasks; explicit completion drains retained slots. Worker frame owners
must survive actual hardware reads. See cascade_inference.md for the lifecycle contract.
