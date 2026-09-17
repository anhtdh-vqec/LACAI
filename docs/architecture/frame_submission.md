# Private bounded appsrc submission primitive

This document defines the private helper that turns a tracked frame into one admitted `appsrc`
push with an explicit reservation/commit ledger, so input ownership and completion can be
reconciled without an unbounded queue.

**Status:** logic-tested — synthetic target ownership test passes; lead/platform review and live
device-completion tests remain pending, and board qualification is pending. **Layer:** adapters.
**Source:** `src/adapters/qualcomm/vqec_vision_frame_submission.{hpp,cpp}`.

## Responsibility

- Perform `reserve -> tracked FD wrap -> commit -> appsrc push` for one outstanding job.
- Keep one serialized caller owning the `appsrc`, `submission_window` and one `frame_submission`
  record per outstanding job; no other producer may push to that `appsrc` or mutate the ledger
  concurrently.
- Bound live jobs by the window and each valid memory view by `max_frame_bytes`; the bridge
  profile separately bounds allocation bytes.
- Do not establish sync, validate a DMA-BUF exporter/colors/caps or own graph shutdown.

The private helper performs `reserve -> tracked FD wrap -> commit -> appsrc push`. Reject
admission BEFORE allocating/duplicating memory when capacity is exhausted. `appsrc` block must be
false; this helper adds no blocking wait or hidden input queue. It does not impose a bound on
downstream vendor pools or SDK execution time.

Caller preconditions: configured window with correct source epoch and graph-running time anchor,
validated source binding/model, active source and a graph lifetime owner that outlives every
submitted read and result. The helper neither establishes sync nor validates a DMA-BUF exporter,
colors, source caps or pipeline health.

## Return semantics

- Before commit, an error leaves the output record empty. A reserved slot is cancelled on wrap
  error or C++ exception. Cancellation does not rewind accepted PTS/job IDs.
- After commit, the record is populated BEFORE push, even if push returns an error.
  `gst_app_src_push_buffer` consumes its buffer reference on every flow return.
- Non-OK flow faults admission but NEVER cancels the committed slot or reports input/result
  completion. Caller must retain the record and reconcile it.
- Successful push means queued, not inference completed. The record holds a release observer, not
  the camera owner; root `GstMemory` retains the owner through readers.

`poll_input` forwards observed root-memory release. `complete_result` requires exact pipeline PTS
and may run only after all output sample/memory readers are released or terminal output discard is
proven by the graph recovery owner. This method checks correlation/bookkeeping only; it cannot
establish that proof. Duplicate results and stale tokens fail. A record can be reset only after
both events; repeated input polling is idempotent. Fault/timeout stops new admission without
cancelling existing work.

## Correlation

The retained ticket carries the RAW descriptor's session epoch, buffer ID as source frame ID,
original source PTS and mapped pipeline PTS. Result consumers reconstruct frame identity from these
recorded values plus activation-time camera/channel identity; they must not infer source identity
from pipeline PTS.

`plugin_graph` calls this helper through `arm_submission`/`submit_frame`. See the
[graph submission lifecycle](qualcomm_submission_lifecycle.md) for retained faults, drain gates and
mandatory supervisor domain ownership.

## Limits and next work

- Do not destroy the `appsrc`/pipeline to manufacture input completion. The helper owns neither the
  graph nor its shutdown.
- Graph integration now provides explicit retained fault state, drain gates and a four-slot
  retention domain. Direct callers of this primitive must supply equivalent lifetime ownership.
- No forced ACK, detached cleanup thread or process-reset guarantee is added. See graph lifecycle
  for last-resort leakage if the supervisor incorrectly destroys a domain with retained resources.
- Synthetic tests use a temporary ordinary file and `appsrc` with no hardware consumer: accepted
  queue, capacity, original owner retention, error before/after commit, PTS, input/result ordering
  and timeout. Flushing that synthetic source is test cleanup, not evidence that flushing a
  Qualcomm pipeline cancels device access.

## See also

- [Qualcomm submission lifecycle and retained faults](qualcomm_submission_lifecycle.md)
- [Legacy camera FD to private GStreamer memory bridge](dmabuf_memory_bridge.md)
- [Qualcomm stream control and output polling](qualcomm_stream_control.md)
- [Qualcomm graph model-load lifecycle](qualcomm_graph_lifecycle.md)
