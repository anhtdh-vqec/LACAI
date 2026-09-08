# Private bounded appsrc submission primitive

Status: implementation candidate; lead/platform ownership review and board tests pending.
Now called by plugin_graph through arm_submission/submit_frame. See
[graph submission lifecycle](qualcomm_submission_lifecycle.md) for retained faults,
drain gates and mandatory supervisor domain ownership. Board qualification is pending.

The private helper performs reserve -> tracked FD wrap -> commit -> appsrc push.
One serialized caller owns the appsrc, submission_window and one frame_submission
record per outstanding job. No other producer may push to that appsrc or mutate
the ledger concurrently. The window bounds live jobs; max_frame_bytes bounds each
valid memory view and the bridge profile separately bounds allocation bytes.
Reject admission BEFORE allocating/duplicating memory when capacity is exhausted.
appsrc block must be false; this helper adds no blocking wait or hidden input queue.
It does not impose a bound on downstream vendor pools or SDK execution time.

Caller preconditions: configured window with correct source epoch and graph-running
time anchor, validated source binding/model, active source and a graph lifetime owner
that outlives every submitted read and result. The helper neither establishes sync
nor validates a DMA-BUF exporter, colors, source caps or pipeline health.

Return semantics:

- Before commit, an error leaves the output record empty. A reserved slot is cancelled
  on wrap error or C++ exception. Cancellation does not rewind accepted PTS/job IDs.
- After commit, the record is populated BEFORE push, even if push returns an error.
  gst_app_src_push_buffer consumes its buffer reference on every flow return.
  Non-OK flow faults admission but NEVER cancels the committed slot or reports
  input/result completion. Caller must retain the record and reconcile it.
- Successful push means queued, not inference completed. The record holds a release
  observer, not the camera owner; root GstMemory retains the owner through readers.

poll_input forwards observed root-memory release. complete_result requires exact
pipeline PTS and may run only after all output sample/memory readers are released
or terminal output discard is proven by the graph recovery owner. This method checks
correlation/bookkeeping only; it cannot establish that proof. Duplicate results and
stale tokens fail. A record can be reset only after both events; repeated input polling
is idempotent. Fault/timeout stops new admission without cancelling existing work.

Do not destroy the appsrc/pipeline to manufacture input completion. The helper owns
neither the graph nor its shutdown. Graph integration now provides explicit retained
fault state, drain gates and a four-slot retention domain. Direct callers of this
primitive must supply equivalent lifetime ownership. No forced ACK, detached cleanup
thread or process-reset guarantee is added. See graph lifecycle for last-resort leakage
if the supervisor incorrectly destroys a domain with retained resources.

Synthetic tests use a temporary ordinary file and appsrc with no hardware consumer:
accepted queue, capacity, original owner retention, error before/after commit, PTS,
input/result ordering and timeout. Flushing that synthetic source is test cleanup,
not evidence that flushing a Qualcomm pipeline cancels device access.
