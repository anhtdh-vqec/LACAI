# FW–AI APP integration requirements: frame transport, safety and optimization

This document lists the FW/AI APP coordination requirements for the existing third NV12/FD
frame transport, the P0 safety conditions before production, and the P1/P2 stability and
extension requests. It is self-contained for hand-off to the FW team and is based on
read-only source review, not board measurement.

**Status:** planned — proposal for review/sign-off dated 2026-09-06, not an FW commitment;
conclusions are source-read only. **Layer:** contracts. **Source:** `n/a`.

Date: 2026-09-06. Recipient: FW BSP and FW software team. Proposer: AI APP team. Status:
proposal for review/sign-off, not an FW commitment.

## Responsibility

- Fixes the existing FW integration baseline and the profile/control/lifecycle behavior AI
  APP must follow.
- Separates two milestones: happy-path development on the current protocol, and production
  acceptance only after the P0 safety conditions below are confirmed.
- Must not use "current compatibility" to waive ownership or hardware-completion
  verification.

## 1. Current integration decision

Companion baseline: [FW release compatibility](fw_release_compatibility.md) covers
AI-owned overlay/H264/ring, legacy AI control and backend/UI integration. Its final
section adds ring SDK, stale-consumer and deployment review requests. These are not
requirements to change the current raw transport before AI source development.

AI APP currently receives exactly the `third` NV12/FD branch; the target contract is
collectively called FW RAW source. AI Box must normalize RTSP into the same RAW contract
before the AI APP boundary. Do not require FW to change the protocol or add 4K before AI
writes the receiver and adapter. Do not open camera sensor/ISP directly and do not take
encoded main/sub to decode again. One AI runtime takes one connection per logical RAW
source and shares frame/compute across features.

Separate two milestones: the happy path can be developed/integrated with the current
protocol; production acceptance only after the P0 safety conditions below are confirmed.
Do not use "current compatibility" to waive ownership or hardware-completion verification.

This document is self-contained for hand-off to FW. The source paths below are relative to
the `vqec_camera_service` repository, snapshot commit
`139d335913e19e5a33a36fa8f8d706009892db44`. Conclusions are based on source reading, not
board measurement. Target per project information: QCS6490 / Qualcomm Linux 1.8.

## 2. FW baseline AI will follow

### 2.1 Data plane

```text
qtiqmmfsrc -> master NV12 -> tee
  -> queue (leaky downstream, max 2 buffers)
  -> videorate (drop-only)
  -> qtivtransform + caps per third_profile
  -> appsink -> Unix SOCK_SEQPACKET + SCM_RIGHTS -> AI
```

- This is post-ISP NV12 before encoding, not sensor RAW/Bayer.
- The producer accepts only one GstMemory, FD-backed, with GstVideoMeta. The fd-backed check
  in code does not by itself prove every FD can be imported into AI hardware.
- One packet holds a packed 104-byte FrameHeader and one FD; the ACK is an 8-byte buf_id.
- The wire has no magic/version/endianness declaration; format is a GstVideoFormat enum. AI
  will isolate this compatibility inside the camera adapter and pin the ABI of the
  integrated FW build.
- Socket camera 0: `/tmp/camera_ai/0_third_ai.sock`.
- Socket camera N other than 0: `/tmp/camera_ai/0_third_ai_camN.sock`.
- The producer's actual channel is 0; stream route `third`, socket consumer route `ai`.
- Do not use the shared transport config default `/run/camera_ai` for this producer.
- Server fan-out is at most 8 clients; each client holds at most 4 unACKed buffers. When 4
  are full, new frames are dropped; appsink max-buffers=1, drop=true.
- Send to each client sequentially; current send timeout is 2 seconds/client. This is a code
  limit, not an accepted latency SLA.

