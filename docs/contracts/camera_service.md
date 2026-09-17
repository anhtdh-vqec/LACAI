# Unified FW RAW source contract — proposal v1

This document proposes a unified multi-source FW RAW source contract shared by AI Camera
and AI Box. It defines the control operations, media transport, frame descriptor,
ownership/synchronization and QoS semantics that LACAI would consume, and it records which
parts are still proposals rather than APIs that the current receiver may assume exist. It
does not describe an IPC API that currently exists.

**Status:** planned — unsigned four-team proposal with no released IPC API; the current AI
receiver instead consumes the existing FW third NV12/FD stream. **Layer:** contracts.
**Source:** `n/a`.

Owner: FW software + BSP; consumer/reviewer: AI APP.
The contract does not fix 4K/25 FPS. Each source must return an exact effective profile; RAW
means post-ISP uncompressed NV12 (not Bayer). 3840x2160@25 is only one workload to measure;
every resolution/FPS combination and concurrent source count must be declared as BSP/FW
capability and accepted separately.

## Responsibility

- Defines the proposed boundary between FW RAW-source production and AI APP consumption.
- Fixes descriptor, lease, synchronization, epoch and backpressure semantics for 1..16
  logical sources on both AI Camera and AI Box.
- Must not be treated as a released protocol: the operations and extended descriptors below
  are a future proposal, not APIs the current receiver may assume exist.

## Baseline integration update — 2026-09-06

AI implementation will consume the existing FW third NV12/FD stream, not wait for the
proposed 4K/versioned contract. See the standalone
[FW requirements and current integration baseline](fw_camera_integration_requirements.md)
for actual socket paths, profiles, legacy wire layout, control calls and safety gaps. The
operations and extended descriptors below remain a future proposal, not APIs that the
current receiver may assume exist. P0 safety sign-off remains a production gate.

## Unified multi-source RAW update — 2026-09-08

- AI Camera and AI Box must both provide 1..16 logical FW RAW sources with the same
  descriptor, lease/ownership, synchronization, epoch and backpressure semantics to AI APP.
- On AI Camera, FW takes RAW from sensor/ISP/Camera Service. On AI Box, FW owns the entire
  RTSP endpoint, credential, demux, decode, decoder surface and reconnect before RAW.
- AI APP only resolves `raw_source_ref`; it receives no URI/codec/credential, does not
  demux/decode RTSP and has no per-product-type processing branch.
- Capability must state max source, RAW resolution/FPS/format/memory ranges and concurrent
  workload combinations. Decoder/RTSP cost on AI Box is FW-budgeted but must be counted when
  FW returns overall capability for admission.
- Source profile change/reconnect increments epoch and forces bounded drain/rebind; AI APP
  does not continue temporal state across a discontinuity.

Configuration and memory admission details are in
[multi-source configuration](../architecture/multi_source_configuration.md).

AI APP resolves each configured identity through the bounded
[FW RAW-source resolver](../architecture/raw_source_resolution.md). A future FW registry
must return an attachment route without exposing upstream RTSP details. Until that RPC is
released, the compatibility helper maps the existing `third` Unix-socket naming convention
at activation time; no frame-path component derives product topology.

Compatibility note: the released Camera1 RPC carries both `camera_id` and `channel_id`,
but current FW validates `channel_id=0`; multi-sensor Camera uses distinct `camera_id`
values. AI acquisition now preserves both values in its request identity and sends the
configured channel instead of hard-coding it. A non-zero channel therefore still fails
closed against the current release until FW advertises support; this change does not
invent a new FW topology.

## Control operations

| Operation | Request -> Response |
|---|---|
| get_capabilities | version/client -> modes, formats, memory/sync, limits |
| acquire_source | request_id, app_instance, source_id, requirements -> lease_id, epoch, effective_profile, expiry |
| renew_lease | request_id, lease_id -> expiry/status |
| get_source_status | lease/source -> state, profile, epoch, counters |
| release_source | request_id, lease_id -> released or draining |
| attach_consumer | authenticated lease + protocol -> transport endpoint/session |
| quiesce | source/lease, deadline -> ack request; quiesce_complete when readers end |
| release_frame | session, lease, epoch, frame token, completion -> ACK status |

