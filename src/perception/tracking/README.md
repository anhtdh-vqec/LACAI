# tracking

Per-source tracking state, association and ID continuity with explicit source-epoch resets.

- **Status:** source-delivered port + registry + coordinator — concrete association missing
- **Naming registry:** `track` (`trkst`, `trreg`)
- **Depends on:** neutral `tracker_port`, decoded observation batches
- **Used by:** `src/app/perception_result_stage` and `perception_stage_factory`

## Responsibility

- Receive explicit monotonic time and source-gap information from the coordinator.
- Return observation batches with track IDs and reset state on a new source epoch.
- Keep association algorithms and vendor choices behind the neutral `tracker_port` boundary.
- Create exactly one tracker owner per source/model binding from an activation-supplied contract.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_tracking_stage.cpp` | Serialized coordinator: validation, epoch reset, monotonic time, fault isolation |
| `vqec_vision_tracker_registry.cpp` | Bounded activation-time mapping from tracker contract to a factory |

## Limits and next work

- Concrete association/tracking implementation and replay qualification remain pending.
- An ambiguous update failure faults that epoch; only a successful later-epoch reset resumes.
- Composition owns authenticating the binding and selecting the contract.

## See also

- [Tracking stage](../../../docs/architecture/tracking_stage.md), [tracker registry](../../../docs/architecture/tracker_registry.md)
- [Tracker port](../../../docs/architecture/tracker_registry.md)
