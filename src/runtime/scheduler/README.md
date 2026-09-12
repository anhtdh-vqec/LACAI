# scheduler

Allocation-free per-source model cadence selection and bounded future job scheduling.

- **Status:** source-delivered cadence scheduler — live execution unverified
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

## Limits and next work

- Measured workload policies, ROI and temporal scheduling remain open.
- `vqec_vision_job_scheduler.cpp` is reserved but not implemented.

## See also

- [Model cadence](../../../docs/architecture/model_cadence.md)