| Existing metadata | How AI uses it |
|---|---|
| buf_id | ACK token within the correct session, not treated as a global frame ID |
| width, height, format, n_planes | Validate before import; do not hardcode 4K |
| offset[4], stride[4] | Preserve the original layout; do not assume stride equals width |
| size, mem_offset, mem_maxsize | Validate view and allocation with checked arithmetic |
| pts_ns, dts_ns, duration_ns | Preserve valid values; handle GST_CLOCK_TIME_NONE separately |

If CPU mapping is used for diagnostics: a plane starts at
`mapped_base + mem_offset + offset[i]`. Do not fall back to packed NV12 when offset/stride
is invalid. Do not use the FD number as allocation identity; an FD can be reused. The wire
has no color matrix/range, modifier, fence, source epoch or UTC mapping yet.

### 2.2 Existing profile

| Camera | Default | Resolution / max FPS allowed by code |
|---|---|---|
| 0 | 1920x1080 @25 | 1280x720 @25; 1920x1080 @25; 2560x1440 @20 |
| 1 | 854x480 @25 | 640x360 @25; 854x480 @25; 1280x720 @25 |

FPS is chosen from 10/15/20/25 and must not exceed the resolution ceiling.
The default does not replace the running effective profile; AI checks the response and every
frame. Raw 3840x2160 is not yet allowed by the third API, even if the source/master may be
larger. AI does not raise the third profile on its own: this is a shared branch, and a
change affects other consumers.

### 2.3 Control plane and lifecycle

D-Bus bus name `com.vnpt.camera.Camera`, object `/com/vnpt/camera/Camera`,
interface `com.vnpt.camera.Camera1`; the implementation defaults to the system bus, with
session-bus configuration for suitable environments. RpcClient port 9101 is the
endpoint-mapping key, not a requirement for AI to connect to TCP 9101.

1. StartStream(camera_id, channel_id=0, stream_id=third, transport=dmabuf,
   a distinct and stable consumer_id for the current acquisition).
2. Keep the stream_handle; read the effective profile, connect the socket with bounded
   retry/backoff.
3. Receive/validate the frame; ACK only a frame already dropped before submit or with every
   reader finished.
4. On normal stop: block submit, drain readers, ACK, close the socket, then StopStream with
   the exact stream_handle and consumer_id that acquired it.
5. On reconnect: increment the internal session epoch, reset temporal state appropriately;
   do not ACK an old-session token over a new socket. A source restart must reacquire the
   lease.

The lease consumer_id is neither the route string `ai` in the socket name nor an auth token.
StartStream has an idempotent fast path keyed on existing camera/channel/stream/consumer; do
not change transport silently under the same identity and then treat it as a new
acquisition. A successful StartStream does not prove the socket/first frame is ready. Do not
use third_ring_id or the third debug-ring state as evidence that the raw socket is healthy.

## 3. P0 requirements: safety conditions before production

P0 can be resolved with existing BSP evidence/contracts or FW changes where missing; do not
assume every item needs a new API.

### FW-AI-01 — Do not recycle a buffer while hardware is still reading

Owner: FW BSP + FW software; AI APP coordinates fault testing.

Today the client session destructor unrefs all pinned buffers even if unACKed. Prune
client/disconnect, stop producer or profile change may enter this path. A still-open FD at AI
only holds the allocation; it does not guarantee the pool will not overwrite the contents.

Requirements:

- Normally return the pool only after ACK representing the last reader completed.
- On disconnect/crash/timeout: prove hardware has quiesced before recycle, or quarantine the
  old allocation/pool with a memory bound and clear escalation.
- Do not time out and treat it as completion; do not quarantine indefinitely without
  recovery.
- Specify which side performs reset/quiesce, the reset scope affecting camera/encoder/NPU,
  the release conditions and the state reported to the supervisor.
- Profile change/stop must obey the same invariant, not only consumer crash.

Acceptance: fault injection while hardware is reading; the trace proves the next write/reuse
only happens after completion/quiescence. "No crash observed" alone is not accepted.