Event: source_state_changed, source_profile_changed, source_discontinuity,
lease_revoked, frame_available, quiesce_complete.
Logical operation names are snake_case; the DBus/socket binding is fixed by FW and APP.
C++ functions implementing the binding still follow the prefix convention and do not change
the wire name.

`request_id` is idempotent for a mutation; retrying the same id returns the same result and
does not create a new lease. An incompatible major version must be rejected; a minor version
is negotiated. Errors: unauthorized, unsupported_profile, source_busy, invalid_lease,
stale_epoch, protocol_mismatch, resource_exhausted, timeout, source_lost.

## Media transport

Proposed Unix domain socket + SCM_RIGHTS for FD, with descriptor messages carrying framing
and size limits. DBus is control-only if FW chooses it. The transport may be replaced by an
ADR while keeping semantics. Do not send 4K pixels over the control bus, and do not
serialize a numeric FD and expect it to be usable in another process.

Binding transport to authenticated peer credentials + an authorized lease is mandatory;
`consumer_id` sent by the client is not evidence of access rights. Determine whether the
descriptor is per-frame or pool-registration + token; if a pool is registered, the pool
`allocation_id`/`generation` is durable within the session and invalidated after restart.

## Minimum frame descriptor

magic, protocol_major/minor, descriptor_size, message_type;
producer_instance_id, source_id, source_epoch, profile_revision;
frame_sequence, buffer_allocation_id, buffer_generation, frame_lease_token;
capture_timestamp_ns, clock_domain, UTC mapping revision/uncertainty;
width,height, pixel_fourcc, modifier, orientation;
plane_count and for each plane: handle_index, offset_bytes, stride_bytes,
row_count, valid_size_bytes, allocation_size_bytes;
color_matrix, color_range, chroma_siting;
acquire_sync_kind + sync handle/token if present; deadline/hold budget.

Validate `offset + span <= allocation_size` with checked arithmetic, bound plane/FD count,
match dimensions to the negotiated profile and do not confuse the timestamp with UTC. Do not
expose GstVideoFormat/GstBuffer in the neutral ABI. A raw C++ struct is not a wire
serializer: pin endianness, widths, layout and lengths.

## Ownership and synchronization

The producer grants a read-only frame. AI does not overlay/write into the shared input. AI
holds the lease until the last reader completes; closing an FD does not replace ACK, and ACK
does not rely on timeout. The acquire fence must be waited/imported before a device read.
CPU cache START/END and hardware completion are two different mechanisms. A C2D gpointer
token is not sent as a Linux sync_file FD. `release_frame` uses the completion token of the
negotiated mechanism; reject a stale token/epoch, make duplicate ACK idempotent and never
double-return a pool slot.

The camera does not recycle a buffer held by AI/device just because a consumer timed out.
When a consumer dies: FW+BSP must have a quarantine/quiesce/reset guarantee that proves DMA
has stopped; if it does not, do not enable an unsafe reclaim policy. One APP source
acquisition manager aggregates every feature requirement.

## QoS and variation

Fix max_inflight, max_hold_ms, queue_depth, drop policy, min/max FPS and profile priority
relative to recording/live streaming. Do not hold buffers indefinitely waiting for
inference. On resolution/stride/format change: notify, define an epoch/profile revision
boundary, drain the old pool before replacement; do not silently modify a descriptor with
the old memory layout. On source loss/restart: invalidate epoch, reset temporal state;
reconnect uses backoff.

## Common acceptance

4K pattern with stride padding/color bars; slow consumer; queue full; duplicate/lost ACK;
disconnect during a job; producer restart; stale FD/pool generation; profile change while a
frame is in flight; malformed descriptor; unauthorized peer; Camera record/live continue to
meet KPI while APP is overloaded. Record the trace capture -> receive -> last hardware read
done -> release ACK.

## Limits and next work

- Missing BSP sign-off: memory/sync/reset guarantees and formats.
- Missing FW software sign-off: wire schema, auth, idempotency, supervision + source
  lifecycle.
- Missing AI APP sign-off: hold budgets, accepted modes, reconnect/overload behavior.
- Numeric QoS parameters and clock accuracy must be filled in after measurement, before the
  week-4 gate.

## See also

- [FW requirements and current integration baseline](fw_camera_integration_requirements.md)
- [Multi-source configuration](../architecture/multi_source_configuration.md)
- [FW RAW-source resolver](../architecture/raw_source_resolution.md)
