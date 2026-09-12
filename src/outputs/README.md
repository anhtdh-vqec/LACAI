# outputs

Portable output path: overlay metadata preparation, encoded-AU routing and feature-event
delivery. AI owns preview overlay/encode/ring production through private adapters; FW owns
RTSP/UI, recording and persistent evidence/search.

- **Status:** source-delivered helpers — end-to-end output pipeline not implemented
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
| `vqec_vision_encoded_dispatch.cpp` | Synchronous one-AU dispatch through `output_gate` with freshness/correlation and ring generation/demand checks |
| `vqec_vision_overlay_preparation.cpp` | Authorizes scope, validates observations, publishes overlay metadata transactionally |
| `vqec_vision_feature_event_dispatch.cpp` | Validates one event and dispatches it synchronously; retry keeps the original event/revision |

## Limits and next work

- No internal queue, retry or FW transport; delivery status is separate from event completion.
- Trusted renderer must supply complete scopes bound to pixels; that integration is missing.
- The optional FW ring sink is not runtime-wired into a service event loop.
- No real output delivery is available yet.

## See also

- [Encoded dispatch](../../docs/architecture/encoded_dispatch.md), [encoded output](../../docs/architecture/encoded_output.md)
- [Feature event dispatch](../../docs/architecture/feature_event_dispatch.md)
- [FW release compatibility](../../docs/contracts/fw_release_compatibility.md)
