# Production composition foundation review — 2026-09-17

This record verifies the AI APP Plan 0 composition foundation on the approved QCS6490
`.98` target. Technical development closure is separate from released-FW qualification,
model quality and independent owner sign-off.

**Status:** board-smoke — approved eSDK/QEMU 127/127 and QCS6490 native 121/121; live
single-source production candidate drains cleanly. **Layer:** docs.
**Source:** `src/app/service/vqec_vision_service_main.cpp`,
`src/app/supervision/vqec_vision_runtime_executor.cpp`,
`src/app/cascade/vqec_vision_cascade_execution_worker.cpp`,
`src/app/session/vqec_vision_multi_model_session.cpp`,
`src/runtime/admission/vqec_vision_activation_snapshot.cpp`.

## Responsibility

- AI APP owns composition, authorization, bounded scheduling and evidence collection.
- The user, AI APP lead, requested closing the implementation blockers and establishing
  a directly observed baseline for the other teams.
- BSP+FW must review DMA/completion/reset, released camera/encoder integration and
  coexistence. AI Model must review package provenance, decoder/attribute semantics and
  golden results. This record does not invent their approvals or waive those reviews.

## Audit and corrections

The earlier closure wording at baseline `0efb457` was premature. Helper tests did not prove
production wiring, a mapped memfd did not prove seals, several embeddings did not prove
concurrent execution, and a profile label did not prove measured hardware capacity.
The corrective candidate closes the following source defects:

| Finding | Correction and verification |
|---|---|
| F01 scoped association | Production now projects one complete source/usecase/feature association, resolves the exact assigned model slot, preserves attribute scopes and nonzero config/policy revisions. Feature reconciliation validates and retains it. Missing assignment or mismatched scope/revision fails closed; unit/contract tests cover these branches. |
| F02/F05 composition | Existing explicit portable IoU baseline, independent source/model owners and production rejection of unavailable/reference factories remain intact. Fake-port multi-source tests are not concurrent hardware acceptance. |
| F03 prepared output | Existing demand, TTL, scope, revoke and PTS tests remain. FR-only now binds the output gate even without feature fan-out. Async identity output uses its captured policy revision, not a harness constant. |
| F04 artifact | Existing resolver verifies the copied bytes and seals/retains the memfd before loading its descriptor path. Source and hostile-artifact tests establish that mechanism. `/proc/maps` alone is not seal verification or artifact signature acceptance. |
| F06 asynchronous cascade | Production binds the bounded worker, not synchronous coordinator calls. Pending + executing + completed batches share one admission budget. Completion preserves frame key, geometry, observations and policy revision; metrics/frame-store access is synchronized. No timeout detach/UAF is permitted. |
| F06 shutdown | Stop closes cascade admission and joins workers before primary/source drain. Late primary results retire their exact frame. After graph reconciliation, session clears the pump's completed primary copies; regression checks the owner survives submitted work and expires after drain. |
| F07 delivery | Existing bounded production event seam remains explicit acceptance/handoff/discard, not durable downstream delivery. UDS/ACK/replay/evidence receipts remain Plan 3. |
| F08 resource | Tensor admission now sums source tensor budgets instead of model-context resident bytes. Preview CLI surface count must equal the admitted deployment. Profile is restricted to one observed source; a 16-source deployment is rejected. |

The worker executes secondary faces serially within one graph. It is asynchronous relative
to the control loop, not a thread pool or proof of simultaneous NPU face execution.
Join timeout retains the live worker and its borrowed owners. Unresolved destruction is
fail-stop, never detach/free underneath a running thread; it is not a BSP DMA reset.

## Validation results

- Approved eSDK full cross-build and QEMU/CTest: **127/127 passed** after the corrective
  source changes, including D-Bus logic tests. No host-compiler build was used.
- QCS6490 `.98`, `/opt/lacai`: **PASS=121 FAIL=0**, using the current cross-built
  native executable set and current example-profile fixture.
- One compatibility camera source, 1920x1080 NV12 at 30 FPS, YOLOv8n person + SCRFD face,
  FastCV alignment + EdgeFace embedding, portable IoU and QTI H.264 ring output.
  Final run uses two preview surfaces, matching the admitted deployment.
- Final live candidate exited **0** with `service stopped=true`, `first_error=0`:
  600 service-loop iterations, 442 routed reports, **282 cascade tasks / 282 embeddings**,
  `cascade_failed=0`, no stale-policy FR denial. Route latency: average 59236 us,
  minimum 33411 us, maximum 124475 us over 295 primary samples. This is reservation-to-route
  latency, not capture-to-preview latency; executor step metrics exclude completion-only polls.
- Host `ffprobe` against the board ring reader at `/live/ai/detect0` reports
  `codec_name=h264`, `width=1920`, `height=1080`, `r_frame_rate=30/1`.
  This proves that compatibility output, not released RTSP conformance or sustained frame rate.
