# Bounded reusable CPU NV12 preview pool

Source-only. No FD import, hardware rendering/encoding or ring integration.
One-shot configure preallocates 1–4 equal packed NV12 surfaces with <=64 MiB per
surface and <=256 MiB aggregate pixel storage, within the caller's explicit budget.
Bounds do not include allocator/control-block overhead and are not board capacity claims.

Acquire lends a slot through writable_preview_surface (move-only). seal passes the
same lease to immutable shared readers. A lease control block marks the slot free
only after the LAST strong reader/writer owner releases it. Each acquisition uses a
new lease control block, so weak pointers from an old lease cannot regain access after
slot reuse. Do not infer reusability by sampling the pixel allocation use_count.

The release callback owns the backing slot, not the pool facade. Destroying the facade
does not invalidate active readers; the last reader then releases storage safely.
Callbacks may finish on backend threads; the busy flag uses release/acquire ordering.
Configure/acquire/inspection are serialized by the runtime. No pool reconfiguration,
forced reclaim, timeout recycle or hidden fallback allocation when exhausted.

Acquire allocates only a shared lease control block, not new pixel storage. Failure
leaves the output writer unchanged and the slot available. Pool exhaustion returns
resource_exhausted; caller drops unsubmitted work or retries, never waits in this API.
Pixels are NOT cleared on reuse: the renderer/copy stage must overwrite the complete
image before publishing to avoid stale content disclosure. Zero initial allocation
is not a color-correct black frame. Mutable borrows must end before seal/release.

Encoder integration: reserve encoder_window before acquire; cancel the unsubmitted
reservation if acquire/render fails. Commit before encoder push; retain sealed owner
through hardware input completion. H264 result/timeout alone never releases input.
The pool cannot validate a dishonest/mistimed completion event from a backend.

Runtime must budget all simultaneously alive pool generations, including retired
facades with outstanding readers. Do not recreate pools repeatedly to bypass pressure.
This primitive is not a global memory supervisor or proof that hardware has quiesced.
