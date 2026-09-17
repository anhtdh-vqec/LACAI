# Synchronous authorized encoded dispatch

Encoded dispatch is a source-only composition that authorizes one caller-owned immutable
H264 access unit per call, queries sink demand and synchronously writes to a ring. This
document defines its inputs, authorization order, the shared policy limits and the encoder
event handling helper.

**Status:** logic-tested — source-only composition; an optional FW SDK sink adapter exists
but is not runtime-wired. Synthetic contract cases are included in the executed QCS6490
unit/contract smoke suite; they do not exercise a live FW ring. **Layer:** outputs.
**Source:** `src/outputs/vqec_vision_encoded_dispatch.cpp`,
`tests/contract/vqec_vision_encoded_dispatch_test.cpp`.

## Responsibility

- Accepts exactly one caller-owned immutable H264 AU per call.
- Validates envelope/correlation, age, generation and 1–16 scope requests.
- Runs every requested scope through the existing `output_gate`.
- Queries sink demand only after authorization and writes synchronously.
- Must not queue, retry, retain an owner or allocate a fallback internally.
- Must not prove that the renderer's scope list is complete.

## Inputs and authorization

Exactly one caller-owned immutable H264 AU
per call; no internal queue, retry, retained owner, worker or fallback allocation.
Pending means not written, not internally queued. Caller bounds retained outputs.

Inputs: independently expected frame/geometry and logical source ID, original mapping
generation, monotonic creation time/max age, and ALL rendered feature/attribute scopes.
Scope extraction is a TRUSTED renderer responsibility, not external request metadata.
H264 bytes cannot reveal what attributes were drawn. This helper does not prove that
the scope list is complete or implement the renderer/physical-to-logical source mapping.
The authorized overlay preparation result is the preferred source for this trusted scope
list; callers must retain it with the per-job render context.
For an overlay containing multiple features, use its scoped result directly so every
authorized scope is carried to the encoded dispatch context under one policy revision.
Do not expose it directly to remote callers. A production renderer must bind these
claims to the pixels before seal/encode; this integration remains missing.

Validate envelope/correlation, age, generation, then 1–16 scope requests. Every request
must use the expected logical source and the same nonzero policy revision; every scope
must pass the existing [output gate](output_gate.md). The list may include a separately
provisioned raw preview scope even for zero boxes; do not invent an implicit grant for
empty metadata.

## Shared policy limits

`output_policy_limits` in the output_gate contract owns the shared 128-byte identifier
ceiling, 64 attributes per scope, 64 policy rules and 16 rendered scopes per AU.
These are AI metadata safety ceilings, not FW protocol fields or licensed feature counts.
Rule count and rendered-scope count remain separate budgets. This extraction changes
no accepted values, authorization semantics or FW wire representation.

## Demand and write ordering

Query demand only after authorization. No consumers returns pending and performs no
write. Missing/stale generation rejects output instead of silently using a recreated
ring. Repeat authorization after demand query immediately before synchronous write.
Entire call and policy/source changes must be serialized, non-reentrant; real adapters
must not mutate authorization during write. `_now_ns` is one captured monotonic instant,
not a deadline guarantee across blocking SDK/I/O. Sink calls must meet runtime latency
budgets; no wall-clock expiry or cancellation is inferred by this source-only helper.

Return sink errors unchanged; never auto-retry after an ambiguous write. Recheck policy,
generation and age on a later caller-directed retry; original revision is immutable.
`ok` means the sink copied to ring, not network delivery. Tests use a fake sink only.

## Encoder event composition

`poll_event` calls the backend once into a local event, validates it against the ledger,
then transfers it to a reset caller destination. It refuses to overwrite an unhandled
event. Pending leaves destination unchanged; poll errors, exceptions and invalid events
stop admission without completing jobs. Invalid returned output is discarded; retained
job accounting requires explicit recovery. A valid fault is transferred for `handle_event`,
not interpreted as completion. Calls must obey backend nonblocking/no-loss semantics.
The caller retains the event, resolves its original per-job context, handles it, then
resets the destination before another poll. No unbounded polling loop or queue is added.

`handle_event` now serializes ledger preflight -> optional dispatch -> ledger completion.
Invalid/stale/duplicate events fault admission without any sink call. Non-output events
go directly to the ledger after validation. For `output_ready`, delivery outcome is returned
separately through `_delivery`; the function return reports event/ledger handling status.
No-consumer, denied, stale and failed-write output is terminally discarded for live preview,
then result handling is completed. This does not complete input or promise network delivery.
Never replay a successfully handled event because `_delivery` reports a failure/pending.
No internal output owner or queue is retained; caller must drop event owners promptly.

Runtime supplies the original per-job dispatch context and serializes the entire call
against policy, binding and ledger changes. The helper cannot reconstruct missing scopes
or detect a caller deliberately retagging a job. Sink calls must obey their no-reentrancy
contract. Exceptions are not interpreted as completion: caller retains the event and
ledger for fault recovery, with no blind retry after a possibly completed write.
`handle_event` now begins ledger drain before propagating a dispatch exception. It does
not complete the result or reset the event owner. Recovery may explicitly discard the
known output and apply its completion without retrying the sink. A fake sink test throws
after counting a write and checks retained accounting, stopped admission and no second
write during explicit reconciliation; this binary passes in the QCS6490 board smoke suite.
The one-event polling helper exists; concrete encoder polling implementation, retained
per-job context storage and the process event loop remain pending.

## Tests

Contract test source covers successful delivery, policy denial, sink write error and
no consumers through `handle_event`. Each outcome completes only the result half; input
accounting stays reserved until its separate event. Tests also check duplicate rejection
and no repeated write on input completion. These synthetic cases are included in the
executed QCS6490 unit/contract smoke suite; they do not exercise a live FW ring.

## Limits and next work

- Concrete encoder polling implementation, retained per-job context storage and the process
  event loop remain pending.
- A production renderer must bind authorization claims to the pixels before seal/encode;
  this integration remains missing.
- Synthetic contract cases do not exercise a live FW ring.

## See also

- [Revision-aware output authorization gate](output_gate.md)
- [Owned encoded output and synchronous sink port](encoded_output.md)
- [Encoder backend lifecycle port](encoder_backend.md)
- [FW ring sink](fw_ring_sink.md)
