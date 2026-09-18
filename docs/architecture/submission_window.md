# Bounded submission and clock mapping

This document defines the pure C++ `submission_window` bookkeeping component: one
acquisition-cycle window, its four-slot job ledger, the two-notification completion rule and
the mapping from source PTS to pipeline PTS. It is a ledger, not a scheduler or a device
completion implementation.

**Status:** logic-tested — the existing unit/contract binary passes in the QCS6490 board
smoke suite; this is ledger logic evidence only. Encoder hardware wiring is still pending.
**Layer:** core. **Source:** `src/core/inference/vqec_vision_submission_window.cpp`,
`tests/unit/core/vqec_vision_submission_window_test.cpp`.

## Responsibility

- Binds one nonzero acquisition-cycle ID and one receiver session epoch per window.
- Issues monotonically increasing job IDs within a fixed four-slot live-job array.
- Tracks the two independent completion notifications required per committed job.
- Maps accepted source PTS to a graph running-time anchor with checked arithmetic.
- Owns no FD, frame or GstMemory and makes no callbacks.
- Must not be used as a resource cancellation mechanism or scheduler.
- Must not introduce an unbounded queue or per-frame dynamic container allocation.

## Window, capacity and admission

Pure C++ `submission_window` is a bookkeeping component, not a scheduler or device
completion implementation. It owns no FD/frame/GstMemory and makes no callbacks.
All methods run on one serialized executor, including forwarded backend completions.
No new wire contract or change to Camera Service is introduced.

One window binds one nonzero acquisition-cycle ID and one receiver session epoch.
The supervisor supplies a unique cycle ID across window objects. Each reservation
gets a monotonically increasing job ID; a fixed array caps live jobs at four.
Default capacity is one until plugin output correlation and concurrency are proven.
`submission_limits::g_max_jobs` owns this ceiling and sizes both the base ledger and
encoder correlation table. It is independent of preview pool capacity and FW pinned-FD
limits. The named one-second timeout default is a configurable fallback, not a measured
performance requirement or proof that a timed-out device is safe to release.
No unbounded queue or per-frame dynamic container allocation is introduced.

`reserve` -> `commit` BEFORE handing memory to a backend that may synchronously complete.
If wrapping fails before commit, `cancel_reserved` returns the slot. Cancellation of
committed jobs is rejected. After commit, even push failure must be reconciled using
the backend's actual ownership/completion semantics, not `cancel_reserved`.

## Completion and ownership

Each committed job needs TWO independent notifications: input readers complete and
result consumed (or terminally discarded with no output readers). Neither alone
returns the slot. Notifications may arrive in either order, but must match the full
cycle/job token. Timeout/fault/drain stops admission without releasing slots. A BSP
quiescence proof may justify explicit completion calls; the window cannot prove it.
Frame owners remain in backend/GstMemory until actual last input read completion;
the ledger is deliberately not their resource owner. Destroying the ledger is not
a resource cancellation mechanism. Its owner must reconcile jobs first.

## Source cadence and clock mapping

Every accepted ticket preserves the exact source epoch, source frame ID and source PTS
supplied with the reservation. The default `unique_source_frames` policy requires source
timestamps to increase strictly. A dependent tensor graph explicitly arms with
`repeated_tasks_per_source_frame`; only then may consecutive jobs repeat both the same
source frame ID and the same PTS, for example one embedding job per face ROI. Equal PTS
with another frame ID, backward PTS, stale epoch, missing/overflow timestamps are rejected.
No caller fabricates cadence or changes source PTS to distinguish tasks. Rejected
reservations do not advance the clock. Canceled reservations leave a permitted time gap.

The caller supplies the graph running-time anchor after graph clock/base time are
known. First accepted source PTS maps to that anchor; later PTS maps to anchor plus
source delta with checked arithmetic. Original source epoch/frame ID/PTS remain in the
ticket for decoder and output correlation. This establishes relative timing, not
UTC/capture-time accuracy. No
wall clock is used. A separate steady-clock value is used for deadline checks and
must never be compared directly against source/pipeline timestamps.

The ticket therefore carries two distinct clocks. `pipeline_pts_ns_` is the
vendor/pipeline-domain PTS used for encoder correlation; `submitted_steady_ns_` is the
monotonic steady time captured at reserve. Latency or queue-age arithmetic must use
`submitted_steady_ns_` only. The runtime executor's `route_latency_*` metric is the
steady interval from reservation to result routing; it is not camera-to-output latency
and excludes FW capture and preview encode.

Deadline expiry marks the window faulted but keeps all outstanding slots. Inflight
budget includes prepared, submitted and result-held jobs; it does NOT budget vendor
pools, tensor bytes or camera buffers retained elsewhere.

## Wiring

The private `plugin_graph` now uses `submission_window` for tickets and completion
bookkeeping; see [qualcomm submission lifecycle](qualcomm_submission_lifecycle.md) for
current wiring and retention requirements. Encoder hardware wiring is still pending.
Source tests do not validate any device completion guarantee.

## Limits and next work

- Encoder hardware wiring is still pending.
- Source tests do not validate any device completion guarantee.
- A BSP quiescence proof is required before explicit completion calls are justified; the
  window cannot prove quiescence itself.

## See also

- [Qualcomm submission lifecycle](qualcomm_submission_lifecycle.md)
- [Frame submission](frame_submission.md)
- [Encoder admission and completion ledger](encoder_window.md)
