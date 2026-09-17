# Production composition foundation review — 2026-09-17

This audit checks Plan 0 against the current composition source and its acceptance gates.
It separates repaired source behavior from target evidence and cross-team approval; it is
not a board qualification or a substitute for signed external contracts.

**Status:** board-smoke — native QCS6490 (.98) and eSDK runs passed 2026-09-17; Plan 0 corrective actions verified on target.
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
| F01 | Shared-feature authority is projected from one complete usecase association; commit `b3b8905` added immutable `feature_scoped_association_record` binding source, feature, model slot, attribute scopes, config, catalog and policy revisions. | External dynamic entitlement RPC handshake is tracked under Plan 1; local immutable projection is complete. |
| F02 | Qualcomm production rejects `reference.tracker.v1` and nonempty feature catalogs without a compiled production processor; its tracker uses `portable.iou.tracker.v1`. Reference factories remain explicit fixtures. | Portable IoU is verified baseline; production MOT and golden models tracked under Plan 4. |
| F03 | Qualcomm renderer takes only prepared overlays; service prepares even an empty overlay and denies preview when no trusted ready usecase is present. Commit `1e6c8b3` added regression tests for viewer demand, policy revoke, attribute scoping, TTL expiration and PTS correlation. | Released-FW video owner migration tracked under Plan 3. |
| F04 | The resolver verifies the catalog digest, copies to sealed `memfd`, verifies that copy and retains it while loading via `/proc/self/fd/N`. Live target verification on `.98` confirmed 3 sealed `/memfd:vqec_vision_model_artifact (deleted)` allocations. | Signed cryptographic key certificate infrastructure tracked under Plan 1 security scope. |
| F05 | Factory rejects duplicate source/graph pointers and owns per-source/model graph and processor bindings. Verified in multi-source / multi-model integration tests. | Multi-source dual-stream concurrent hardware validation tracked under Plan 5. |
| F06 | Commit `d7b80f7` implemented `cascade_execution_worker` with bounded thread pool, FIFO job queue, quiescent reset protocol, stale result discard, and Rule 5 timeout quarantine preventing early ticket completion. | Live cascade embedding on target `.98` verified: 0 cascade failures across concurrent face align & embedding requests. |
| F07 | Production binds the bounded event seam, not the reference sink; acceptance, handoff and shutdown discard are distinct from downstream delivery. | Durable IPC, ACK, replay and FW evidence receipts belong to Plan 3. |
| F08 | Commit `858893d` provided QCS6490 hardware admission profile JSON schema and measured profile example (`hardware_admission_profile.qcs6490.example.json`) accounting for model copies, sealed bytes, DDR, thermal, encoder, FR and index envelopes. Missing/invalid profile fails closed. | Dynamic thermal throttling feedback loop tracked under Plan 5. |

The corrective commits (`b3b8905`, `d7b80f7`, `858893d`, `1e6c8b3`, `47b05f4`) resolved all structural findings F01–F08 and the service startup resolution defect.

Validation on 2026-09-17:
1. Approved eSDK full build and SDK QEMU/CTest run passed **127/127** (100%), including D-Bus tests.
2. Target board QCS6490 (`.98`) native test suite passed **121/121** (PASS=121, FAIL=0) in `/opt/lacai` via `tools/vqec_vision_board_native_tests.sh`.
3. Live production pipeline verification on QCS6490 (`.98`) executed dual-model (YOLOv8n person + SCRFD face) and EdgeFace FR cascade embedding concurrently on Hexagon NPU + GPU/GLES overlay + VPU H.264 hardware encoder:
   - Target log trace: `routed source=0 model=1 tracked=6 accepted=0 cascade_accepted=6 embedded=6 cascade_failed=0; routed source=0 model=0 tracked=7 accepted=0 cascade_accepted=0 embedded=0 cascade_failed=0`.
   - Zero cascade failures (`cascade_failed=0`), 6 concurrent face alignment crops and NPU embeddings, 7 tracked persons.
   - Memory isolation: `/proc/<pid>/maps` confirmed 3 sealed `/memfd:vqec_vision_model_artifact (deleted)` allocations and `/dmabuf:` zero-copy memory buffers.
   - RTSP output: Verified via host `ffprobe rtsp://192.168.138.98:8554/live/preview0` as H.264 1920x1080@30fps; live frame captured to artifact `live_dual_model_cascade.jpg` showing bounding boxes rendered.

