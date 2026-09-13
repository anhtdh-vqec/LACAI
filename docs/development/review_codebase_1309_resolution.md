# Resolution log — codebase review 2026-09-13

Source review: [review_codebase_1309.md](review_codebase_1309.md). This log records the
disposition of every section. A section is `fixed` only when source, tests and docs are in
the tree and the eSDK configurations run; `planned` means the change needs a board vertical
slice, a measurement, or an explicit product/lead decision.

Evidence commands: neutral `53/53` and expanded `71/71` under eSDK QEMU. Board checks are
listed in [QCS6490 target](../testing/qsc6490_board.md) and
[QNN board validation](../testing/qnn_board_validation.md).

## Status

| § | Topic | Disposition | Commit / reference |
|---|---|---|---|
| 1 | FW / neutral core / vendor boundary | Kept as-is (review agrees) | — |
| 2 | `frame_source` IPC hardening | Kept as-is (review agrees) | — |
| 3 | ACK in final-owner destructor can fault on EAGAIN | **Fixed** | `e84f1c1` |
| 4 | Allocator created per `wrap_frame()` | **Fixed** (device measurement still open) | `cb75485` |
| 5 | Plugin backend is the production baseline | Strategy adopted; benchmark is board-gated | S02 in [optimization plan](../planning/model_agnostic_optimization_plan.md) |
| 6 | Owned QNN not a hot path | **Partially fixed**: honest capabilities + off-hot-path metadata; async/worker remain | `b20ae11`, `52fe276` |
| 7 | Blocking `graphExecute` blocks the supervisor | Planned: bounded worker/async execution | S07 (board/measurement gate) |
| 8 | Direct QNN missing production preprocess/buffer manager | Planned: FastCV/converter processor + registered buffers | S03/S05 (board gate) |
| 9 | Multi-model arm-before-submit ownership | Kept as-is (review agrees) | — |
| 10 | Cadence QoS is uniform drop-if-busy | **Fixed (contract)**: explicit per-model policy, unsupported classes rejected | `d469495` |
| 11 | No explicit secondary-inference/task graph | Planned; deferred until after the vertical slice to avoid more architecture ahead of hardware | Needs ADR |
| 12 | Supervisor faults become invisible `pending` | **Fixed**: bounded fault channel + counters | `a33a87e` |
| 13 | Stop drain semantics implicit | **Fixed**: explicit `multi_model_drain_policy` | `692cd1a` |
| 14 | Long C++ method naming | Deferred by reviewer; AGENTS mandates the scheme, needs a lead/ADR decision to change | AGENTS.md rule 2 |
| 15 | Root CMake too granular | Deferred to the engineering-debt phase (reviewer roadmap step 7) | — |
| 16 | No production executable | Acknowledged: reference harness only until the board vertical slice | S02/S10 |
| 17 | Reuse Qualcomm IM SDK output/encode plumbing | Strategy adopted for S09 | S09 (board gate) |
| 18 | Suggested roadmap | Adopted as the ordering for S02–S11 | — |
| minor | `QSC6490` vs `QCS6490` casing | **Fixed** in prose; identifiers kept | `3480be5` |

## What changed

### §3 Release ACK dispatcher (`e84f1c1`)

The final frame owner no longer calls `send()` in its destructor. It hands the ACK token to
the originating session's bounded release queue, then closes its FD. The queue sends
nonblocking, retries on `EAGAIN`, and faults only past
`camera_receiver_limits::g_release_deadline_ms` or `g_max_pending_releases`. `receive()` and
`disconnect()` pump queued releases. A full socket buffer no longer faults the session on
first `EAGAIN` and destruction never blocks. No dedicated backpressure fixture yet; the
existing ACK-on-old-session and parser-fault tests still pass.

### §4 Adapter-lifetime allocator (`cb75485`)

`dmabuf_allocator_context` plus `vqec_vision_ai_qcom_dmbrg_ensure_allocator` own one
`GstDmaBufAllocator` for the adapter lifetime. `frame_submission` holds it and `wrap_frame`
reuses it, removing per-frame allocator construction. Still to measure: actual frames/s and
allocator impact on the board.

### §6/§10 capability and QoS honesty (`b20ae11`, `52fe276`, `d469495`)

The owned QNN engine advertises only implemented operations, resolves tensor identity once
off the hot path, and the cadence scheduler now carries a per-model `model_dispatch_policy`.
Only `drop_if_busy` is implemented; `latest_wins`, `must_process_once` and `event_triggered`
are rejected as `unsupported` at activation until a bounded per-model queue exists, so the
OCR/face/event QoS gap is explicit instead of silently uniform.

### §12 Supervisor fault channel (`a33a87e`)

`multi_source_supervisor_snapshot` now exposes `fault_event_total_`, `faulted_sources_` and
per-source `source_fault_codes_`; `vqec_vision_ai_appl_mssup_take_fault` pops a bounded
`multi_source_fault_event` ring. Fault isolation is unchanged, but an isolated error is no
longer only a swallowed `pending`.

### §13 Explicit drain policy (`692cd1a`)

`multi_model_drain_policy` decides whether a result ready while draining is delivered
(`drain_and_deliver`, via `take_drain_result`) or discarded (`drain_and_discard`, default).
The choice is validated at activation and tested on both paths.

## Open items and their gates

- **Board/measurement (S02/S03/S05/S07/S09/S10):** baseline plugin vs direct-QNN vs CPU,
  production preprocess and buffer manager, registered/native buffers, execution fairness,
  hardware output/ring and admission/recovery. None can be claimed from source or QEMU.
- **Secondary inference (§11):** needs an ADR for `secondary_inference_request` (source
  epoch, frame, track, model, ROI, deadline, priority) routed back to the scheduler. Held
  until the vertical slice so the repository does not keep growing contracts ahead of
  hardware evidence.
- **Naming (§14) and CMake split (§15):** reviewer-endorsed follow-ups, intentionally after
  the end-to-end slice; the naming change also needs a lead/ADR decision because AGENTS.md
  mandates the current scheme.
- **Execution:** `qnn_inference_graph::submit_tensors` still executes synchronously inside
  the submit call; the port shape is asynchronous, the backend is not yet. This is the
  highest-impact remaining architecture/performance item (§6/§7).
