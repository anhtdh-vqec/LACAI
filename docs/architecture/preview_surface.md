# AI-owned CPU preview surface

Reusable CPU storage is now provided separately by [preview pool](preview_pool.md).
The standalone create path below still allocates independently; production callers
must not mix it into a pooled path to bypass admission. No hardware allocator is present.

Source-only baseline; not a DMA allocator, renderer, encoder, reusable pool or board
optimization. This isolates writable preview pixels from shared FW input memory.

`writable_preview_surface` is move-only. create allocates zero-initialized tightly
packed NV12 storage within an explicit per-surface byte budget (maximum 64 MiB).
Even dimensions <=8192 are accepted structurally, not advertised as hardware capacity.
Stride equals width; Y offset is zero; UV offset is width*height. There is no imported FD.
The image must be filled before encoding; zero initialization is not a valid black-frame
color policy. A future renderer/copy adapter must honor source stride/cache ownership.

create fails if the object is already populated, without replacing it. seal transfers
the allocation to a shared immutable vector owner and leaves the writer empty. No
mutable storage owner is exported. Raw writable data access is a synchronous borrow:
all such borrows MUST end before seal/move/destruction. C++ cannot revoke a saved pointer.
Calls on a writer are serialized by its owner, not internally synchronized.

The encoder must retain the sealed owner while reading input, including after an
encoded result arrives. Result completion, timeout, disconnect and stop do not release
that input owner. This primitive has no hardware completion detector and cannot make
an unsafe encoder callback safe. Geometry/frame identity travel separately and must
be validated and bound by the later submission contract.

Memory is freed after the last sealed owner is released, including if the original
writer no longer exists. This does not touch/ACK any Camera lease. The caller enforces
aggregate allocation admission; repeated create calls on different objects are NOT
globally bounded. A reusable pool and encoder job ledger are still required before
production integration. Do not allocate one unbounded surface per incoming frame.

Encoder admission now has source in [encoder window](encoder_window.md): reserve before
allocation, retain accounting until both completions. This is not a reusable pool and
does not replace the actual sealed owner held by a backend job.
