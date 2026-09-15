# Per-source session worker

Status: device-free source delivered and tested. The supervisor async mode drives sessions
through it and is unit-tested; application composition does not enable that mode on the
running service path yet.

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

- The supervisor async path (`mssup_activate` with `use_session_workers_`) drives sessions
  through these workers and is covered by
  `tests/unit/vqec_vision_multi_source_supervisor_async_test.cpp`. `application_composition`
  and the service harness do not enable it yet, so the running service still calls session
  steps directly on the control thread.
- Automatic restart/backoff and epoch replacement remain open.
