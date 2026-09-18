# outputs

Portable output path: overlay metadata preparation, encoded-AU routing and feature-event
delivery. AI owns preview overlay/encode/ring production through private adapters; FW owns
RTSP/UI, recording and persistent evidence/search.

- **Status:** board-smoke — eSDK tests and the 2026-09-18 `.98` native candidate run pass
- **Layer:** outputs
- **Naming registry:** `outpt` (`encdp`, `ftdsp`, `ovrpr`)
- **Depends on:** `src/core/` output policy and neutral `encoded_sink`/event-sink contracts
- **Used by:** runtime executor and, later, a threaded service event loop

## Responsibility

- Authorize the complete requested scope before rendering or dispatch, using the exact scope list.
- Map validated observations into neutral overlay metadata without rendering pixels.
- Dispatch one encoded AU or one feature event synchronously through a neutral sink, deriving
  authorization from actual payload fields immediately before delivery.

## Contents

| Path | Purpose |
|---|---|
| `events/` | Feature-event authorization, synchronous dispatch and bounded delivery seam |
| `media/` | Encoded-AU authorization/dispatch and transactional overlay preparation |
| `CMakeLists.txt` | Declares the portable output target from both ownership subtrees |

## Limits and next work

- No internal queue, retry or FW transport; delivery status is separate from event completion.
- Full authorization-scope wiring through `prepared_overlay` remains open.
- The Qualcomm service path writes H.264 to the released FW ring. A QCS6490 board smoke
  showed correct NV12 color, visible person boxes and late-join RTSP decoding through the
  FW compatibility harness; released FW RTSP/UI acceptance remains open.

## See also

- [Encoded dispatch](../../docs/architecture/encoded_dispatch.md), [encoded output](../../docs/architecture/encoded_output.md)
- [Feature event dispatch](../../docs/architecture/feature_event_dispatch.md)
- [Event delivery seam](../../docs/architecture/event_delivery_seam.md)
- [FW release compatibility](../../docs/contracts/fw_release_compatibility.md)
- [Repository source layout](../../docs/development/source_layout.md)
