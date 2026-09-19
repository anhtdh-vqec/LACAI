# Cascade graph session

The cascade graph session is the vendor-neutral lifecycle owner for one secondary model
graph. This document defines how that graph is started, drained and unloaded without
joining the full-frame cadence or RAW fan-out owned by `multi_model_session`.

**Status:** logic-tested — compile/QEMU tests prove state-machine behavior only. **Layer:**
app. **Source:** `src/app/session/vqec_vision_cascade_graph_session.cpp`,
`tests/unit/application/vqec_vision_cascade_graph_session_test.cpp`.

## Responsibility

- Owns the lifecycle of one secondary model graph behind `inference_graph_port`.
- Borrows one `inference_graph_port` and immutable activation metadata.
- Validates an empty graph, then advances it through configure, load, source binding and
  start using bounded, serialized `step` calls.
- Must not insert the secondary graph into the full-frame cadence or the RAW fan-out owned
  by `multi_model_session`.
- Keeps every vendor type out of this owner.

## Lifecycle and stop

Secondary models consume tensors produced from admitted primary results, so they must not
be inserted into the full-frame cadence or RAW fan-out owned by `multi_model_session`. A
graph is prepared and bound to the executor before the first executor step, but it stays
`idle` while its primary source is acquiring or waiting for the first frame. The service
may advance the secondary graph only after the corresponding `multi_model_session` has
received, validated and released its probe frame. The cascade coordinator owns epoch
arming and tensor submission after that point. This prevents Qualcomm QNN/HTP activation
while FW is absent or has not produced media, without introducing a process startup-order
dependency.

Stop closes new cascade invocation at the caller, requests graph drain, consumes any
completed result needed to reconcile the graph's submission window, waits for outstanding
work, unloads, and reports `stopped` only when the graph is empty or unloaded/configured.
Startup and stop use separate monotonic deadlines. A timeout or graph fault with an
unsettled backend is reported as recovery-required; validation failure while the graph
remains empty is not. Neither condition authorizes early destruction of a graph that may
still own hardware resources.

## Portability and source binding

The source binding describes the source identity and color/synchronization contract used to
construct the model plan. Tensor-based secondary submission does not grant permission to
skip the backend's READY-state binding invariant. Qualcomm, Rockchip, MediaTek and other
adapters implement the same graph port; no vendor type enters this owner.

## Ownership order

1. Platform graph/alignment/decoder owners are prepared.
2. The primary source composition acquires the source and crosses its first-frame gate.
3. `cascade_graph_session` starts the secondary graph to `running` before primary model
   results can schedule secondary work.
4. The serialized coordinator aligns, submits, polls, decodes and completes each retained
   frame ticket.
5. Primary admission stops and all retained frame tickets drain.
6. The cascade graph session drains and unloads the secondary graph.
7. Platform owners may be destroyed.

## Limits and next work

- Initial integration uses one synchronous secondary graph for each active source.
- Sharing a graph across sources, asynchronous in-flight task scheduling and context
  sharing require a separate admission and lifecycle design backed by target measurements.
- Compile/QEMU tests prove state-machine behavior only. They do not prove QNN, HTP, DMA,
  FastCV completion, performance or model accuracy on the target board.

## See also

- [Cascade inference](cascade_inference.md)
- [Inference graph port](inference_graph_port.md)
- [Multi-model session](multi_model_session.md)
