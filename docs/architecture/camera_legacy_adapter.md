# Legacy camera adapter implementation boundary

This document defines the FW legacy camera transport boundary: the fixed 104-byte wire
layout, the nonblocking FD receiver and the shared `received_frame` ownership that carries the
ACK back to the originating session. It records what the adapter guarantees and what remains an
FW/BSP responsibility.

**Status:** source-delivered — strict 104-byte decoder, SOCK_SEQPACKET/SCM_RIGHTS receiver and
session-owned ACK exist; live transport validation and sync/thermal sign-off remain pending.
**Layer:** adapters. **Source:** `src/adapters/camera/vqec_vision_legacy_wire.{hpp,cpp}`,
`vqec_vision_source_lifecycle.{hpp,cpp}`, `vqec_vision_raw_source_resolver.{hpp,cpp}`.

## Responsibility

- Decode fixed native-endian FW header fields by byte offset without casting untrusted bytes.
- Receive third-stream dmabuf frames over a resolved socket path and own the ACK release queue.
- Provide shared immutable frame ownership to every reader and derived memory view.
- Do not derive Camera/Box topology from a logical camera ID, map pixels or negotiate caps.

## Wire layout and decoding

- Fixed header byte offsets now live in `legacy_wire_layout` in the private decoder header;
  receiver and decoder share `g_header_bytes`. Layout is still exactly 104 native-endian bytes;
  no packed-struct reinterpretation or new negotiation is introduced.
- Independent wire fixtures retain numeric offsets as compatibility evidence rather than copying
  the decoder's constants. NV12 chroma rows remain `height/2` by the pixel format definition.
- The legacy decoder copies fixed native-endian fields by byte offset, never casts untrusted
  bytes to a packed C++ struct. Host and FW must share endian and the pinned `GstVideoFormat`
  ABI. The caller supplies `GST_VIDEO_FORMAT_NV12` from its installed GStreamer integration as an
  explicit numeric compatibility setting; core never depends on GStreamer. This is not a format
  negotiation mechanism.
- Metadata supports variable even dimensions, padded strides and nonzero memory offset, bounded
  by configurable maximum dimensions/allocation bytes. No 1080p or 1440p capability table is
  hardcoded into the parser; FW controls offered profiles. Changing resolution/FPS with unchanged
  NV12 wire does not require a new parser. It still requires graph renegotiation/drain in the
  future runtime, not silent reuse of old tensor transforms. Colorimetry and sync are unknown,
  not invented defaults.

Baseline: [FW wire and requirements](../contracts/fw_camera_integration_requirements.md). The
media receiver is composed with the [Camera control client](camera_control_client.md) by
`source_lifecycle` and `camera_session`; `raw_source_resolver` supplies the activation-time
endpoint. The caller must acquire `StartStream(third, dmabuf)` before connecting and
`StopStream` only after every frame owner and transport session has drained.

## Receiver and transport

- Linux receiver uses nonblocking `SOCK_SEQPACKET`, `CLOEXEC`, `SCM_RIGHTS` and `SO_PEERCRED`.
- Expected producer UID is mandatory; caller obtains it from deployment policy.
- The receiver consumes an exact resolved socket path and never derives Camera/Box topology or
  a socket name from the logical camera ID on the frame path.
- Connect is one nonblocking attempt; caller schedules retries, no internal sleep.
- Receive has a bounded timeout, exact payload/ancillary validation and a maximum of four live
  leases across all sessions belonging to that receiver (including detached sessions). It does
  not map, copy or import image pixels.

## Frame ownership and ACK

`received_frame` is shared immutable ownership: retain the shared owner across every
synchronous/asynchronous reader and every derived memory view. Its last destruction hands the
ACK token to the originating session's release queue and closes the FD; the queue performs the
nonblocking send, retries on `EAGAIN` and only faults the session past a bounded deadline or
bounded queue depth. Destruction therefore never blocks and never faults on a single full send
buffer. This is safe only when owner lifetime tracks actual completion.

- Never retain only the borrowed integer FD after releasing the frame object.
- No destructor waits for hardware, and no timer fabricates completion.
- Future GstMemory bridge must retain this owner until all reading memory views are released
  after device completion.

## Epochs, faults and disconnect

- Disconnect drops the receiver's session reference; outstanding frames retain the old socket
  and its release queue. It is not a cancellation primitive.
- New sessions receive monotonically increasing receiver-local epochs; callers add their
  runtime/source identity. Wire `buf_id` must increase within a session (as in the current
  producer).
- Malformed packet marks the session faulted without closing the socket underneath outstanding
  readers; no further receive is allowed. Each delivered frame still queues its ACK on that same
  session. Last session owner closes the socket.
- A hard transport error or an exceeded release deadline marks transport fault; the receiver
  never reconnects or resends onto a new session.

## Limits and next work

- AI cannot prevent FW independently recycling after its own stop/disconnect. FW P0 safety
  sign-off remains mandatory; shared ownership alone does not solve it.
- No graph activation, hardware compatibility or production safety is claimed here.
- Live transport validation, sync/thermal sign-off and automatic source restart remain open.

## See also

- [Camera control client: FW legacy D-Bus binding](camera_control_client.md)
- [Combined Camera control/media lifecycle](camera_source_lifecycle.md)
- [FW wire and requirements](../contracts/fw_camera_integration_requirements.md)
- [Legacy camera FD to private GStreamer memory bridge](dmabuf_memory_bridge.md)
