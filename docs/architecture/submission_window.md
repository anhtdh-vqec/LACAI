# Bounded submission and clock mapping

Pure C++ submission_window is a bookkeeping component, not a scheduler or device
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

reserve -> commit BEFORE handing memory to a backend that may synchronously complete.
If wrapping fails before commit, cancel_reserved returns the slot. Cancellation of
committed jobs is rejected. After commit, even push failure must be reconciled using
the backend's actual ownership/completion semantics, not cancel_reserved.

Each committed job needs TWO independent notifications: input readers complete and
result consumed (or terminally discarded with no output readers). Neither alone
returns the slot. Notifications may arrive in either order, but must match the full
cycle/job token. Timeout/fault/drain stops admission without releasing slots. A BSP
quiescence proof may justify explicit completion calls; the window cannot prove it.
Frame owners remain in backend/GstMemory until actual last input read completion;
the ledger is deliberately not their resource owner. Destroying the ledger is not
a resource cancellation mechanism. Its owner must reconcile jobs first.

Every accepted ticket preserves the exact source epoch, source frame ID and source PTS
supplied with the reservation. Source timestamps must be present and strictly increasing
among accepted jobs. Missing, repeated, backward, stale epoch or overflow timestamps are rejected;
no fabricated cadence or receive-time fallback. This is a single-source raw image
policy, not a generic reordered/B-frame video policy. Rejected reservations do not
advance the clock. Canceled reservations leave a permitted time gap.

The caller supplies the graph running-time anchor after graph clock/base time are
known. First accepted source PTS maps to that anchor; later PTS maps to anchor plus
source delta with checked arithmetic. Original source epoch/frame ID/PTS remain in the
ticket for decoder and output correlation. This establishes relative timing, not
UTC/capture-time accuracy. No
wall clock is used. A separate steady-clock value is used for deadline checks and
must never be compared directly against source/pipeline timestamps.

Deadline expiry marks the window faulted but keeps all outstanding slots. Inflight
budget includes prepared, submitted and result-held jobs; it does NOT budget vendor
pools, tensor bytes or camera buffers retained elsewhere.

The private plugin_graph now uses submission_window for tickets and completion bookkeeping;
see qualcomm_submission_lifecycle.md for current wiring and retention requirements.
Encoder hardware wiring is still pending. Source tests do not validate any device
completion guarantee, and C++ tests have not been executed in this environment.
