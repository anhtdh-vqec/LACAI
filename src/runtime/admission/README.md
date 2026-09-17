# admission

Admission by measured workload/camera/model/ROI/memory budgets, with explicit rejection reasons.

- **Status:** source-delivered cold-path snapshot — board capability admission not implemented
- **Layer:** runtime
- **Naming registry:** `admis` (`actsp`)
- **Depends on:** deployment/model/feature catalogs
- **Used by:** runtime composition factory

## Responsibility

- Materialize a validated deployment/catalog into fixed-capacity numeric source/model slots.
- Bind every index to exact immutable revisions and remove string lookup from per-frame work.
- Calculate comprehensive resource envelopes: frame pool, tensor pool, encoder pool, cascade ROI, DDR bandwidth, FW concurrency slots, and worker load.
- Validate resource requirements against measured hardware admission profiles (pool, queue, encoder, DDR, thermal, FW load) with explicit fail-closed rejection reasons.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_activation_snapshot.cpp` | Fixed-capacity numeric source/model slots and measured resource admission bound to immutable revisions |

## Limits and next work

- This is a cold-path component, not the multi-source supervisor.
- Actual admission must account for lower backend limits (one outstanding Qualcomm job, four
  slots per graph-retention domain).
- Real-time dynamic thermal throttling adaptation remains open for future operational runbooks.

## See also

- [Multi-source configuration](../../../docs/architecture/multi_source_configuration.md)
- [Model catalog](../../../docs/architecture/model_catalog.md)
