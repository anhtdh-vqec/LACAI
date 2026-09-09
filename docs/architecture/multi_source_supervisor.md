# Multi-source supervisor

Status: source implementation delivered; Linux SDK compilation, integration and board
qualification are pending.

`multi_source_supervisor` is the process-level fairness and fault-isolation layer above
one `source_session_port` per admitted FW RAW source. The port may be implemented by the
current single-model `camera_session` or a multi-model fan-out session. Every
implementation consumes the same RAW NV12/FD descriptor/lease contract. The supervisor
does not know whether FW produced that source from a sensor or an RTSP decoder.

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

The report always carries the numeric source index and the source's original status. A
source-level error is isolated: its session already enters its own drain path, healthy
slots continue to progress, and the supervisor returns `pending` rather than converting
that source fault into a process-wide failure. The caller must publish the reported fault
and must use `has_result` before consuming the returned tensor. Invalid supervisor state,
time or binding remains a supervisor API error.

## Stop and recovery

Global stop latches `request_stop` for every bound session without issuing FW RPC in that
call. Later round-robin steps drive actual graph drain, hardware completion reconciliation,
FD ACK and FW lease release. The supervisor reaches `stopped` only after every source is
observed stopped. A timeout/recovery flag in one slot never authorizes clearing its graph,
closing a DMA-BUF early or stopping other slots.

Per-source automatic restart, backoff, epoch replacement and FW/BSP recovery are not part
of this slice. A failed slot stays stopped until a future deployment-revision replacement
creates new owners. This prevents stale descriptors, model outputs and output generations
from being retagged as a new source cycle.

## Runtime invariants

- source count is 1..16 and every declared index is bound exactly once before activation;
- deployment/catalog revision and numeric index remain stable for the supervisor lifetime;
- one call performs at most one session progress action, except global stop latching;
- no stopped slot is selected while another slot still needs progress;
- tensor output is correlated by the report's source index, never by product type;
- snapshots are serialized diagnostics, not concurrent synchronization primitives;
- no zero-copy, throughput or 16-source board-capacity claim follows from this scheduler.

`raw_source_ref` now resolves through the bounded adapter described in
[RAW-source resolution](raw_source_resolution.md); the released FW registry RPC and
transactional session-owner construction remain pending. The portable `multi_model_pump`
now fans one received frame out to due running graphs while holding one shared lease until
every graph releases it. The remaining source-session slice must configure/start/drain
those graphs around one FW acquisition and expose the pump's numeric result slot through
`source_session_port`. RTSP URI, credentials, codec and decoder state remain outside AI APP.
