# Encoder admission and completion ledger

The encoder window is a source-only admission and completion ledger built on
`submission_window`. This document defines what it reserves, how encoder events are
validated and applied, and how input/result completion stays independent.

**Status:** logic-tested — source-only, not connected to an encoder or ring. The
duplicate/fault-retention unit binary passes on QCS6490. **Layer:** core. **Source:**
`src/core/vqec_vision_encoder_window.cpp`,
`tests/unit/vqec_vision_encoder_window_test.cpp`.

## Responsibility

- Reserves encoder jobs and a bounded aggregate input-byte budget before surface
  allocation.
- Uses `submission_window` for one-shot cycle identity, monotonic PTS mapping and
  reserve/commit/cancel.
- Tracks independent input and result completion.
- Must not perform hardware calls or own image memory.
- Must not reuse capacity before both completions.
- Must not resubmit the same token.

## Event application

`apply_event` now validates neutral backend events and forwards valid completions into
this ledger. `output_ready` must be handled/discarded by the caller before applying it;
this method neither publishes nor queues H264. Exact frame metadata is validated against
the stored reservation. Duplicate/stale/malformed events latch fault admission without
releasing reservations. A fault event also latches fault and returns its error, never
completes a job. Later proven completions can reconcile retained entries; they do not
restart admission. Direct completion methods remain for explicit pre-device rejection
reconciliation. The duplicate/fault-retention unit binary passes on QCS6490.

Before delivering output, call `validate_event`: it performs non-mutating structural,
committed-token, duplicate and AU frame/profile validation. Then handle output and
`apply_event` on the same serialized owner without intervening ledger mutation. This
order prevents discovering a duplicate only after writing it to a sink. Preflight is
not entitlement authorization and grants no completion. On failed preflight, do not
dispatch; `apply_event` can latch the fault while preserving reservations. The eventual
runtime must enforce this sequence; no output event pump is wired yet.

## Configuration and reservation

Configure one camera/channel/source epoch and fixed even NV12 geometry <=8192 per axis.
Capacity is 1–4 jobs; aggregate packed input budget is explicit and <=256 MiB.
Each surface must fit the CPU surface 64 MiB limit. These are safety ceilings, not
measured capacity. Source profile change needs drain and a fresh cycle/ledger.

Call reserve BEFORE allocating a surface. No consumer demand returns pending without
reserving or consuming source PTS. Full jobs/byte budget returns `resource_exhausted`.
On success the ticket binds frame identity and encoder PTS; exact frame key is retained
until completion. Supply independent current consumer demand, not an authorization flag.
Authorization/freshness still needs the output gate and preview validator.

## Commit and completion

Allocate/render/seal input only after reservation. Cancel only if hardware submission
has never happened. Commit BEFORE calling an encoder that may accept input even when
reporting an error. After commit, errors/timeouts/stop do not cancel or free accounting.
Complete input only on proven last-reader completion. Complete result after validating
the correlated H264 envelope and finishing result handling. A dropped/no-output job
must use explicit `complete_dropped_result` after backend proves no output will arrive;
this is not input completion. Do not infer it from timeout or a leaky queue alone.

Accounting conservatively retains the full input byte reservation until BOTH events
complete. This may underutilize memory, but does not reuse capacity prematurely.
Reservations consume unique tickets and strictly increasing source PTS even if cancelled.

`begin_submission` is a one-shot marker after commit and immediately before backend
submit. It rejects uncommitted, already attempted, partially completed, drained or
faulted jobs. It changes no ownership or completion accounting. The marker is never
rolled back: even an exception or rejection requires explicit reconciliation, never
resubmission of the same token. A repeated-call rejection must NOT synthesize completion
for an earlier accepted attempt. Concrete submission composition must enforce this guard;
direct backend callers are not automatically protected by declaration alone.

`begin_input` is the preferred full-envelope entry. The ledger retains the pipeline PTS
issued at reserve time, reconstructs the independently expected ticket/frame/profile,
and validates pixels and caller-supplied binding generation before the one-shot marker.
Invalid metadata does not consume the submission attempt or complete a reservation.
It performs no backend call; the runtime must submit only after success and must not
reconcile a failed duplicate attempt as rejection of the original accepted job.
No FIFO output matching or repeated-PTS fallback. Each timestamp supplied to reserve/
deadline polling is in the same monotonic domain and cannot move backwards.

The serialized runtime must keep the ledger and actual input owners alive through
drain. Destroying this bookkeeping object cannot release pixels (it owns none), but
would lose correlation; it is forbidden while outstanding jobs exist. Downstream
hardware must independently retain sealed owners.

An optional FW SDK ring sink exists in `adapters/fw_output`. The neutral encoder port and
submission/event/drain helpers exist; concrete encoder and live ring lifecycle remain missing.

## Limits and next work

- No output event pump is wired yet; the eventual runtime must enforce the
  validate/handle/apply sequence.
- Concrete encoder and live ring lifecycle remain missing.
- This is not a pool, memory recycler, quiescence proof, process-crash recovery or global
  allocation limit outside this window.
- Destroying the ledger while outstanding jobs exist is forbidden.

## See also

- [Bounded submission and clock mapping](submission_window.md)
- [Encoder backend lifecycle port](encoder_backend.md)
- [Encoder input preparation composition](encoder_preparation.md)
- [FW ring sink](fw_ring_sink.md)
