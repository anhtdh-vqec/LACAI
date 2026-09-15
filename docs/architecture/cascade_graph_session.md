# Cascade graph session

The cascade graph session is the vendor-neutral lifecycle owner for one secondary model
graph. Secondary models consume tensors produced from admitted primary results, so they
must not be inserted into the full-frame cadence or RAW fan-out owned by
`multi_model_session`.

The session borrows one `inference_graph_port` and immutable activation metadata. It
validates an empty graph, then advances it through configure, load, source binding and
start using bounded, serialized `step` calls. A graph reaches `running` before the primary
source is activated. The cascade coordinator owns epoch arming and tensor submission after
that point.

Stop closes new cascade invocation at the caller, requests graph drain, consumes any
completed result needed to reconcile the graph's submission window, waits for outstanding
work, unloads, and reports `stopped` only when the graph is empty or unloaded/configured.
Startup and stop use
separate monotonic deadlines. A timeout or graph fault with an unsettled backend is reported
as recovery-required; validation failure while the graph remains empty is not. Neither
condition authorizes early destruction of a graph that may still own hardware resources.

The source binding describes the source identity and color/synchronization contract used
to construct the model plan. Tensor-based secondary submission does not grant permission
to skip the backend's READY-state binding invariant. Qualcomm, Rockchip, MediaTek and other
adapters implement the same graph port; no vendor type enters this owner.

Initial integration uses one synchronous secondary graph for each active source. Sharing a
graph across sources, asynchronous in-flight task scheduling and context sharing require a
separate admission and lifecycle design backed by target measurements.

## Ownership order

1. Platform graph/alignment/decoder owners are prepared.
2. `cascade_graph_session` starts the secondary graph to `running`.
3. The primary source composition starts and retains cascade-root frames.
4. The serialized coordinator aligns, submits, polls, decodes and completes each retained
   frame ticket.
5. Primary admission stops and all retained frame tickets drain.
6. The cascade graph session drains and unloads the secondary graph.
7. Platform owners may be destroyed.

Compile/QEMU tests prove state-machine behavior only. They do not prove QNN, HTP, DMA,
FastCV completion, performance or model accuracy on the target board.
