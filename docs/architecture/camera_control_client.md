# Camera control client: FW legacy D-Bus binding

This document defines the portable FW Camera Service lease client: how AI calls
StartStream/StopStream over the legacy D-Bus wire, how it maps FW result codes and how it
keeps an uncertain acquisition recoverable. It exists so control ownership, request identity
and reconciliation rules stay explicit before any live transport is trusted.

**Status:** source-delivered — `camera_control`, its private `camera_protocol` constants and the
optional GIO `dbus_rpc` are in the tree; live transport validation remains pending.
**Layer:** adapters. **Source:** `src/adapters/camera/vqec_vision_camera_control.{hpp,cpp}`,
`vqec_vision_camera_protocol.hpp`, `vqec_vision_camera_rpc.hpp`.

## Responsibility

- Provide one acquisition/release cycle per controller through an injected `camera_rpc` port.
- Keep the portable state machine free of sibling FW headers or libraries.
- Save handles and pending state conservatively; never force-forget an unresolved acquisition.
- Do not own the media receiver, prove hardware quiescence or perform destructor StopStream.

## FW wire and result mapping

- FW result mapping recognizes ResourceExhausted (2004) as `resource_exhausted` and
  VersionMismatch (9001) as `protocol_error` rather than generic `io_error`. Source is FW
  `common/result.hpp`. Neither error authorizes forgetting a pending acquisition or creating a
  replacement request ID; reconciliation ownership remains unchanged.
- Endpoint and method spelling now live in the private `camera_protocol` constants header; AI
  request/reply size and timeout ceilings are explicitly labeled parser safety policy, not
  FW-advertised capabilities. D-Bus GVariant format strings remain literal standard marshalling
  syntax. No method, endpoint or wire value changed in this extraction.
- Source evidence: FW `rpc.cpp` `fields_to_variant`/`call_once_sync`/`make_ok_response`,
  `types.cpp` `result_from_fields`/`stream_info_to_fields`, `Camera1.xml` and
  `camera_service.cpp` `schema_for_method`/`handle_start_stream`/`handle_stop_stream` at baseline
  commit 139d335.
- FW sends a tuple containing `a{sv}`; ALL dictionary values are string variants, including
  `camera_id`, `channel_id`, `fps` and `code`. Success is numeric string `code="0"`, not a
  boolean `ok` field. The actual reply field is `camera_reconcile_revision`, not a source epoch;
  the client must not use it as frame generation.
- Reply fields are bounded; numeric parsing requires full decimal consumption and rejects
  overflow. Geometry/FPS are reported, not changed; quality updates belong to coordinated
  runtime renegotiation, not a hidden SetStreamResolutionAndFps call.

## State machine and identity

State: `idle -> start_pending -> acquired -> stop_pending -> idle`.

- `start_pending` means FW may have acquired a lease, even if the response was lost.
- Retry Start uses the same `request_id` and camera/channel/consumer identity. Do not create
  another client/consumer identity to hide an unresolved acquisition.
- Errors conservatively retain pending state; there is no unsafe force-forget API.
- A valid handle is saved BEFORE validating the effective profile; malformed profile does not
  lose the ability to release a successfully acquired stream.
- Stop uses the same handle/consumer and a separate stable `request_id`; success or FW code
  1002 (NotFound) completes release. Other errors retain handle/pending state.
- If stopping was requested, Start cannot reacquire until stop is reconciled.
- Input `camera_id` and `consumer_id` are immutable for each acquisition. Consumer and request
  IDs must be bounded ASCII identifiers with no whitespace/control bytes.
- The caller must supply fresh request IDs for a new acquisition/release cycle; stable IDs are
  reused only while retrying the same logical operation.

The portable `camera_control` state machine uses an injected `camera_rpc` port. The optional
private GIO implementation is `dbus_rpc`. Neither includes sibling FW headers or libraries.
Default bus is system; session bus is explicit for testing. The endpoint/method names are fixed
to `Camera1`, not arbitrary caller-selected RPCs. No automatic service activation, method retry,
sleep, or destructor StopStream.

Caller must drain all camera readers and detach the receiver BEFORE calling Stop. The controller
does not own the receiver and cannot prove hardware quiescence. Destruction performs no remote
mutation and no blocking cleanup; the supervisor must reconcile pending state before destruction.
Allocation exceptions may escape the C++ API; stored pending state is intentionally kept for
caller recovery.

## GIO D-Bus client

`dbus_rpc.open` establishes a private bus connection outside the per-method deadline. Address
lookup/connection setup have no caller-specified connection timeout: do not run `open` on the
frame thread. Exit-on-close is disabled on this private connection; teardown requests asynchronous
bus close, never a remote StopStream. Each method call uses an explicit bounded timeout. Method
transport errors are ambiguous outcomes, not proof of no mutation. Wire replies remain separate
from transport status. GIO allocations happen before bounded reply parsing; the parser limit is
not a cap on the bus daemon/GIO's receive allocation.

## Limits and next work

- Not yet implemented: NameOwnerChanged-driven reconciliation, persistence across AI crash,
  subscription to profile changes, GstMemory bridge, active inference.
- The [combined source lifecycle](camera_source_lifecycle.md) now coordinates local acquisition,
  reception and drain/release. The service supervisor must coordinate restart; this slice does
  not claim automatic recovery across arbitrary Camera Service restarts.

## See also

- [Combined Camera control/media lifecycle](camera_source_lifecycle.md)
- [Legacy camera adapter implementation boundary](camera_legacy_adapter.md)
- [Single-camera session coordinator](camera_session.md)