### FW-AI-02 — Confirm layout, color and memory synchronization

Owner: FW BSP; AI APP verifies import/preprocess.

- Fix the actual FD type/allocator, linear NV12 or another modifier, plane/stride/alignment.
- Fix NV12 color matrix/range/chroma siting. If the wire does not carry it, provide a fixed
  versioned profile; do not change it silently between firmware releases.
- Fix by what mechanism the producer write is complete when a frame is sent; if implicit
  sync exists, specify which producer/consumer API performs the wait/import and the test
  evidence.
- Fix the CPU cache synchronization mechanism for debug map; do not confuse cache sync with
  device fence.
- State how last device read completion is confirmed before ACK.

Acceptance: padded-stride/nonzero-offset fixtures, color bars and golden tensor; no
plane/color deviation and no read past the view; producer-write/AI-read stress without torn
frames.

### FW-AI-03 — Protect the raw socket and tighten the parser

Owner: FW software; BSP supports image policy.

- The socket directory/file has managed owner/group/mode; do not expose raw frames to
  arbitrary users. Check peer credentials (for example SO_PEERCRED) and bind stream
  authorization to the client per FW design.
- A self-declared consumer_id must not be treated as authentication.
- recvmsg checks MSG_TRUNC/MSG_CTRUNC, payload length, ancillary type/length and exact FD
  count; close every extra/error FD, use CLOEXEC, validate dimensions/planes/ranges with
  overflow checks.
- ACK parsing checks the full packet length; a duplicate/stale/unknown ACK must not release
  the wrong slot.
- Do not unlink/bind a socket in an untrusted directory without ownership control.

AI will tighten its own parser; FW is responsible for the producer-side endpoint and ACK
receiver.

Acceptance: unauthorized peer rejected; malformed/truncated/extra-FD packets do not leak FD,
do not crash and do not wrongly release a buffer.

## 4. P1 requirements: stability and optimization of the integration

| ID | Main owner | Requirement / acceptance output |
|---|---|---|
| FW-AI-04 | FW software | Isolate slow clients: bounded/nonblocking send or bounded worker; one client must not hold another client's send path up to the 2-second timeout. Measure the impact when encode/record run together. |
| FW-AI-05 | FW software + BSP | Publish total and per-client pool budget, max hold time and drop policy. Do not only raise max_inflight to hide a bottleneck; report pinned high-water/drop reason. |
| FW-AI-06 | FW software | GetStatus reflects the raw branch separately, socket readiness, connected clients, effective profile, producer instance and errors; distinguish it from the third debug ring. |
| FW-AI-07 | FW software + AI APP | Reconfigure has a drain boundary and discontinuity notification; avoid multiple clients changing each other's profile. Fix owner/arbitration and recovery when a Start/Stop response is lost. |
| FW-AI-08 | FW software | Lease cleanup when the app dies: policy with owner, deadline and reconcile; control-lease cleanup must not lead to DMA recycle without quiescence. |
| FW-AI-09 | FW BSP | Hand over an image/GStreamer/plugins/allocator/SDK compatibility table; package runtime dependencies and device permissions so AI IPK is independent. Do not force AI to link source from the FW repo. |
| FW-AI-10 | FW software + AI APP | Counters and timing with clock domain: sent/acked/dropped, pinned, send latency, hold time, reconnect, pool/FD usage; bounded logging that contains no images/biometrics. |

Optimize the data path based on profiling: keep FD + metadata, avoid a full-frame CPU copy;
do not force FastCV for every FW operation if the current qtivtransform path meets
correctness/KPI. AI may ACK after preprocess once it proves the output is independent and no
input reader remains; it does not need to hold the camera frame for the whole
inference/temporal window when there is no dependency.

## 5. P2 requirements: versioned extension that does not block the baseline

### FW-AI-11 — Raw 4K and traffic

