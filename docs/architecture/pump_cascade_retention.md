# Pump cascade retention and dependent drain (design)

Status: **decided (owner-directed)**; implementation is not delivered. The
`cascade_frame_store` primitive and `image_alignment_port` contract exist; the pump is not
integrated and no secondary backend runs.

## Current state

- `cascade_frame_store` (logic-tested): exact `camera/channel/epoch/frame/PTS` keys, byte
  budget, domain-scoped completion tickets, retire/complete/reclaim. Not wired into
  `multi_model_pump`.
- `multi_model_pump` retains `retained_frames_[slot]` from submit to result take only; it
  does not keep pixels after a primary result is taken, so a decoded detection cannot crop.
- No cascade coordinator or secondary backend exists.

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
  waits until all secondary tasks complete and `store.bytes() == 0` before releasing the FW
  source lease. The supervisor's global stop latches to the session; the source is not
  released while any owner is outstanding.
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
accuracy or performance. It must not be described as delivered until the pump integration,
coordinator and board evidence exist.

## See also

- [ADR 0005](../adr/0005_scalable_model_integration.md),
  [cascade inference](cascade_inference.md),
  [multi-model pump](multi_model_pump.md)
