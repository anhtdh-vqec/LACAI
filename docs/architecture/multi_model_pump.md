# Multi-model RAW frame fan-out

`multi_model_pump` is the bounded frame-path primitive for one logical FW RAW source and
1..16 already-started model graphs. This document defines its one-receive shared-lifetime
rule, its bounded scheduling and its composition boundary.

**Status:** source-delivered — portable source delivered; its fake-port binary passes
natively on QCS6490. **Layer:** app. **Source:**
`src/app/vqec_vision_multi_model_pump.cpp`,
`tests/unit/vqec_vision_multi_model_pump_test.cpp`.

## Responsibility

- Receives at most one new RAW frame per step and submits the same `raw_frame` view to
  every due graph that has capacity.
- Is product-origin agnostic: sensor capture and FW-decoded RTSP both enter through the
  same `raw_source_port`.
- Does not configure/load/start/drain/unload graphs or acquire/release FW.
- Keeps model strings off the frame path and allocates no container on the frame path.

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

When a graph result becomes ready, the pump moves that slot's retained frame into the
single serialized result report. The downstream session may retain that exact frame for
output/cascade correlation, but the completed model slot keeps no second owner. This
transfer is required for bounded FW producers: stale owners from several model slots must
not consume every in-flight camera buffer and prevent the next receive that would replace
them.

Every received frame also replaces one source-local, latest-wins preview mailbox. The
serialized output owner takes that frame independently of model cadence and renders it
with the newest completed observation snapshot. A 1 FPS model therefore does not force a
30 FPS camera preview down to 1 FPS. Taking or replacing the mailbox explicitly releases
its owner; stop clears it before source reconciliation.

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
- each tensor binding owns one persistent preprocessing output buffer, reused across frames
  because the pump only preprocesses/submits when that graph is not outstanding; it never
  reuses an input a graph may still read;
- a graph whose dispatch policy is `latest_wins`/`replace_pending` parks the newest due
  preprocessed input in a one-slot mailbox (`pending_model_mask`) and submits it when the
  graph frees, so a slow model keeps the newest frame instead of dropping it;
- cadence advances once a frame is received, including skipped/busy selections;
- preview storage is one frame per source and replacement drops stale preview work instead
  of creating an output backlog;
- every newly due graph is armed before the first submit, so shared retention-capacity
  rejection cannot occur after an earlier graph has already accepted that frame;
- first hard source, cadence or graph error latches pump failure; the owning source session
  must stop acquisition, drain all graphs and reconcile the FW lease.

## Parallel root-model execution

Production may explicitly select parallel root-model execution. The pump then creates one
persistent, joined worker per active model during activation; each worker owns only its
model's preprocessing buffer and graph calls. A slot holds at most one frame, accepts only
`drop_if_busy`, and never creates a stale backlog. The serialized pump thread still owns
cadence, graph arming, cascade admission and result ordering. Stop cancels work that has not
started, waits for submitted synchronous vendor calls to return, then releases frame owners.
The default remains serialized for deterministic fixtures and backends whose graph owners
cannot execute concurrently. This concurrency is a deployment policy, not an assertion that
one vendor execution domain supports parallel graphs.

## Report

The report includes due/submitted/busy masks, one indexed ticket per accepted graph, the
single result slot/ticket and an error slot. Each ticket preserves source epoch/frame
ID/PTS as well as the mapped pipeline PTS. It contains no model strings and allocates no
container on the frame path. Tensor extraction remains owned by each graph adapter and
currently may allocate/copy in the Qualcomm implementation.

## Composition boundary

`multi_model_session` owns graph lifecycle once per source, validates each graph before the
first FW acquisition, then supplies running owners to this pump. After every graph is
running and before any frame is received, the session calls
`vqec_vision_ai_appl_mmump_resolve_targets`, which resolves and caches each preprocessing
binding's model input identity so the per-frame path performs no metadata lookup. Per-board
admission must reduce configured model/source counts when measured graph, memory,
accelerator, encoder or thermal limits are lower than the schema ceiling.

## Limits and next work

- The shared-lifetime rule does not prove DMA-BUF import, cache coherence, hardware
  completion or end-to-end zero-copy on a board.
- Tensor extraction remains owned by each graph adapter and currently may allocate/copy in
  the Qualcomm implementation.
- Per-board admission must reduce configured model/source counts when measured limits are
  lower than the schema ceiling.
- Parallel root-model execution is a deployment policy, not an assertion that one vendor
  execution domain supports parallel graphs.

## See also

- [Model cadence](model_cadence.md)
- [Inference graph port](inference_graph_port.md)
- [RAW source port](raw_source_port.md)
- [Multi-source supervisor](multi_source_supervisor.md)
- [Multi-model result router](multi_model_result_router.md)
