# Pump cascade retention and dependent drain (design)

Status: **source-delivered through production runtime invocation.** The pump borrows a
session-owned `cascade_frame_store`, retains or rolls back cascade-root frames per frame,
and the result route invokes the bounded coordinator using the exact retained identity.
Unit and contract tests cover retention, completion and dependent drain. Target hardware
completion and live model parity remain separate acceptance gates.

## Delivered (slice 1)

- `multi_model_graph_binding.cascade_root_` marks a primary-with-dependents model.
- `multi_model_pump::vqec_vision_ai_appl_mmump_bind_cascade_store(store, camera_id,
  channel_id)` borrows the session store before the first frame.
- The pump retains the exact frame once per due cascade-root set before submitting and
  reports `cascade_retained_model_mask_`/`cascade_dropped_model_mask_`; a full store drops
  the whole cascade-root set for that frame (all dependents need the same pixels), and a
  rejected submit retires the entry so nothing stays charged.
- Tests: `multi_model_pump_cascade_retention` covers retain, rollback, drop, fail-closed
  unbound store and the non-cascade no-op path.

## Delivered (slice 2)

- `multi_model_session` owns the `cascade_frame_store` (`multi_model_graph_config.
  cascade_root_`, `multi_model_session_config.camera_id_/channel_id_/cascade_frames_/
  cascade_tasks_per_frame_/cascade_max_bytes_`; a nonzero budget is required when any graph
  is a cascade root) and binds it to the pump.
- Coordinator API: `acquire_cascade_frame`, `retire_cascade_frame`,
  `complete_cascade_task`, `get_cascade_bytes`; the snapshot exposes `cascade_bytes_`.
- Drain gate: with a retained frame outstanding, the session waits before
  `releasing_source`, so the FW source is not released until `store.bytes() == 0`. A stop
  that never completes hits the existing stop deadline and flags recovery-required.
- `multi_model_session` now implements the neutral `cascade_frame_lease_port` (acquire/
  retire/complete delegate to the store), so the coordinator can drive it directly.
- Tests: the `multi_model_source_lifecycle` session test covers missing-budget rejection,
  retain on submit, the stop gate holding the source, and release after retire + complete.

## Delivered (slice 3a)

- Composition wiring: camera/channel from deployment and `cascade_root_` derived from the
  catalog `role`/`depends_on`, plus admission-derived store sizing.

The deployment source config has an optional `cascade`
(`frames`/`tasks_per_frame`/`max_bytes`) budget, counted in the resident total and either
all-zero or fully set; `runtime_composition_factory` derives `cascade_root_` from the
catalog `role`/`depends_on`, sets camera/channel and the store sizing, and fails composition
when a cascade root has no source budget. Covered by the deployment validation test and the
composition factory contract test.

## Delivered (slice 3b and runtime binding)

`cascade_coordinator` (`src/app/vqec_vision_cascade_coordinator.cpp`)
plus the neutral `cascade_frame_lease_port`. For one decoded primary observation batch it
admits at most `max_tasks_per_frame_` faces, acquires the exact retained frame per task,
aligns each through `image_alignment_port`, completes every acquired ticket (including on
alignment failure) and closes admission once. When a secondary embedding graph and decoder
are configured it also quantizes the aligned RGB to the model input, submits, polls and
decodes the embedding. Per-task failures are counted and isolated. Unit test
`cascade_coordinator` covers bounded admission, ordering, align failure, acquire failure,
the align+embedding pipeline and misconfiguration. The coordinator arms the secondary graph
once with its configured cycle/deadline for the retained source epoch before tensor
submission; an epoch change requires graph lifecycle restart. `runtime_executor` invokes
the coordinator only for the dependency root slot, and the production service owns the
secondary graph lifecycle and binding. Real EdgeFace golden parity and live cascade
acceptance remain M5 work; a compatibility live smoke exists on `.98`, while post-fix
multi-face and released-FW evidence remain open.



## Current state

