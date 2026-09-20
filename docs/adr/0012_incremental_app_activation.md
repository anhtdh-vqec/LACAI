# ADR 0012 — Incremental application activation

Status: proposed — source delivery and QCS6490 evidence are required before acceptance.
Date: 2026-09-20.
Owner: AI APP lead.

## Context

The AI-owned App Manager publishes complete immutable runtime snapshots, but the service currently
turns every newer snapshot into a full runtime-generation replacement. That behavior is safe for
ownership, but it stops the RAW source and every model graph even when one installed application is
merely enabled, disabled or reconfigured. It therefore does not scale to eighteen independently
controlled usecases.

Several applications can depend on the same model. Counting, intrusion and smoking may share a
person detector, while face recognition can share a detector with other face usecases. A UI-level
toggle or an output-only gate is insufficient: newly unauthorized work must stop, an unreferenced
graph must drain and unload, and a graph still referenced by another application must remain
running.

## Decision

- App Manager continues to publish complete version 1 snapshots. Signals remain wake-ups; the
  runtime computes a delta between two validated snapshots and never accepts an unauthenticated
  command delta as authority.
- The runtime derives one immutable dependency identity from source/profile, model catalog entry,
  artifact digest, preprocess contract, tensor contract, ROI/cadence/quality policy and target.
  Reference counts are derived from effective application consumers; they are never writable
  counters supplied by backend or UI.
- One feature instance is owned per effective `(source, app, feature, configuration)` association,
  except that exact compatible instances may be shared only under an explicit equivalent identity.
  Model graphs are shared by exact dependency identity and numeric model slot.
- Revocation and disable first publish the candidate output policy and remove the obsolete feature
  binding on the serialized executor. The model slot then stops accepting work, drains real backend
  completion and unloads only when its reference count changes from one to zero.
- Enable constructs and validates the candidate feature owner before publication. A zero-to-one
  model transition loads only that slot after the source has produced a valid frame; it does not
  restart another running slot or reacquire the source.
- A shared-count transition `N -> N-1`, where `N-1 > 0`, performs no graph lifecycle operation.
  Configuration-only changes replace only the affected feature owner. Unrelated tracker, temporal
  feature, cadence and graph state keep their identities and history.
- Source/profile identity changes, capacity additions not present in the prepared installed-app
  set, model artifact/semantic-contract replacement and unsafe hardware recovery remain explicit
  replacement boundaries. A replacement is source-local where ownership permits; it is never
  silently described as an incremental delta.
- Disabling the final application releases the source after all graph readers complete. Enabling
  from the all-off control-only state creates a new source generation so the non-restartable source
  owner and Qualcomm first-frame gate remain correct.
- Delta apply is serialized, bounded and transactional at the authority/binding boundary. Failure
  cannot publish partial feature/output authority. Submitted hardware work is never cancelled by a
  timeout, FD close or counter change.

## Alternatives

- Full generation replacement for every snapshot: retained only as a safe fallback for the
  replacement boundaries above; rejected for normal toggles because unrelated applications lose
  continuity.
- One process or one RAW lease per application: rejected because it duplicates capture, graph,
  tensor and accelerator ownership and prevents exact dependency sharing.
- Keep every installed graph permanently loaded and gate only scheduling: rejected as the product
  rule because eighteen applications would reserve memory/accelerator state without consumers.
- Trust a backend-provided reference count: rejected because authorization and dependency closure
  belong to AI APP and the count can become inconsistent with the complete snapshot.

## Consequences

- The source session needs mutable per-slot scheduling and graph lifecycle while preserving the
  immutable slot identity of its prepared capacity set.
- Feature fan-out needs a serialized transactional rebind rather than a generation-lifetime-only
  borrow.
- Admission distinguishes prepared capacity from currently active resource demand. Preparing an
  owner does not prove or reserve hardware capacity; zero-to-one activation must revalidate the
  measured envelope.
- Metrics and status must expose snapshot revision, dependency counts, active/draining masks,
  delta failure and fallback reason so a full replacement cannot be hidden.
- Product updates that change model identity can still use controlled replacement; ordinary
  desired/configuration/entitlement changes use the incremental path.

## Approve after (gates)

1. Planner tests cover all eighteen catalog applications, shared and unique dependencies,
   conflicting artifacts, stale snapshots, overflow and transaction preservation.
2. Fake-port lifecycle tests prove `2 -> 1` does not drain a shared graph and `1 -> 0` drains only
   that graph while another slot continues receiving/submitting frames.
3. Feature tests prove disable/reconfigure preserves unrelated processor identity and temporal
   state, and revoked output is rejected before obsolete work can publish.
4. eSDK cross-build and the complete QEMU/CTest suite pass.
5. The recorded QCS6490 runs two installed applications in one source session, toggles each at
   five-to-ten-second cadence, preserves service/source identity and preview continuity, and records
   graph lifecycle, FPS, CPU and RSS evidence without leaks.