Owner: FW BSP + FW software; AI APP provides the workload.

- Evaluate adding third NV12 3840x2160; 25 FPS is a proposed target, not a commitment.
- Confirm sensor mode, ISP/transform/DDR/pool budget together with main/sub encode, record,
  camera count and thermal steady state. Do not just add a resolution to the validation
  table.
- Evaluate bypass scale when geometry matches master, only if memory lifetime/isolation and
  camera-pool starvation are proven; do not assume exporting master is better.
- Traffic requires additional FPS/shutter/timestamp/orientation/calibration capability
  suitable for the model; fix it per workload and do not consider 4K sufficient for every
  traffic task.

NV12 4K packed = 12,441,600 bytes/frame, about 311 MB/s at 25 FPS for the one-directional
payload; this excludes padding and read/write/copy passes. This is a capacity calculation,
not a benchmark.

### FW-AI-12 — Next protocol

Owner: FW software; BSP and AI APP review the schema.

Propose adding magic/version/header_size/message_type, explicit endianness,
producer/session/source epoch, profile revision, portable pixel format/modifier/color
information, allocation generation, clock domain and synchronization description; a sync
handle only when the mechanism requires it. Capability API returns effective profile, limits
and transport endpoint.

Keep the legacy endpoint/protocol during migration or add a separate versioned endpoint. Do
not insert fields into the 104-byte packed header on the old endpoint and break deployed AI.
The new version must have serializer/decoder fixtures, a compatibility matrix and a rollback
test.

### FW-AI-13 — Unified multi-source RAW registry

Owner: FW software + BSP; AI APP provides workload/admission fields.

- Publish 1..16 logical source IDs, each source's effective profile/rate and
  concurrent-combination limits; `16` is a configuration ceiling, not a default or SoC
  commitment.
- Provide a stable `raw_source_ref` and the same RAW descriptor/lease/FD/ACK/epoch semantics
  for both AI Camera and AI Box; a new feature must not create another producer when it
  shares a source.
- On AI Box, FW resolves RTSP itself, manages credentials, demux/decode and exports only
  RAW. Do not pass RTSP URI/codec/password or decoder lifecycle through the AI APP contract.
- Publish RAW profile/format/modifier, reconnect/discontinuity, timestamp/clock,
  allocation/synchronization and the whole-system budget when running with record/live
  stream.

### FW-AI-14 — Preview output registry for multiple sources

Owner: FW software; backend/UI/RTSP service co-review.

The release has only fixed `detect0`/`detect1` for cam0/ch0. To expose independent preview
for multiple sources, FW needs a versioned registry contract with unique writer ownership,
source -> ring/mount/UI mapping, lifecycle/generation, first-viewer keyframe, stale-reader
cleanup and backward compatibility. AI APP will reject an unresolvable `preview_output_ref`;
an inference-only source sets its preview surface count to 0.

### FW-AI-15 — Memory/synchronization evidence per source

Owner: FW BSP + FW software.

The capability/admission response must state allocator/modifier, allocation size with
stride, pool bytes/high-water, acquire/completion synchronization and quarantine/reset
behavior. Acceptance must trace each boundary to distinguish importing the same allocation
from an implicit copy; do not call the whole pipeline zero-copy just because IPC sends an
FD.

## 6. AI APP responsibilities now

- A private legacy-wire camera adapter; do not expose Gst/vendor types in the neutral core.
- Validate descriptor, correct FD ownership, original strides/offsets, session-bound ACK.
- Bounded queue/inflight within the FW 4-buffer limit; admission for shared models/ROI.
- A frame not yet submitted may be dropped + ACK; a submitted frame holds the lease until
  last-reader completion.
- Control Start/Stop and handle reconnect; do not change the shared-branch profile.
- Do not guess missing color/fence guarantees; use the BSP-confirmed integration
  configuration.
