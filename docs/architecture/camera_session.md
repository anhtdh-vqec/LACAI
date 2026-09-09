# Single-camera session coordinator

Source candidate; ownership review and board validation pending. camera_session is
composition-level orchestration, not process supervision or an executable service.
It exclusively borrows one initially idle raw_source_port and empty plugin_graph.
The application owns both and the shared graph_retention domain until stopped;
destroying the coordinator does not issue RPCs, cancel readers or release a lease.

camera_session implements the generic `source_session_port` as the current one-model
compatibility path. The multi-source supervisor therefore does not depend on Camera or
Qualcomm types. A future multi-model source session can replace this implementation
without reacquiring the same FW RAW source per model.

Progress semantics: successful intermediate startup/stop actions return pending until
the session reaches running/stopped. Running step may return ok with a submission or
result report; callers must inspect report.has_result before publishing a tensor.
Errors remain errors even if cleanup advances. request_stop returning ok means only
that the request was latched, not that shutdown completed.

get_snapshot is a serialized, read-only snapshot of session state, graph state, Camera
state, graph jobs, Camera readers, recovery flag and first error code. No RPC, polling,
FD release or mutation is performed. It is not thread-safe against concurrent progress;
take snapshots on the same executor. First startup retry failure is retained for
diagnostics even when the subsequent retry succeeds; last_error is historical, not
the current health state. Stop deadline expiry is recorded if no earlier error exists.
No raw frames, tensor values, handles, paths or biometric data are included.

States: idle -> acquiring -> configuring -> loading -> binding -> starting -> running
-> draining_graph -> releasing_camera -> stopped. One acquisition cycle, no restart.
Config includes an explicit plan, source binding, ordered output specs, cycle ID,
job timeout, startup timeout, stop timeout and bounded RPC timeout. Initial plan and
binding and full tensor output-contract validation precede StartStream. Both session
and graph use core tensor_contract, including shape/name/aggregate-byte checks.
After acquisition the effective FW width,
height and rational FPS must match the plan; no silent source/model reconfiguration.

step(now, result, report) executes one state action. Camera start/stop makes at most
one RPC per step; SDK READY/PLAYING/NULL requests may synchronously block, so the
caller must use a backend executor, not assume a hard wall-time bound. Caller time
is monotonic nanoseconds; backwards/overflow-prone inputs fail without lifecycle
mutation. Runtime output is published only while running. Stop discards late results
while continuing the completion ledger; it is not an entitlement system.

request_stop(now) is idempotent, permanently disables pump receives and sets a stop
deadline once. During startup it never resumes startup actions. A starting graph is
polled until the state transition resolves, then drained if PLAYING. A loading/READY
graph with no submitted work can be explicitly unloaded. Outstanding jobs are always
reconciled before unload; faulted graph output is discarded, not fabricated.

Only observed empty/configured graph plus zero jobs permits releasing_camera.
source.stop then performs its own cross-session reader gate and uncertain Start/Stop
RPC reconciliation. Stop timeout sets recovery_required but still permits subsequent
cleanup progress. It never clears jobs, resets the domain, disconnects early or calls
StopStream while the graph has outstanding work. Reaching stopped clears the current
recovery-required flag; first operational error remains available for diagnosis.

Startup timeout or an operational error automatically enters draining_graph. An
uncertain StartStream remains owned by source_lifecycle and is reconciled during stop.
No automatic retry cycle, new request IDs, BSP reset, process kill, configuration
loader, decoder, event routing or service main is introduced.

Tests in this slice cover initial guards, stop-before-start, stable stop deadlines,
uncertain acquisition recovery and completion of camera release without loading QTI.
Full socket/model startup and pending-hardware teardown still require integration tests.
