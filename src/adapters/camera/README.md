# camera

Private FW RAW-source adapter: legacy third-stream media receiver, lease control and
activation-time RAW-reference resolution. Implements the neutral `raw_source_port` without
leaking Camera Service wire or product-origin types into application code.

- **Status:** source-delivered — not board-qualified; no live FW inference run yet
- **Naming registry:** `camer` (`lwire`, `frsrc`, `cctrl`, `cmrpc`, `dbrpc`, `srclc`, `rsrsv`, `cmpro`)
- **Depends on:** neutral `raw_source_port` and core source-binding validation
- **Used by:** `src/app/camera_graph_pump`, `camera_session`, `multi_source_supervisor`

## Responsibility

- Decode the released 104-byte native-endian frame header with strict NV12 view validation.
- Receive frames over SOCK_SEQPACKET/SCM_RIGHTS with a peer UID check and session-owned ACK.
- Drive Start/Stop lease reconciliation, combined acquisition lifecycle and source release.
- Map an opaque `raw_source_ref` to the exact control identity/socket/producer/ABI route
  without a Camera/Box branch.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_legacy_wire.cpp` | Portable 104-byte decoder with NV12 view validation |
| `vqec_vision_frame_source.cpp` | Optional Linux receiver with peer UID check |
| `vqec_vision_received_frame` (in frame source) | Shared completion ownership; final owner ACKs on the original session |
| `vqec_vision_camera_control.cpp`, `vqec_vision_camera_rpc.hpp` | Start/Stop lease state machine with ambiguous-outcome reconciliation |
| `vqec_vision_dbus_rpc.cpp` | Optional private GIO binding for the current FW `a{sv}` contract |
| `vqec_vision_source_lifecycle.cpp` | Acquire/connect/receive/drain/release for one cycle |
| `vqec_vision_raw_source_resolver.cpp` | Bounded activation-time RAW-reference resolution |

## Limits and next work

- Caller must retain every frame owner until hardware completion, acquire the lease before
  connect and release it only after all readers/sessions drain.
- A stopped lifecycle is terminal; the supervisor supplies fresh request IDs.
- Authenticated registry RPC and live transport validation remain open.
- One lifecycle is shared by compatible consumers; do not recreate it per model/feature.

## See also

- [Camera source lifecycle](../../../docs/architecture/camera_source_lifecycle.md), [control client](../../../docs/architecture/camera_control_client.md)
- [Legacy adapter boundary](../../../docs/architecture/camera_legacy_adapter.md), [RAW-source resolution](../../../docs/architecture/raw_source_resolution.md)
- [RAW-source port](../../../docs/architecture/raw_source_port.md), [FW camera baseline](../../../docs/contracts/fw_camera_integration_requirements.md)
