# Bounded inference worker

Status: device-free source delivered and tested; not yet wired into the production pump.

`inference_worker` moves a blocking backend call off the control/source executor. The
control thread calls `submit()` and gets control back immediately; one or more worker
threads run `inference_work_executor::execute`; the control thread collects results with
non-blocking `poll()`. It is vendor-neutral: jobs carry identity, ordering and deadline,
never a tensor or vendor type.

Source: `src/runtime/scheduler/vqec_vision_inference_worker.{hpp,cpp}`.

## Why

The `inference_graph_port` shape is asynchronous (`arm`, `submit`, `poll`, `outstanding`),
but a synchronous backend still runs inference inside `submit`. With one serialized
supervisor step, a slow model blocks every other source. This module decouples the two
without requiring the QNN async API.

## Subject

```text
control/source executor            worker pool                 backend
        │                               │                        │
   submit(item, qos) ──► bounded queue ──► execute(item) ────────┘
        │                               │
   poll(result) ◄──── bounded completions
```

## Bounds and policies

- Fixed worker count (1..8) and fixed queue/completion capacities; no thread-per-frame.
- `drop_if_busy`: reject with `resource_exhausted` when the queue is full.
- `latest_wins`: supersede a queued item from the same source/model slot (emits a
  `superseded_` completion) and enqueue the newer one.
- `must_process_once` and `event_triggered`: rejected `unsupported` because they need a
  durable queue; they are not silently downgraded.
- Completions carry `cancelled_`, `superseded_` and `stale_epoch_`.
- `set_source_epoch` is monotonic per source slot; a completion whose epoch differs from
  the current one is flagged `stale_epoch_`, so an old-epoch result never silently attaches
  to a new epoch.

## Lifecycle

`start` once, then `submit`/`poll`. `request_stop` stops accepting, cancels queued items
into cancelled completions and wakes workers; `drain` joins them. The destructor calls both
and never throws. A backend exception is converted to an `io_error` completion, not a
worker death.

## Tests

`tests/unit/vqec_vision_inference_worker_test.cpp` covers configuration/identity rejection,
success, queue-full rejection, latest_wins supersession, stale-epoch flagging, executor
failure, slow-backend non-blocking submit plus two-source progress, and stop-cancel/drain.
Runs in the neutral and expanded eSDK QEMU configurations.

## Remaining

- Wiring: the pump/graph must be driven from the worker (or the graph moved behind a
  non-blocking command facade) so the production path actually uses this module. Until then
  the module is tested but not on the running path.
- Deadline expiry reporting needs the deterministic clock from the device-free plan before
  it can be asserted without wall-clock flakiness.
