# Output binding generation issuance

`output_generation` is a serialized, non-copyable/non-movable runtime-owned allocator that
issues output binding generation IDs. This document defines its monotonic issuance rules,
the optional watermark constructor and the integration order for replacement sinks.

**Status:** logic-tested — unit source covers monotonic issuance, burned IDs, type
restrictions and exhaustion with an unchanged destination; it is compiled and unit-tested.
Lead/lifecycle owner review and reconnect integration tests remain required.
**Layer:** core. **Source:** `src/core/output/vqec_vision_output_generation.cpp`,
`tests/unit/core/vqec_vision_output_generation_test.cpp`,
`tests/contract/outputs/vqec_vision_encoded_dispatch_test.cpp`.

## Responsibility

- Issues strictly increasing nonzero `uint64` binding generation IDs.
- Serves all output bindings from one instance that survives source/profile/ring rebuilds.
- Never rolls back a spent ID, including when subsequent binding setup fails.
- Must not derive identity from a mutable global, random ID, SDK counter, camera epoch or
  timestamp.
- Must not wrap, reset or automatically recover on exhaustion.
- Must not provide a cross-process uniqueness guarantee.

## Issuance contract

One instance must serve all output bindings and survive source/profile/ring rebuilds.
No mutable global, random ID, SDK counter, camera epoch or timestamp provides identity.
Exhaustion returns `resource_exhausted` without changing the caller's output; there is no
wrap, reset or automatic recovery.

The optional constructor watermark is the highest previously issued ID, not the next
ID. Default zero is for a fresh process with no surviving queued output. A nonzero
watermark requires trusted lifecycle ownership of the previous issuance domain; it
does not validate persistence or permit two allocators to share a domain. `UINT64_MAX`
constructs an exhausted allocator. There is no cross-process uniqueness guarantee.
Persisted/replayed outputs are unsupported and must not reuse process-local IDs.

## Integration order

Obtain a fresh ID before constructing a replacement sink and tag its encoder/output context
with that ID. SDK `mapping_generation` is obtained separately from the actual open ring;
`ring_sink` `dispatch_generation` uses the allocated ID. Do not query a new sink to retag
old output. Failed setup burns its ID. Destruction of a binding never resets the allocator.
All allocation and binding publication serialize on the runtime control owner; this
primitive is deliberately not an atomic singleton.

## Evidence and remaining work

This source does not open rings, own hardware, publish bindings, authorize output or
prove runtime wiring. The encoded dispatch contract test also composes the issuer with a
replacement fake sink: old binding rejected, independently constructed new-frame output
accepted, old binding still rejected afterward. This is test-source integration, not
runtime wiring or a live FW reopen test.

## Limits and next work

- Lead/lifecycle owner review and reconnect integration tests remain required.
- No cross-process uniqueness guarantee; persisted/replayed outputs are unsupported.
- The contract test is test-source integration, not runtime wiring or a live FW reopen
  test.

## See also

- [Synchronous authorized encoded dispatch](encoded_dispatch.md)
- [Owned encoded output and synchronous sink port](encoded_output.md)
- [Encoder admission and completion ledger](encoder_window.md)
