# Multi-source supervisor

`multi_source_supervisor` is the process-level fairness and fault-isolation layer above
one `source_session_port` per admitted FW RAW source. The port may be implemented by the
current single-model `camera_session` or a multi-model fan-out session.

**Status:** source-delivered — source implementation delivered; eSDK compilation, service
composition and threaded execution are delivered, board qualification remains pending.
**Layer:** app. **Source:** `src/app/supervision/vqec_vision_multi_source_supervisor.{hpp,cpp}`.

## Responsibility

- Advance one already-composed session per fixed numeric source index with bounded
  round-robin fairness.
- Isolate a source-level fault while healthy slots continue to progress.
- Must not allocate a session, own an FD, load a model, call FW directly, resolve RAW
  sources or create threads. Every implementation consumes the same RAW NV12/FD
  descriptor/lease contract, and the supervisor does not know whether FW produced a source
  from a sensor or an RTSP decoder.

## Ownership and activation

The composition root first authenticates configuration, builds an immutable activation
snapshot, resolves every `raw_source_ref`, and constructs a complete source lifecycle,
vendor graph, retention domain and session for each admitted slot. It then:

1. constructs the supervisor with the exact deployment/catalog revisions and source count;
2. binds each session to its fixed activation-snapshot index exactly once;
3. activates only after all declared slots are present;
4. transfers exclusive progress authority to the supervisor until every slot is stopped.

The supervisor stores a fixed array of 16 borrowed `source_session_port` pointers. It does
not allocate a session, own an FD, load a model, call FW directly or make a string lookup on
the running path. The composition owners must outlive it and must not call a bound session
concurrently. The interface reports a numeric model slot, so future multi-model sessions do
not need hot-path string correlation.

## Progress and fairness

Each `step` advances at most one non-stopped slot, then rotates the cursor. Consequently a
busy source cannot consume an unbounded number of operations before another source is
visited. Receive and result polling inside a running session are non-blocking. Startup,
graph state changes and FW RPCs may still block for their explicitly bounded backend/RPC
timeout; true wall-time isolation requires one serialized executor per source and will be
added at the service-runtime layer.

The report always carries the numeric source index and the source's original status.
Per-source progress has fixed due/submitted/busy masks plus a primary numeric model slot;
no model string lookup is needed on the running path. A
source-level error is isolated: its session already enters its own drain path, healthy
slots continue to progress, and the supervisor returns `pending` rather than converting
that source fault into a process-wide failure. The caller must use `has_result` before
consuming the returned tensor. Invalid supervisor state, time or binding remains a
supervisor API error.

An isolated fault is never invisible: it is recorded on a bounded, independent fault
channel. `multi_source_supervisor_snapshot` carries `fault_event_total_`,
`faulted_sources_` and a per-source `source_fault_codes_` array, and
`vqec_vision_ai_appl_mssup_take_fault` pops the oldest retained
`multi_source_fault_event` (source index, code, monotonic time). A caller loop that only
checks the step code still cannot miss the error, and counters feed telemetry
(disconnect/fault totals). Fault duration and last-frame age remain session/telemetry
fields, not supervisor guesses.

## Async execution (opt-in)

`multi_source_supervisor_config::use_session_workers_` runs each bound session on its own
`source_session_worker`. `step` then polls at most one completion round-robin and requests
one non-blocking step on the next available slot, so a blocking backend call inside a slow
source cannot stall the control loop or the other sources. Faults are still isolated onto
the fault channel, stop queues a session stop per worker, and `drain()` joins every worker
(the destructor also drains). Synchronous behavior remains the default.

## Stop and recovery

Global stop latches `request_stop` for every bound session without issuing FW RPC in that
call. Later round-robin steps drive actual graph drain, hardware completion reconciliation,
FD ACK and FW lease release. The supervisor reaches `stopped` only after every source is
observed stopped. A timeout/recovery flag in one slot never authorizes clearing its graph,
closing a DMA-BUF early or stopping other slots.

The supervisor never restarts an individual slot or reuses its owners. A failed slot stays stopped
until the enclosing service replaces the entire, completely drained generation with fresh owners.
That service-level `source_lost` replacement and bounded backoff are delivered; per-slot restart,
BSP reset and released-FW recovery are not. This prevents stale descriptors, model outputs and
output generations from being retagged as a new source cycle.

## Runtime invariants

- source count is 1..16 and every declared index is bound exactly once before activation;
- deployment/catalog revision and numeric index remain stable for the supervisor lifetime;
- one call performs at most one session progress action, except global stop latching;
- no stopped slot is selected while another slot still needs progress;
- tensor output is correlated by the report's source index, never by product type;
- snapshots are serialized diagnostics, not concurrent synchronization primitives;
- no zero-copy, throughput or 16-source board-capacity claim follows from this scheduler.

## Limits and next work

- `raw_source_ref` resolves through the bounded adapter described in
  [RAW-source resolution](raw_source_resolution.md); the released FW registry RPC and
  transactional session-owner construction remain pending.
- Service-level replacement of a completely drained source-loss generation is delivered.
  Per-source restart inside this supervisor and BSP/released-FW recovery remain open.
- True wall-time isolation requires one serialized executor per source and will be added at
  the service-runtime layer.
- RTSP URI, credentials, codec and decoder state remain outside AI APP. The portable
  `multi_model_pump` fans one received frame out to due running graphs while holding one
  shared lease until every graph releases it; `multi_model_session` configures, starts and
  drains those graphs around one FW acquisition and exposes the pump's numeric result slot
  through `source_session_port`. Executable composition and live integration are delivered.

## See also

- [RAW-source resolution](raw_source_resolution.md)
- [single-model compatibility session](camera_session.md)
- [multi-model source session](multi_model_session.md)
- [multi-model pump](multi_model_pump.md)
- [per-source session worker](source_session_worker.md)
