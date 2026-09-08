# Legacy camera adapter implementation boundary

Fixed header byte offsets now live in legacy_wire_layout in the private decoder header;
receiver and decoder share g_header_bytes. Layout is still exactly 104 native-endian
bytes; no packed-struct reinterpretation or new negotiation is introduced. Independent
wire fixtures retain numeric offsets as compatibility evidence rather than copying the
decoder's constants. NV12 chroma rows remain height/2 by the pixel format definition.

Baseline: [FW wire and requirements](../contracts/fw_camera_integration_requirements.md).
The media receiver is composed with the
[Camera control client](camera_control_client.md) by `source_lifecycle` and
`camera_session`; `raw_source_resolver` supplies the activation-time endpoint.
The caller must acquire StartStream(third, dmabuf) before connecting and StopStream
only after every frame owner and transport session has drained.

The legacy decoder copies fixed native-endian fields by byte offset, never casts
untrusted bytes to a packed C++ struct. Host and FW must share endian and the pinned
GstVideoFormat ABI. The caller supplies GST_VIDEO_FORMAT_NV12 from its installed
GStreamer integration as an explicit numeric compatibility setting; core never
depends on GStreamer. This is not a format negotiation mechanism.

Metadata supports variable even dimensions, padded strides and nonzero memory
offset, bounded by configurable maximum dimensions/allocation bytes. No 1080p or
1440p capability table is hardcoded into the parser; FW controls offered profiles.
Changing resolution/FPS with unchanged NV12 wire does not require a new parser.
It still requires graph renegotiation/drain in the future runtime, not silent reuse
of old tensor transforms. Colorimetry and sync are unknown, not invented defaults.

Linux receiver uses nonblocking SOCK_SEQPACKET, CLOEXEC, SCM_RIGHTS and SO_PEERCRED.
Expected producer UID is mandatory; caller obtains it from deployment policy.
The receiver consumes an exact resolved socket path and never derives Camera/Box topology
or a socket name from the logical camera ID on the frame path.
Connect is one nonblocking attempt; caller schedules retries, no internal sleep.
Receive has a bounded timeout, exact payload/ancillary validation and a maximum
of four live leases across all sessions belonging to that receiver (including detached
sessions). It does not map, copy or import image pixels.

received_frame is shared immutable ownership: retain the shared owner across
every synchronous/asynchronous reader and every derived memory view. Its last
destruction attempts a nonblocking ACK on the original session and closes the FD.
This is safe only when owner lifetime tracks actual completion. Never retain only
the borrowed integer FD after releasing the frame object. No destructor waits for
hardware, and no timer fabricates completion. Future GstMemory bridge must retain
this owner until all reading memory views are released after device completion.

Disconnect drops the receiver's session reference; outstanding frames retain the
old socket. It is not a cancellation primitive. New sessions receive monotonically
increasing receiver-local epochs; callers add their runtime/source identity.
Wire buf_id must increase within a session (as in the current producer).
Malformed packet marks the session faulted without closing the socket underneath
outstanding readers; no further receive is allowed. Each delivered frame still
attempts its ACK on that same session. Last session owner closes the socket.
Failed ACK marks transport fault, does not reconnect or resend onto a new session.

AI cannot prevent FW independently recycling after its own stop/disconnect.
FW P0 safety sign-off remains mandatory; shared ownership alone does not solve it.
No graph activation, hardware compatibility or production safety is claimed here.
