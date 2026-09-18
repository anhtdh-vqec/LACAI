# Per-source session worker

`source_session_worker` runs one serialized `source_session_port` on its own thread so a
blocking backend call inside a session step (for example a synchronous `graphExecute`)
never stalls the control/source scheduler.

**Status:** logic-tested — device-free source delivered and tested; the supervisor async
mode drives sessions through it, and the service exposes it through
`--source-execution threaded`. **Layer:** app.
**Source:** `src/app/session/vqec_vision_source_session_worker.{hpp,cpp}`.

## Responsibility

- Own exactly one serialized `source_session_port` on one worker thread.
- Keep step requests and completion polling non-blocking for the control/source scheduler.
- Must not expose a second concurrent caller to the session; the session still sees a
  single serialized caller, so its ownership rules are unchanged.

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

`tests/unit/application/vqec_vision_source_session_worker_test.cpp`: control thread returns while the
session is blocked (measured), rejects a second step in flight, `pending` before a
completion, stop/drain ordering, and exception-to-completion. Neutral and expanded eSDK
QEMU.

## Limits and next work

- The supervisor async path (`mssup_activate` with `use_session_workers_`) drives sessions
  through these workers and is covered by
  `tests/unit/application/vqec_vision_multi_source_supervisor_async_test.cpp`.
- `application_composition` is used when the service runs `--source-execution threaded`;
  the default serialized mode calls session steps directly on the control thread.
- Automatic restart/backoff and epoch replacement remain open.

## See also

- [multi-source supervisor](multi_source_supervisor.md)
- [application composition](application_composition.md)
