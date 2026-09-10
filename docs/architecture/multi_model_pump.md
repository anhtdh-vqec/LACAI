# Multi-model RAW frame fan-out

Status: portable source delivered; its fake-port binary passes natively on QCS6490.

`multi_model_pump` is the bounded frame-path primitive for one logical FW RAW source and
1..16 already-started model graphs. It is product-origin agnostic: sensor capture and
FW-decoded RTSP both enter through the same `raw_source_port`.

## One receive, shared lifetime

Each pump step polls existing graph results first. It receives at most one new RAW frame,
runs the fixed-capacity cadence selector once, and submits the same `raw_frame` view to
every due graph that has capacity. The const submit boundary copies only the shared owner
and descriptor/handle metadata; it does not request another FW frame per model and does not
copy pixels itself.

Every accepting graph independently retains the same owner control block. The Camera
adapter can therefore ACK the legacy frame only after the last real graph reader
releases it. This lifetime rule is required, but does not prove DMA-BUF import, cache
coherence, hardware completion or end-to-end zero-copy on a board.

## Bounded scheduling and overload

- graph bindings, arm state and submission tickets use fixed arrays with a hard ceiling of
  16 model slots;
- model slot order equals the immutable activation/cadence order;
- each call polls at most 16 graphs, returns at most one tensor result and receives at most
  one frame;
- result polling rotates after each delivered result to prevent a fast graph monopolizing
  output progress;
- if every graph has an outstanding job, the pump does not receive a frame;
- a due but busy graph skips the current frame and is recorded in `busy_model_mask`; it
  never creates a stale-frame backlog or burst retry;
- cadence advances once a frame is received, including skipped/busy selections;
- every newly due graph is armed before the first submit, so shared retention-capacity
  rejection cannot occur after an earlier graph has already accepted that frame;
- first hard source, cadence or graph error latches pump failure; the owning source session
  must stop acquisition, drain all graphs and reconcile the FW lease.

The report includes due/submitted/busy masks, one indexed ticket per accepted graph, the
single result slot/ticket and an error slot. Each ticket preserves source epoch/frame
ID/PTS as well as the mapped pipeline PTS. It contains no model strings and allocates no
container on the frame path. Tensor extraction remains owned by each graph adapter and
currently may allocate/copy in the Qualcomm implementation.

## Composition boundary

The pump does not configure/load/start/drain/unload graphs or acquire/release FW.
`multi_model_session` owns that lifecycle once per source, validates each graph before the
first FW acquisition, then supplies running owners to this pump. Per-board admission must
reduce configured model/source counts when measured graph, memory, accelerator, encoder or
thermal limits are lower than the schema ceiling.

See also [model cadence](model_cadence.md),
[inference graph port](inference_graph_port.md),
[RAW source port](raw_source_port.md), and
[multi-source supervisor](multi_source_supervisor.md). Completed tensors are mapped to
their decoder/tracker stage by the [multi-model result router](multi_model_result_router.md).