- `cascade_frame_store` is session-owned and wired to `multi_model_pump` for exact
  `camera/channel/epoch/frame/PTS` retention, byte accounting and domain-scoped tickets.
- `multi_model_pump` retains cascade-root frames before primary submission and rolls back a
  rejected submit; `multi_model_session` exposes the neutral frame-lease port and holds FW
  release until retained work drains.
- The standalone coordinator implements align + synchronous embedding execution. Production
  service composition starts/drains the secondary graph and binds the coordinator to the
  declared primary slot. Live hardware and model parity remain open.

## Ownership model

- **Store owner**: one `cascade_frame_store` per source session, created at composition
  (activation-sized frames/tasks/bytes) and destroyed only after the session reaches
  `stopped`. The session owns it; the pump borrows it. The store outlives any hardware read.
- **Retain**: the pump retains the received frame under its exact key before submitting to a
  cascade-root (primary-with-dependents) graph. If the submit is rejected, the retain is
  rolled back in the same step. A slot is never left charged without a matching result.
- **Acquire**: when a primary result is decoded into detections, the cascade coordinator
  looks up the exact frame from the submission ticket's source key — never the latest
  preview — and acquires owner + completion ticket per admitted secondary task.
- **`complete(ticket)` owner**: the coordinator that owns the secondary task, after
  `image_alignment_port::poll_completion` (or the embedding graph completion) confirms the
  device finished reading. The store owner does not auto-complete; timeout, FD close, stop
  and source disconnect are not completion.
- **Ticket domain**: tickets are domain-scoped to the store instance. A stale completion
  from a retired store is rejected, so a replacement store cannot release the wrong task.

## Drain and epoch semantics

- On source epoch change: retire the old keys (close admission), cancel queued tasks, drain
  submitted tasks; publish a gap so a new person is not emitted because an ID reset.
- On stop: `multi_model_pump::begin_stop` closes new receives; the session retires keys and
  drains primary graphs. A cascade-root result completed during drain is not sent to a new
  dependent, so the session retires that exact ticket's frame. Already-acquired secondary
  tickets remain charged until their real completion. The session requires
  `store.bytes() == 0` before releasing the FW source lease.
- On failure/timeout: the task is marked faulted and counted; the frame stays charged until
  real completion or an explicit recovery/quarantine decision. No silent release.

## Decisions (owner-directed)

1. **Store is session-owned.** One `cascade_frame_store` per source session, sized at
   composition, destroyed only after `stopped`. Rationale: the session owns the FW source
   lease and is the unit that must not release FW until every owner drains; bundle- or
   pump-level placement would couple sources or violate the borrowed-pump model.
2. **Coordinator runs in the serialized session step, not a separate thread.** The owned
   QNN engine is synchronous with one inflight job, so concurrency is not yet justified;
   `source_session_worker` already provides per-source thread isolation when enabled. The
   `complete(ticket)` call happens on the session's serialized thread after
   `poll_completion`. Revisit only with measured need.
3. **Store full: bounded priority drop, never replace a charged frame.** When full, drop the
   lowest-priority evictable *queued* task and emit a metric; a frame charged by an acquired
   ticket is never dropped. Configured max age stops new admission and cancels queued tasks;
   it never releases an acquired frame (completion stays the only release).
4. **Per-session drain, independent and ordered.** Stop new receives → retire keys (close
   admission) → cancel queued secondary → drain submitted secondary (complete tickets) →
   drain primary graphs → require `store.bytes() == 0` → release FW source. Supervisor global
   stop latches to every session and releases no source until that session reports drained.

## Non-claims

This design does not establish device completion, DMA/cache/fence behavior, zero-copy,
accuracy or performance. The pump integration and coordinator are delivered and logic-tested; released-FW device
completion evidence remains open, so hardware completion is not claimed.

## See also

- [ADR 0005](../adr/0005_scalable_model_integration.md),
  [cascade inference](cascade_inference.md),
  [multi-model pump](multi_model_pump.md)
