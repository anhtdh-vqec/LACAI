# Qualcomm submission lifecycle and retained faults

Status: source candidate; lead/BSP review and hardware validation pending.
This supersedes earlier no-submit statements in incremental architecture notes.

Sequence: configure -> load READY -> bind_source -> start PLAYING -> arm_submission
-> submit_frame -> poll_output/poll_jobs -> request_drain -> poll output/jobs/state
until drained -> unload NULL. Calls are serialized on one backend executor.

arm_submission requires a shared graph_retention owned by the supervisor, one unique
cycle ID, current receiver epoch and timeout. Capacity is exactly one job per graph.
It reserves one of four retention slots BEFORE allowing camera jobs. Use one retention
domain for the supervisor lifetime; never replace a full domain to bypass admission.
The graph samples its clock minus base-time for the source PTS anchor when armed.
Deadlines use caller steady-clock nanoseconds, never camera or pipeline timestamps.

submit_frame retains the existing third-stream descriptor/FD/owner, enforces input
view/allocation budgets, and uses frame_submission. A populated output ticket means
commit occurred even if push failed. No new input is admitted until both the result
and independent root-memory release are reconciled. Output correlation uses the
internal ticket; a caller's expected PTS must also match before pulling a sample.
Result vectors are CPU-owned copies; sample readers are released before bookkeeping.

poll_result(now, result) is the preferred executor entrypoint: it invokes poll_jobs
before consuming output and preserves timeout/error status while reconciling late
results. get_pending_ticket permits correlation after restoring a retained wrapper.
poll_jobs forwards input release and checks deadlines even in faulted state. A fault
stops admission but does not clear outstanding work. poll_output in faulted state
may consume a matching late result for reconciliation but does NOT publish it to
the caller. An invalid/missing result leaves the job retained; no fabricated terminal
result or BSP quiescence API is supplied. Drain requires EOS, empty sink AND no jobs.
unload rejects any outstanding job, including after a push error or deadline expiry.

Graph retention has four preallocated slots, no global state or background worker.
An armed graph destroyed before confirmed NULL transfers its entire implementation
into its reserved slot without a state transition. The supervisor can restore a
retained graph into an empty wrapper and continue polling/reconciliation; it cannot
reuse the slot until explicit NULL unload succeeds. No automatic retry/restart.

Last-resort misuse containment: destroying the retention domain while quarantined
graphs remain deliberately leaves those implementations allocated (at most four per
domain), emits a warning, and never unrefs their pipeline or camera leases. This is
NOT a recovery mechanism or production shutdown path. Supervisor must keep the domain
alive and report FW/BSP recovery-required; process exit is not certified DMA quiescence.
Total memory also includes vendor pools, so four slots is a graph-count bound, not
a measured RAM/FD budget. Pre-arm SDK init/teardown may still block; use explicit unload.

Tests must cover empty-state guards plus board success/error/timeout/late-result,
destructor retention, restore and full-domain rejection before deployment. Synthetic
bridge tests cannot certify the backend memory-retention/cache contract. FW P0
disconnect/pool-reuse issues and trusted source-binding evidence remain deployment gates.

Synthetic lifecycle test source now covers successful submit/result/drain/unload,
timeout preventing unload, destructor retention, restore/late-result discard, full
domain rejection and slot reuse. A separate test-only library compiles plugin_graph
with VQEC_VISION_AI_GRAPH_TEST_FIXTURE; it uses appsrc -> appsink with zero-filled
test bytes advertised as FLOAT32, not a mock of Qualcomm numerical inference.
The production backend target has no fixture definition or runtime fallback.
No compile or test execution was performed in this implementation slice.
