# Single-camera session coordinator

This document defines the composition-level coordinator that drives one camera acquisition and
one inference graph through a full lifecycle. It is the current one-model compatibility path
behind the generic `source_session_port`.

**Status:** logic-tested — synthetic target test passes for initial guards, stop-before-start,
stable stop deadlines, uncertain acquisition recovery and camera release without loading QTI;
live FW/ownership validation remains pending. **Layer:** app. **Source:**
`src/app/session/vqec_vision_camera_session.{hpp,cpp}`.

## Responsibility

- Exclusively borrow one initially idle `raw_source_port` and empty `inference_graph_port`.
- Implement the generic `source_session_port` as the current one-model compatibility path so the
  multi-source supervisor does not depend on Camera or Qualcomm types.
- Own the start/receive/submit/result/stop progress only; it is composition-level orchestration,
  not process supervision or an executable service.
- Do not issue RPCs, cancel readers or release a lease from the destructor.

The platform composition owns both concrete adapters and any private safety-retention domain
until stopped; destroying the coordinator does not issue RPCs, cancel readers or release a
lease. A future multi-model source session can replace this implementation without reacquiring
the same FW RAW source per model.

## Progress and snapshot semantics

- Successful intermediate startup/stop actions return pending until the session reaches
  running/stopped.
- A running step may return ok with a submission or result report; callers must inspect
  `report.has_result` before publishing a tensor.
- Errors remain errors even if cleanup advances.
- `request_stop` returning ok means only that the request was latched, not that shutdown
  completed.

`get_snapshot` is a serialized, read-only snapshot of session state, graph state, Camera state,
graph jobs, Camera readers, recovery flag and first error code. No RPC, polling, FD release or
mutation is performed. It is not thread-safe against concurrent progress; take snapshots on the
same executor. First startup retry failure is retained for diagnostics even when the subsequent
retry succeeds; `last_error` is historical, not the current health state. Stop deadline expiry is
recorded if no earlier error exists. No raw frames, tensor values, handles, paths or biometric
data are included.

## State machine and configuration

States: `idle -> acquiring -> configuring -> loading -> binding -> starting -> running
-> draining_graph -> releasing_camera -> stopped`. One session owner performs one acquisition
cycle and never restarts itself. After a completely drained `source_lost` terminal state, the
service-generation owner may destroy this session and construct a fresh generation after its
configured backoff; it never reuses this owner.

Config includes an explicit plan, source binding, ordered output specs, cycle ID, job timeout,
startup timeout, stop timeout and bounded RPC timeout. Initial plan and binding and full tensor
output-contract validation precede StartStream. Both session and graph use core
`tensor_contract`, including shape/name/aggregate-byte checks. After acquisition the effective FW
width, height and rational FPS must match the plan; no silent source/model reconfiguration.

`step(now, result, report)` executes one state action. Camera start/stop makes at most one RPC per
step; SDK READY/PLAYING/NULL requests may synchronously block, so the caller must use a backend
executor, not assume a hard wall-time bound. Caller time is monotonic nanoseconds;
backwards/overflow-prone inputs fail without lifecycle mutation. Runtime output is published only
while running. Stop discards late results while continuing the completion ledger; it is not an
entitlement system.

## Stop and reconciliation

`request_stop(now)` is idempotent, permanently disables pump receives and sets a stop deadline
once. During startup it never resumes startup actions. A starting graph is polled until the state
transition resolves, then drained if PLAYING. A loading/READY graph with no submitted work can be
explicitly unloaded. Outstanding jobs are always reconciled before unload; faulted graph output is
discarded, not fabricated.

Only observed empty/configured graph plus zero jobs permits `releasing_camera`. `source.stop` then
performs its own cross-session reader gate and uncertain Start/Stop RPC reconciliation. Stop
timeout sets `recovery_required` but still permits subsequent cleanup progress. It never clears
jobs, resets the domain, disconnects early or calls StopStream while the graph has outstanding
work. Reaching stopped clears the current recovery-required flag; the first operational error
remains available for diagnosis. Startup timeout or an operational error automatically enters
`draining_graph`. An uncertain StartStream remains owned by `source_lifecycle` and is reconciled
during stop.

## Limits and next work

- No automatic retry cycle, new request IDs, BSP reset, process kill, configuration loader,
  decoder, event routing or service main is introduced.
- Full socket/model startup and pending-hardware teardown still require integration tests.
- In-process replacement of a completely drained source-loss generation is delivered and passed
  compatibility-board smoke. Per-session retry, BSP reset and released-FW recovery remain open.

## See also

- [Combined Camera control/media lifecycle](camera_source_lifecycle.md)
- [Camera third-stream to Qualcomm graph pump](camera_graph_pump.md)
- [Multi-source supervisor](multi_source_supervisor.md)
- [Inference graph port](inference_graph_port.md)
