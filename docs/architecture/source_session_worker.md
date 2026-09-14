# Per-source session worker

Status: device-free source delivered and tested; not yet wired into the supervisor.

`source_session_worker` runs one serialized `source_session_port` on its own thread so a
blocking backend call inside a session step (for example a synchronous `graphExecute`)
never stalls the control/source scheduler. Source: `src/app/vqec_vision_source_session_worker.{hpp,cpp}`.

## Why

`multi_source_supervisor::step` advances one source per call on the control thread. If a
session step contains a blocking inference call, every other source waits. The supervisor
document already names the fix: one serialized executor per source. This module is that
executor.

## Contract

- `start(session)`: one worker thread owns the borrowed session.
- `request_step(now)`: non-blocking; accepted only when no step is in flight and no
  completion is unread, otherwise `resource_exhausted` (bounded one-in-flight/one-unread).
- `poll_completion(status, result, progress)`: non-blocking; `pending` when none is ready.
- `request_stop(now)`: queues a session stop; an already-queued step may run first.
- `drain()`: stops accepting, joins the worker, discards any unread completion.
- The session still sees a single serialized caller, so its ownership rules are unchanged.

A session exception becomes an `io_error` completion; the worker never dies.

## Tests

`tests/unit/vqec_vision_source_session_worker_test.cpp`: control thread returns while the
session is blocked (measured), rejects a second step in flight, `pending` before a
completion, stop/drain ordering, and exception-to-completion. Neutral and expanded eSDK
QEMU.

## Remaining

- Wiring: the supervisor must drive sessions through these workers (request step/poll
  completion) instead of calling `step` directly, and treat a full worker as backpressure.
  Until then the module is tested but not on the running path.
