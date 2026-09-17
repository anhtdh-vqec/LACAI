# Bounded reusable CPU NV12 preview pool

This document defines the bounded reusable CPU NV12 preview pool: one-shot configuration,
move-only writable leases, sealed immutable readers and the reuse-safety rules. It has no FD
import, hardware rendering/encoding or ring integration.

**Status:** source-delivered — source-only; no FD import, hardware rendering/encoding or
ring integration. **Layer:** core. **Source:**
`src/core/vqec_vision_preview_pool.cpp`,
`tests/unit/vqec_vision_preview_pool_test.cpp`.

## Responsibility

- Preallocates 1–4 equal packed NV12 surfaces within the caller's explicit budget.
- Lends a slot through a move-only writable preview surface.
- Passes the same lease to immutable shared readers on seal.
- Marks a slot free only after the last strong reader/writer owner releases it.
- Must not reconfigure, forcibly reclaim or time out a lease.
- Must not allocate hidden fallback storage when exhausted.

## Pool and lease contract

One-shot configure preallocates 1–4 equal packed NV12 surfaces with <=64 MiB per
surface and <=256 MiB aggregate pixel storage, within the caller's explicit budget.
Bounds do not include allocator/control-block overhead and are not board capacity claims.

Acquire lends a slot through `writable_preview_surface` (move-only). Seal passes the
same lease to immutable shared readers. A lease control block marks the slot free
only after the LAST strong reader/writer owner releases it. Each acquisition uses a
new lease control block, so weak pointers from an old lease cannot regain access after
slot reuse. Do not infer reusability by sampling the pixel allocation `use_count`.

The release callback owns the backing slot, not the pool facade. Destroying the facade
does not invalidate active readers; the last reader then releases storage safely.
Callbacks may finish on backend threads; the busy flag uses release/acquire ordering.
Configure/acquire/inspection are serialized by the runtime. No pool reconfiguration,
forced reclaim, timeout recycle or hidden fallback allocation when exhausted.

## Exhaustion and reuse safety

Acquire allocates only a shared lease control block, not new pixel storage. Failure
leaves the output writer unchanged and the slot available. Pool exhaustion returns
`resource_exhausted`; caller drops unsubmitted work or retries, never waits in this API.
Pixels are NOT cleared on reuse: the renderer/copy stage must overwrite the complete
image before publishing to avoid stale content disclosure. Zero initial allocation
is not a color-correct black frame. Mutable borrows must end before seal/release.

Runtime must budget all simultaneously alive pool generations, including retired
facades with outstanding readers. Do not recreate pools repeatedly to bypass pressure.
This primitive is not a global memory supervisor or proof that hardware has quiesced.

## Encoder integration

Reserve `encoder_window` before acquire; cancel the unsubmitted reservation if
acquire/render fails. Commit before encoder push; retain sealed owner through hardware
input completion. H264 result/timeout alone never releases input. The pool cannot validate
a dishonest/mistimed completion event from a backend.

## Limits and next work

- No FD import, hardware rendering/encoding or ring integration is present.
- The pool cannot validate a dishonest/mistimed completion event from a backend.
- This primitive is not a global memory supervisor or proof that hardware has quiesced.
- Bounds exclude allocator/control-block overhead and are not board capacity claims.

## See also

- [AI-owned CPU preview surface](preview_surface.md)
- [Preview metadata boundary](preview_contract.md)
- [Encoder admission and completion ledger](encoder_window.md)
