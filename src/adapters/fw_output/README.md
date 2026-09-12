# fw_output

Private adapter for the released FW shared-memory encoded AI ring SDK. Implements the
neutral `encoded_sink` contract over an already-open borrowed ring buffer.

- **Status:** source-delivered, optional — not built or tested; closed-ring guard test source only
- **Naming registry:** `fwout` (`rgsnk`)
- **Build option:** `VQEC_VISION_AI_ENABLE_FW_RING` with an externally supplied pinned SDK target
- **Used by:** encoded dispatch/sink wiring (not yet runtime-wired)

## Responsibility

- Translate bounded neutral metadata into the FW ring header transactionally (no ring creation).
- Query consumer demand and write one H264 AU synchronously to the borrowed ring.
- Reject a stale mapping generation and never store borrowed AU pointers.
- Never report queued work as copied-to-ring success.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_ring_sink.cpp` | `encoded_sink` over an already-open borrowed FW `SharedMemoryFrameRingBuffer` |

## Limits and next work

- Requires distinct SDK `mapping_generation` and runtime-unique `dispatch_generation`; the SDK
  counter `1` is not an identity across replacement instances.
- No open/create/unlink or SDK copy; no FW shared-memory layout is copied into portable code.
- Portable outputs/contracts must not expose pthread, `std::atomic` shared layouts or FW types.
- SDK packaging and ABI qualification require FW agreement before production integration.

## See also

- [FW ring sink](../../../docs/architecture/fw_ring_sink.md), [encoded output](../../../docs/architecture/encoded_output.md)
- [FW release compatibility](../../../docs/contracts/fw_release_compatibility.md) (FW04–FW06)