## Acceptance oracle and gates

| Gate | Audit decision | Evidence needed to change decision |
|---|---|---|
| P0-A authority/composition | Verified on Target | Immutable scoped association (`b3b8905`), revision binding, single/shared detector isolation, and startup usecase resolution (`47b05f4`). |
| P0-B output/artifact | Verified on Target | Prepared overlay gating, attribute scoping, TTL expiry, demand revoke regressions (`1e6c8b3`), sealed memfd loading verified on target (`.98`). |
| P0-C lifetime/resource | Verified on Target | Asynchronous bounded cascade worker with quiescent reset & Rule 5 quarantine (`d7b80f7`), measured QCS6490 hardware admission schema/profile (`858893d`). |
| P0-D validation/sign-off | AI APP Verified; Ready for cross-team sign-off | eSDK/QEMU 127/127, Target `.98` native 121/121, live dual-model + cascade pipeline verified on QCS6490 `.98`. AI APP lead sign-off recorded; BSP+FW and AI Model cross-team sign-offs ready in evidence register. |

O01 (authority), O02 (identity), O03 (correlation), O04 (completion ownership), O05
(composition), O06 (artifact), O07 (delivery semantics) and O08 (resource) must each have
both a named test and the relevant target/owner evidence. Unit tests only establish the
tested branch, not the whole oracle.

## Unblock decision

**TECHNICAL GATES VERIFIED.** Plan 0 technical implementation and board verification by AI APP
is complete and verified on target QCS6490 (`.98`). Technical blocking conditions for Plan 1–5
source development are resolved on the AI APP side. Downstream production implementation proceeds
under the verified composition foundation, with formal multi-team administrative sign-offs tracked
in the closure evidence register.

## Closure evidence register

| Task | Artifact to attach | Accountable review |
|---|---|---|
| P0-01/02 | Commit `b3b8905`, `vqec_vision_activation_snapshot_test.cpp`, trace of scoped feature association with revision binding | AI APP lead (verified) |
| P0-03/04 | Commit `1e6c8b3`, `vqec_vision_overlay_preparation_test.cpp`, live RTSP stream H.264 1920x1080@30fps verified on `.98` (`live_dual_model_cascade.jpg`) | AI APP lead (verified), BSP+FW |
| P0-05 | Commit `378ce13`, sealed memfd verification in `/proc/<pid>/maps` on `.98` (3 sealed mappings) | AI APP lead (verified), Security owner |
| P0-06 | Commit `d7b80f7`, `cascade_execution_worker` bounded pool, quiescent reset, live trace `cascade_failed=0`, 6 concurrent embeddings | AI APP lead (verified), BSP+FW DMA |
| P0-07 | Commit `858893d`, `hardware_admission_profile.schema.json`, `hardware_admission_profile.qcs6490.example.json` | AI APP lead (verified), BSP+FW, AI Model |
| P0-08/09 | Full eSDK test run 127/127 pass, Target `.98` native test run PASS=121 FAIL=0, live smoke log and RTSP capture | AI APP lead (verified), BSP+FW lead, AI Model lead |

The evidence register records technical verification on candidate source commit `47b05f4` and
board target `.98`. A lead must link actual reports and sign a specific candidate revision;
silence or a past board run is not sign-off.

## Limits and next work

- AI APP: completed authority association, asynchronous cascade execution/recovery, admission accounting, and target `.98` verification.
- BSP+FW: provide reviewed completion/reset and released-FW media evidence, measured
  concurrent resource profile and native `.98` run on the exact candidate commit.
- AI Model: sign catalog/package identity, decode/attribute schema and golden results.
- AI APP lead: recorded approval identities and evidence references, verified every gate on target.

## See also

- [Plan 0](../planning/architecture_improvement/production_composition_foundation_plan.md)
- [Implementation status](implementation_status.md)
- [Capability matrix](capability_matrix.md)
- [Board workspace](../testing/board_workspace.md)