- Board traces: `/opt/lacai/out/plan0_service_final_verified.log`,
  `plan0_native_closure.log`, `plan0_measurements_final.log` and
  `plan0_ring_rtsp_verified.log`. They contain no published embeddings/gallery entries.
- Resource observations and each ceiling's provenance are recorded in
  [hardware admission profile](../architecture/hardware_admission_profile.md#qcs6490-98-observation-2026-09-17).
  RSS/HWM is not all accelerator/driver memory; CPU memcpy is not sustained DDR capacity.
- Source layout, documentation layout and `git diff --check` are required before commit.
  The Bash source-layout equivalent is used because PowerShell is unavailable.
- Intermediate failures were fixed, not hidden: FR-only revision 0, leaked pump drain owner,
  stale board profile fixture and a synthetic-clock worker-drain test. The existing async
  supervisor test also used short busy-spin loops that could outrun OS thread scheduling
  under concurrent board load; bounded polling now gives worker threads a scheduling interval.

## Acceptance oracle and gates

| Gate | AI APP development decision | Retained external condition |
|---|---|---|
| P0-A authority/composition | Pass for scoped immutable startup/control generation, explicit factories and owner isolation | Trusted provisioning and contract owner review in Plan 1 |
| P0-B output/artifact | Pass for prepared scoped output and verified sealed-copy model loading | Signature/key infrastructure, released-FW output and model golden acceptance |
| P0-C lifetime/resource | Pass for bounded async work, completion-preserving drain and single-source observation profile | BSP completion/reset review and sustained/coexistence capacity in Plans 1/5 |
| P0-D validation | Pass: 127/127 eSDK, 121/121 native, live clean stop and preview; async supervisor 10/10 repeat runs; surface mismatch rejected | Independent BSP+FW and AI Model sign-offs are not recorded |

O01–O08 are supported by named authority, overlay, artifact, factory, worker, executor,
frame-store, session, delivery-seam and activation-snapshot tests. Board evidence is
limited to the workload above; unavailable production feature factories still reject
activation. Neither all 18 usecases nor traffic-camera quality is accepted by Plan 0.

## Unblock decision

**UNBLOCKED for AI APP development of Plans 1–5 on this technical foundation.**
Plan 0's source/wiring defects are closed by the current candidate and results above.
Status remains `board-smoke`, not `accepted`: independent owner reviews and
released-FW/product qualification are mandatory downstream gates, not administrative
approvals inferred from silence. No production rollout is authorized by this record.

## Closure evidence register

| Area | Named tests / evidence | Review scope |
|---|---|---|
| Authority | `vqec_vision_usecase_activation_test.cpp`, `vqec_vision_feature_activation_manager_test.cpp` | AI APP lead; entitlement boundary owner |
| Output/artifact | `vqec_vision_overlay_preparation_test.cpp`, artifact resolver tests, RTSP observation | AI APP; BSP+FW media; provenance/security owner |
| Cascade/ownership | `vqec_vision_cascade_execution_worker_test.cpp`, `vqec_vision_runtime_executor_test.cpp`, `vqec_vision_multi_model_session_test.cpp`, frame-store tests, live clean stop | AI APP; BSP+FW completion/reset |
| Admission | `vqec_vision_activation_snapshot_test.cpp`, profile JSON and observation table | AI APP baseline; BSP+FW/AI Model workload review |
| Candidate | Corrective source commit and staged service/profile SHA-256 below | Exact-revision review, no push implied |

Corrective source commit: `5237c18` (local, not pushed).
Final staged service SHA-256:
`346d1896a28063d5fd771cd66e2cd3c62819d61e87a0f3945afcfd4d413f3d41`.
Final profile SHA-256:
`a73839458a66121ee28bd6216f27d1fdb8b8330e09c08ea34ca2364f93a5842e`.

## Limits and next work

- Profile ceilings are validated policy and declared-workload bounds where direct device
  measurement is unavailable; they are not all measured hardware limits.
- Signed provisioning, full feature algorithms, production MOT/golden quality, durable
  event transport, sustained fault/thermal soak and released-FW coexistence remain open.
- No zero-copy, measured DSP preprocessing, CPU-target achievement or maximum multi-source
  throughput claim is made.
- The canonical board service is not overwritten during candidate testing. Candidate
  staging is temporary under `/opt/lacai`; tested native binaries/profile fixtures may remain.
- Test reader/camera processes were stopped and temporary candidate binaries removed;
  they can be reconstructed from the local eSDK build. During compatibility-camera wrapper
  teardown, `cam-server` reported pending-buffer/DeleteVideoTrack timeouts and subsequently
  had a different PID. No camera-server reset/restart was requested by this agent. This is
  an unresolved camera teardown/recovery observation requiring BSP+FW review; its cause has
  not been isolated. It is not a service clean-stop failure or
  evidence that released Camera Service completion/reset is accepted.

## See also

- [Architecture improvement plans](../planning/architecture_improvement/README.md)
- [Implementation status](implementation_status.md)
- [Capability matrix](capability_matrix.md)
- [Board workspace](../testing/board_workspace.md)
