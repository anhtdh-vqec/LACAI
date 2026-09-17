# Multi-model source session

`multi_model_session` is the default source-session design for an admitted source with
1..16 model graphs. It implements `source_session_port`, so the existing process-level
`multi_source_supervisor` can supervise a mixture of single-model compatibility sessions
and multi-model sessions without depending on Camera Service or vendor types.

**Status:** source-delivered — portable lifecycle source delivered; fake-port tests pass
and the combined person+FD/FR path has live compatibility-source evidence on QCS6490.
Released-FW recovery and acceptance remain pending. **Layer:** app.
**Source:** `src/app/vqec_vision_multi_model_session.{hpp,cpp}`.

## Responsibility

- Own one admitted FW RAW source through preflight, serial graph startup, running
  progress and one common drain path.
- Implement `source_session_port` so the process-level `multi_source_supervisor` can
  supervise a mixture of single-model compatibility sessions and multi-model sessions.
- Keep the source lease, graph adapters and private retention domains alive until stopped
  or until an external BSP recovery procedure proves DMA has ceased.

## Preflight and startup order

Before the first FW `StartStream`, the session validates the source is idle and every graph:

- has a unique borrowed port and cycle identity and begins empty;
- passes adapter activation checks, plan/source-binding checks and tensor-output budgets;
- uses the same source geometry and rational FPS as every other graph;
- matches the fixed 1..16 cadence count and source rate;
- has a nonzero bounded job timeout.

Only after all checks and pump composition succeed does it acquire the source once. The FW
effective profile must match the common graph profile. Graphs then progress serially by
stable model slot through configure -> load -> bind -> start. Serial startup limits each
step to one SDK operation and makes partial-start rollback deterministic; it is not a
throughput optimization and vendor calls can still block internally.

When all graphs are running, the session delegates to `multi_model_pump`. A progress report
carries due/submitted/busy masks plus one primary model slot/ticket. If a tensor result is
present, that result slot is primary; otherwise the first submitted slot is primary. The
full set of accepted tickets remains inside the pump/individual graph ledgers and is not
required for lifetime correctness.

## Stop and partial-start rollback

A stop request permanently disables new receives and enters one common drain path from any
startup/running state. It also releases the completed-result frame retained for the output
consumer; otherwise that obsolete preview owner can prevent the FW source lease from
draining after all hardware work is already complete. Graph slots are reconciled in order:

1. request graph drain/EOS when running;
2. poll real result/input completion while outstanding jobs remain;
3. poll pending load/start/drain/unload transitions;
4. unload ready, drained or faulted graphs;
5. accept only empty/configured with zero outstanding work as reconciled.

A result that becomes ready during step 2 follows an explicit `drain_policy`, never an
implicit one: `drain_and_discard` (default) reconciles ownership and drops the business
result, while `drain_and_deliver` retains the last result for the caller through
`vqec_vision_ai_appl_mmses_take_drain_result`. A model hot-swap/update uses deliver; a
shutdown uses discard.

The source is stopped only after every graph slot reaches that gate. Timeout sets
`recovery_required` but never clears a job, releases an owner, closes a handle or skips FW
lease reconciliation. First operational error is retained for diagnostics. The caller must
keep the source, graph adapters and private retention domains alive until stopped or an
external BSP recovery procedure proves DMA has ceased.

The fake-port test source covers preflight rejection before FW acquisition, failure while
starting a later graph with rollback of an already-running graph, shared-frame running
progress, ordered result correlation and graph-before-source shutdown.

The result frame owner moves from the pump's completed model slot into the session's
single `last_result_frame_`. Replacing that value releases the preceding result owner.
The model slot must not retain a duplicate after completion: a live `.98` soak exposed a
bounded-source deadlock when duplicate owners from separate model slots consumed all
three compatibility-camera in-flight buffers. The ownership regression test now releases
the consumer report and verifies the source owner expires.

## Limits and next work

- The 16-slot arrays are schema/runtime ceilings, not a Qualcomm QCS6490 capacity claim.
- Admission still needs measured graph-retention, accelerator, memory, encoder and
  thermal limits.
- The session does not authenticate configuration/model artifacts, decode tensors,
  enforce entitlements, restart a failed source or construct an executable service.

## See also

- [multi-model pump](multi_model_pump.md)
- [multi-source supervisor](multi_source_supervisor.md)
- [single-model compatibility session](camera_session.md)
