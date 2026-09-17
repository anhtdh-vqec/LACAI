# AI-owned CPU preview surface

`writable_preview_surface` isolates writable preview pixels from shared FW input memory.
This document defines its allocation, sealing and borrow-lifetime rules. Reusable CPU
storage is provided separately by the preview pool.

**Status:** source-delivered — source-only baseline; not a DMA allocator, renderer,
encoder, reusable pool or board optimization. **Layer:** core. **Source:**
`src/core/vqec_vision_preview_surface.cpp`,
`tests/unit/vqec_vision_preview_surface_test.cpp`.

## Responsibility

- Allocates zero-initialized tightly packed NV12 storage within an explicit per-surface
  byte budget (maximum 64 MiB).
- Transfers sealed storage to a shared immutable owner.
- Exposes raw writable access only as a synchronous borrow.
- Must not be mixed into a pooled path to bypass admission.
- Must not export a mutable storage owner.
- Must not release a hardware input owner on result completion, timeout, disconnect or stop.

## Construction and layout

Reusable CPU storage is now provided separately by [preview pool](preview_pool.md).
The standalone create path still allocates independently; production callers
must not mix it into a pooled path to bypass admission. No hardware allocator is present.

`writable_preview_surface` is move-only. `create` allocates zero-initialized tightly
packed NV12 storage within an explicit per-surface byte budget (maximum 64 MiB).
Even dimensions <=8192 are accepted structurally, not advertised as hardware capacity.
Stride equals width; Y offset is zero; UV offset is `width*height`. There is no imported FD.
The image must be filled before encoding; zero initialization is not a valid black-frame
color policy. A future renderer/copy adapter must honor source stride/cache ownership.

`create` fails if the object is already populated, without replacing it. `seal` transfers
the allocation to a shared immutable vector owner and leaves the writer empty. No
mutable storage owner is exported. Raw writable data access is a synchronous borrow:
all such borrows MUST end before seal/move/destruction. C++ cannot revoke a saved pointer.
Calls on a writer are serialized by its owner, not internally synchronized.

## Encoder ownership

The encoder must retain the sealed owner while reading input, including after an
encoded result arrives. Result completion, timeout, disconnect and stop do not release
that input owner. This primitive has no hardware completion detector and cannot make
an unsafe encoder callback safe. Geometry/frame identity travel separately and must
be validated and bound by the later submission contract.

## Memory lifetime

Memory is freed after the last sealed owner is released, including if the original
writer no longer exists. This does not touch/ACK any Camera lease. The caller enforces
aggregate allocation admission; repeated create calls on different objects are NOT
globally bounded. A reusable pool and encoder job ledger are still required before
production integration. Do not allocate one unbounded surface per incoming frame.

Encoder admission now has source in [encoder window](encoder_window.md): reserve before
allocation, retain accounting until both completions. This is not a reusable pool and
does not replace the actual sealed owner held by a backend job.

## Limits and next work

- A reusable pool and encoder job ledger are still required before production integration.
- Repeated create calls on different objects are not globally bounded.
- The primitive has no hardware completion detector and cannot make an unsafe encoder
  callback safe.

## See also

- [Bounded reusable CPU NV12 preview pool](preview_pool.md)
- [Encoder admission and completion ledger](encoder_window.md)
- [Preview metadata boundary](preview_contract.md)