- Do not accept UBWC/multi-FD or a new protocol by heuristics without clear support.
- Collect receive/preprocess/inference/hold/release metrics and build mock/fault/board
  tests.
- Do not use a separate socket for each of the 13 features; handle entitlement and share
  compute in the AI runtime. FR/attributes/traffic do not change the frame ownership
  contract.

## 7. Acceptance matrix and hand-off

| Test group | Scenario | Criterion |
|---|---|---|
| Correctness | Default profile, padding, nonzero mem_offset, invalid timestamps | No wrong/past-view read; tensor compared to golden within model tolerance |
| Backpressure | Slow AI, full queue, client not reading/not ACK | Memory/FD/pinned bounded; drops with reason; measure camera/record impact |
| Lifecycle | Repeated Start/Stop, app crash, socket loss, FW restart | No stale ACK; no recycle before completion; policy-based recovery |
| Reconfigure | Change resolution/FPS while AI is reading | Clear drain/recovery; no layout or temporal-epoch mixing |
| Security | Wrong peer, bad packet/ancillary, duplicate ACK | Reject with reason; no crash/leak/wrong release |
| Coexistence | AI + main/sub + record, thermal soak | Meets KPI signed by the teams per workload |

Propose a 24-hour soak for the release candidate; the official duration and workload must be
agreed. The report must include image/commit, plugin/model version, profile, camera count,
encode/record settings, CPU/RSS/FD/pool/thermal, latency p50/p95/p99, drops, and fault
recovery traces. A numeric KPI that was not measured must not be recorded as "met": the
minimum FPS, p99 frame age, max hold time, memory cap, recovery deadline and
recording-impact level must be fixed per workload.

### Requested FW response

For each FW-AI-01..15, return: owner, supported/needs fix/unsupported, source or test
evidence, plan, target release/date and remaining limitations.

Proposed coordination order:

1. Review P0 and confirm baseline/profile/memory facts before the first hardware integration
   session.
2. AI implements the legacy receiver in parallel; FW adds tests/evidence or fixes P0.
3. Measure the baseline with the real workload, choose P1 changes by bottleneck.
4. Accept P0 + release KPI; schedule 4K/new protocol separately and do not change legacy
   silently.

## 8. Sources used for cross-checking

- `shared/raw_frame_transport/src/raw_frame_wire_protocol.hpp`: header and ACK.
- `shared/raw_frame_transport/src/custom_dmabuf_raw_frame_transport.cpp`:
  BuildSocketPath, SendSample, ReturnAckLoop, destructor, AcceptLoop, ReceiveFrame/ReleaseFrame.
- `shared/raw_frame_transport/include/camera_ai/raw_frame_transport/raw_frame_transport.hpp`:
  metadata and public producer/consumer interface.
- `hal/capture/src/gstreamer_camera_adapter.cpp`: AppendRateThenTransform,
  TryAttachThirdAiBranch and DetachThirdAiBranch.
- `hal/transform/src/transform_adapter.cpp`: hardware transform and output caps.
- `shared/common/include/camera_ai/common/const.hpp`: socket path and third capabilities.
- `platform/camera_service/src/camera_service.cpp`: Start/Stop, profile validation/reconcile.
- `shared/common/src/rpc.cpp`, `rpc_endpoint_catalog.cpp` and
  `platform/ipc/dbus/com.vnpt.camera.Camera1.xml`: control transport/schema.
- `application/ai_app/src/video/DmabufFrameSource.cpp`: existing consumer for cross-check,
  not the new AI implementation, and it does not carry the OpenCV path into the new AI.

## Limits and next work

- Every FW-AI-01..15 item still needs an FW owner, evidence, target release/date and
  remaining-limitation response; none is FW-committed by this proposal.
- Numeric KPI and soak duration remain unmeasured and must be agreed before the week-4 gate.

## See also

- [FW release compatibility](fw_release_compatibility.md)
- [FW–AI APP integration contract](fw_ai_app_contract.md)
