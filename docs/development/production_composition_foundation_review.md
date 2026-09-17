# Production composition foundation review — 2026-09-17

This audit checks Plan 0 against the current composition source and its acceptance gates.
It separates repaired source behavior from target evidence and cross-team approval; it is
not a board qualification or a substitute for signed external contracts.

**Status:** source-delivered — source audit and corrective commits; Plan 0 is not accepted.
**Layer:** docs. **Source:** `src/app/vqec_vision_service_main.cpp`,
`src/app/vqec_vision_production_platform.cpp`,
`src/app/vqec_vision_runtime_composition_factory.cpp`,
`src/app/vqec_vision_runtime_executor.cpp`,
`src/app/vqec_vision_cascade_coordinator.cpp`,
`src/runtime/admission/vqec_vision_activation_snapshot.cpp`.

## Responsibility

- AI APP owns the composition, authorization and runtime changes and collects test traces.
- BSP+FW must review camera/encoder completion and reset, FW concurrency and released-FW
  output conformance. AI Model must review model kit provenance and decoder/attribute/golden
  semantics. This record does not impersonate either owner's approval.
- The AI APP lead makes the final unblock decision only after all Plan 0 gates pass.

## Audit and corrections

| Finding | Current source evidence | Remaining condition |
|---|---|---|
| F01 | Shared-feature authority is projected from one complete usecase association; split grants no longer combine across usecases. | Immutable association must carry source, feature, model slot, attribute scopes and config/catalog/policy revisions together; trusted entitlement provenance remains external. |
| F02 | Qualcomm production rejects `reference.tracker.v1` and nonempty feature catalogs without a compiled production processor; its tracker uses `portable.iou.tracker.v1`. Reference factories remain explicit fixtures. | Portable IoU is a baseline, not accepted MOT; real feature processors and model golden are absent. |
| F03 | Qualcomm renderer takes only prepared overlays; service prepares even an empty overlay and denies preview when no trusted ready usecase is present. | Viewer-demand/revoke and frame/epoch/PTS behavior need released-FW conformance; generic encoded dispatch is not wired into this vertical. |
| F04 | The resolver verifies the catalog digest, copies to sealed `memfd`, verifies that copy and retains it while loading via `/proc/self/fd/N`. | Expected digest, package/decoder metadata and backend/system libraries lack a signed provenance policy; sealed-copy bytes consume additional admission memory. |
| F05 | Factory rejects duplicate source/graph pointers and owns per-source/model graph and processor bindings. | Different-profile, live two-source target workload is not evidenced. |
| F06 | A pending align completion no longer completes the retained frame ticket while its owners live; timeout reports recovery-required. | Cascade still executes synchronously on the progress thread; bounded worker, stale completion, reset/quiescence, bounded stop/join and safe unreconciled teardown are missing. |
| F07 | Production binds the bounded event seam, not the reference sink; acceptance, handoff and shutdown discard are distinct from downstream delivery. | Durable IPC, ACK, replay and FW evidence receipts belong to Plan 3. |
| F08 | Composition requires an explicit profile with ID, target, revision and measurement reference; missing/invalid Qualcomm profile fails closed. Fake/reference use a named fixture. | No reviewed QCS6490 profile or measured DDR/thermal/camera/encoder/FR/index envelope exists; arithmetic is an estimate, not measured capacity. |

The baseline claim of 125 eSDK tests and 118 native tests was produced before the above
corrective commits. A historical smoke cannot validate the current source or certify
ownership, released-FW behavior, sustained performance or sign-off.

The approved eSDK full build and SDK QEMU/CTest run on the corrective source passed
**126/126** on 2026-09-17, including the D-Bus tests. The first sandboxed run could not
bind a D-Bus Unix socket, so the suite was rerun with socket access; it then passed.
BatchMode SSH to the allocated `.98` board failed with `Permission denied
(publickey,password)`; no current-source native or live run was made. No password was
requested or recorded. This is an access blocker, not a board test failure.

## Acceptance oracle and gates

| Gate | Audit decision | Evidence needed to change decision |
|---|---|---|
| P0-A authority/composition | Open | Immutable scoped association and revoke/disabled/shared-model regressions; production processor registration under supported contracts. |
| P0-B output/artifact | Partial | Signed artifact/package authority policy and FW-demand/PTS/revoke tests on the current renderer path. |
| P0-C lifetime/resource | Open | Bounded cascade worker/drain/recovery fault tests and reviewed measured admission profile including sealed bytes and FR/index. |
| P0-D validation/sign-off | Open; eSDK/QEMU 126/126 | `.98` current-source native/live reports and AI APP, BSP+FW, AI Model owner approvals. |

O01 (authority), O02 (identity), O03 (correlation), O04 (completion ownership), O05
(composition), O06 (artifact), O07 (delivery semantics) and O08 (resource) must each have
both a named test and the relevant target/owner evidence. Unit tests only establish the
tested branch, not the whole oracle.

## Unblock decision

**BLOCKED.** Plan 0 remains the production implementation prerequisite for Plans 1–5.
Contract drafting and device-free fixtures can progress without claiming those plans are
production-unblocked. Do not change this decision based solely on a green test count.

## Closure evidence register

| Task | Artifact to attach | Accountable review |
|---|---|---|
| P0-01/02 | Frame-to-output/event trace for all-off, shared detector, policy revoke, two sources, pending stop; one immutable association record and negative cross-grant tests | AI APP lead and entitlement owner |
| P0-03/04 | Production feature factory contract/golden; two-source identity and preview demand/revoke/PTS tests on released FW | AI APP, AI Model, BSP+FW |
| P0-05 | Signed model/package/library provenance decision and sealed-byte QNN load plus mutation/replacement faults | AI APP, AI Model, BSP+FW security owner |
| P0-06 | Bounded worker/queue scheduling, completion and quiescent-reset protocol; timed multi-face fault/restart tests without early frame release or indefinite join | AI APP and BSP+FW DMA/SDK owner |
| P0-07 | Versioned, independently reviewed hardware profile with memory/copies/DDR/thermal/FR/index/encoder and FW coexistence measurements | AI APP, BSP+FW, AI Model |
| P0-08/09 | Full suite on candidate source; `.98` native/live report with exact commit/profile/workload; recorded identities, dates, decisions of the three lead reviewers | AI APP lead, BSP+FW lead, AI Model lead |

The evidence register is intentionally empty of approvals. A lead must link actual reports
and sign a specific candidate revision; silence or a past board run is not sign-off.

## Limits and next work

- AI APP: complete the authority association, asynchronous cascade execution/recovery and
  admission accounting; add negative and stop fault-injection tests.
- BSP+FW: provide reviewed completion/reset and released-FW media evidence, measured
  concurrent resource profile and native `.98` run on the exact candidate commit.
- AI Model: sign catalog/package identity, decode/attribute schema and golden results.
- AI APP lead: record approval identities and evidence references, rerun every gate and
  then explicitly decide `UNBLOCKED` or retain `BLOCKED`.

## See also

- [Plan 0](../planning/architecture_improvement/production_composition_foundation_plan.md)
- [Implementation status](implementation_status.md)
- [Capability matrix](capability_matrix.md)
- [Board workspace](../testing/board_workspace.md)
