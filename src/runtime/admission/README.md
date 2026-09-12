# admission

Admission by measured workload/camera/model/ROI/memory budgets, with explicit rejection reasons.

- **Status:** source-delivered cold-path snapshot — board capability admission not implemented
- **Naming registry:** `admis` (`actsp`)
- **Depends on:** deployment/model/feature catalogs
- **Used by:** runtime composition factory

## Responsibility

- Materialize a validated deployment/catalog into fixed-capacity numeric source/model slots.
- Bind every index to exact immutable revisions and remove string lookup from per-frame work.
- Expose assignment/context counts and a resident resource estimate.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_activation_snapshot.cpp` | Fixed-capacity numeric source/model slots bound to immutable revisions |

## Limits and next work

- This is a cold-path component, not the multi-source supervisor or board capability admission.
- Actual admission must account for lower backend limits (one outstanding Qualcomm job, four
  slots per graph-retention domain).
- Measured board-wide accelerator/memory/encoder/thermal admission remains open.

## See also

- [Multi-source configuration](../../../docs/architecture/multi_source_configuration.md)
- [Model catalog](../../../docs/architecture/model_catalog.md)
