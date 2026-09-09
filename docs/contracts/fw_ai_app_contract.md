# FW–AI APP integration contract

Version: 1.0-draft, 2026-09-09
Owner: AI APP lead
Target: LACAI replacing `application/ai_app` in `vqec_camera_service`

This document is the request to the FW team. LACAI is the reference AI APP architecture.
FW must preserve the existing external behavior below or provide a versioned migration
contract reviewed by both owners. Internal FW implementation may change; the boundary,
ownership and observable behavior may not change silently.

## Required FW boundary

FW owns camera capture, sensor/ISP, GStreamer camera branches, RTSP demux/decode on AI
Box, main/sub streams, RTSP/UI, recording and persistent evidence. LACAI owns inference,
tracking/features, preview overlay, H264 production and AI output-ring writing.

```text
FW camera/RTSP input -> unified RAW NV12/FD lease -> LACAI
LACAI overlay/encode -> released AI ring ABI -> FW RTSP/UI/recording consumers
```

FW must not require LACAI to open a sensor, discover RTSP credentials, decode RTSP,
or depend on FW-private headers. LACAI must not modify FW-owned RAW memory in place.

## Input and lease contract

The baseline endpoint is `StartStream`/`StopStream` on Camera1:

- bus `com.vnpt.camera.Camera`, object `/com/vnpt/camera/Camera`, interface `Camera1`;
- request identifies `camera_id`, `channel_id`, `stream_id=third`, `transport=dmabuf`,
  and a stable unique `consumer_id`;
- a successful start returns `stream_handle` and the effective profile;
- repeated start for the same identity is idempotent; stop requires the matching handle
  and consumer identity;
- a restart invalidates old handles and requires a new acquisition.

FW exports one logical source through the existing RAW transport: Unix `SOCK_SEQPACKET`,
one 104-byte metadata packet plus one `SCM_RIGHTS` FD, and an 8-byte `buf_id` ACK.
Metadata includes dimensions, format, plane count, offset/stride/size, memory bounds and
timestamps. The current camera-0 socket is `/tmp/camera_ai/0_third_ai.sock`.

The producer must cap each consumer at four unacknowledged buffers and drop new frames
when that bound is reached. It must bound slow-client handling and keep one client from
blocking other consumers. ACK means the final reader has completed; timeout, disconnect,
FD close, socket send, or appsrc acceptance is not completion. After disconnect/crash,
FW must quarantine or otherwise prove quiescence before reusing a buffer.

FW must document allocator/modifier, cache/fence semantics, profile limits, clock domain,
completion signal and recovery behavior. A received FD is not evidence of zero-copy.

## Output contract

LACAI writes H264 byte-stream access units to the released version-4 shared rings:

- `encoded_ai_detect0_cam0_ch0` → `/live/ai/detect0`;
- `encoded_ai_detect1_cam0_ch0` → `/live/ai/detect1`.

The ring ABI, sequence/seqlock, consumer registration, wakeup, keyframe and SPS/PPS
behavior must remain compatible. Exactly one writer owns each ring. FW must advertise
viewer demand before the first frame; discovery must not wait for ring sequence > 0.
Ring replacement, reader cleanup and profile changes require an explicit generation and
reconnection policy. LACAI does not promise a separate preview ring for every source
unless FW supplies a versioned output registry mapping source, ring and RTSP mount.

## Control compatibility

The replacement must retain the existing AI1 endpoint:

- `SetModelEnabled(task, enabled)`;
- `QueryModel(task)`;
- `ListModels()`;
- `ModelStateChanged(task, enabled)`.

Fields remain string-valued as currently defined. Existing launcher-visible executable
name `cameraai_app`, configuration location and consumer identity remain compatible during
migration. FW must provide one control owner and must not assume one process per feature.

## Acceptance gates

FW must return an owner, evidence, target release and limitations for each gate:

1. RAW correctness: padded stride, nonzero offset, malformed packet and FD closure.
2. Lifetime: slow AI, duplicate/stale ACK, app crash, socket loss and FW restart.
3. Lifecycle: repeated start/stop, profile change, source epoch change and drain.
4. Coexistence: LACAI with main/sub, recording, RTSP viewers and thermal load.
5. Output: first viewer/keyframe, dead reader, ring replacement and oversized AU.
6. Security: peer credentials, socket ownership, ancillary validation and consumer
   authorization; `consumer_id` alone is not authentication.

Each report must identify FW image/commit, profile, camera count, model/workload, latency,
memory/FD/pool high-water, drops, hold time and recovery traces. “FD passed”, “no crash” or
“source compiles” does not close a gate.

## Change control

FW must not change the 104-byte legacy wire packet, socket naming, ACK meaning, D-Bus
spelling, ring IDs, ring ABI or RTSP mounts without a versioned compatibility plan and
AI APP lead review. New capabilities such as 4K RAW, multi-source preview or richer sync
metadata should use a new versioned endpoint while the migration baseline remains usable.

Detailed evidence and P0/P1/P2 requests are in
[FW camera integration requirements](fw_camera_integration_requirements.md).
