# FW release compatibility baseline

Date: 2026-09-06. AI APP implementation baseline authorized by the lead.
FW optimization requests remain proposals, not FW-approved changes.
Evidence: sibling vqec_camera_service commit
`139d335913e19e5a33a36fa8f8d706009892db44`, read-only source review.
No board test, performance result or drop-in replacement qualification is implied.
Source paths in this document are relative to that FW repository, not this repo.

## Compatibility matrix and acceptance gates

| ID | Existing external behavior to preserve | New owner / current gap | Required acceptance |
|---|---|---|---|
| FW01 | Camera StartStream: third, dmabuf, consumer_id; reply stream_handle; matching StopStream | camera adapter source exists | Real RPC + uncertain response/retry + no duplicate lease |
| FW02 | NV12/FD, SOCK_SEQPACKET, 104-byte header, one SCM_RIGHTS FD; 8-byte buf_id ACK | camera adapter source exists | Padding/offset, malformed packet, slow reader, disconnect, exact session ACK |
| FW03 | AI draws onto process-owned full-resolution NV12, never shared source in-place | output renderer missing | Source unchanged; coordinate/label/tracking golden at each profile |
| FW04 | AI produces H264 byte-stream/AU, keyframe + SPS/PPS metadata | encoder missing | First viewer, GOP join, frame/PTS association, overload, profile change |
| FW05 | detect0/detect1 rings and v4 shared-memory ABI | guarded ring adapter source exists; runtime/open integration missing | Released RTSP reader consumes new writer without modification |
| FW06 | Consumer demand enables frame submission to preview encoder; inference is independent | output runtime missing | Cold boot, no viewer, first/last viewer, dead reader; main/sub unaffected |
| FW07 | AI D-Bus model methods/signal and persisted task config | compatibility server missing | Request/reply/signal fixtures + restart persistence and failure cases |
| FW08 | cameraai_app executable, bundle working directory, unique lease and ring per instance | service/package missing | Existing launcher starts/stops new executable with compatible arguments |
| FW09 | Effective input profile changes rebuild media adapters; stale frame drain keeps live edge | supervisor recovery/scheduler missing | Resolution/FPS changes and source restart do not leak leases or reuse stale results |

## Camera input and ownership

Use the current effective third profile; do not require 4K, change wire layout or
start another camera source. See fw_camera_integration_requirements.md and the
camera_legacy_adapter/source_lifecycle architecture documents for exact parsing.
The raw branch is demand-driven by Camera Service leases, not always-on broadcast.

Old app ACKs after copying all required pixels and completing its resize reads,
before inference/drawing/encoding. That location is NOT a rule for an FD-backed
asynchronous adapter. ACK only after every reader of that source allocation completes.
Timeout, fd close, socket disconnect and appsrc acceptance are not completion.
Preview needs a bounded AI-owned writable surface pool or an explicitly agreed
exclusive-write contract. Do not draw onto FW's shared allocation.

Evidence: application/ai_app/src/video/DmabufFrameSource.cpp startCameraLease,
stopCameraLease, receiveLatestRawFrame; application/ai_app/apps/dmabuf_main.cpp
release_input/processNv12/encoder->write; platform/camera_service/src/camera_service.cpp
third_ai_consumers and third_ai_enabled.

## Encoded output ABI

- detect0: `encoded_ai_detect0_cam0_ch0`, RTSP `/live/ai/detect0` (default).
- detect1: `encoded_ai_detect1_cam0_ch0`, RTSP `/live/ai/detect1` (optional second producer).
- Exactly one writer per ring; detect index is NOT a feature ID or camera selector.
- This released identity space supports two fixed outputs, not 16 independently selected
  source previews. Multi-source inference can run headless, but additional preview routes
  require an FW-owned versioned output registry/ring contract and backend/UI routing update.
- Header stream_id `ai`, codec `H264`, format `byte-stream`, stride 0; actual width/height,
  frame_id, timestamp_ns, is_keyframe, cached h264_sps/h264_pps and encoded payload.
- Shared ring magic 0x43414952, version 4. Existing AI writer uses 16 slots with
  2 * 1024 * 1024 bytes per payload. Keep compatible layout across profile rebuilds.
- Preserve consumer registration, sequence, seqlock, wakeup and parameter-set behavior;
  matching struct field names alone does not establish cross-process ABI compatibility.
- Use a pinned FW contract/ring SDK with agreed toolchain/ABI. Do not copy pthread/
  std::atomic shared layouts into portable core or include sibling source by relative path.
  No such SDK dependency is wired yet. Distribution/license review precedes code reuse.

Released writer uses v4l2h264enc, baseline profile, h264parse config-interval=-1,
Annex B access units. Codec2 is an optional measured replacement, not a new wire contract.
Retain source correlation independently of internal graph PTS; no guessed FIFO
association after dropping frames. AU size overflow must be visible, not memory overwrite.
Ring replacement/unlink requires reader-reconnection verification; never blindly recreate
on a resolution change. New 4K profiles must reassess AU limits and memory budgets.

Evidence: shared/common/include/camera_ai/common/ai_output.hpp and ring_buffer.hpp;
shared/common/src/ring_buffer.cpp; application/ai_app/src/video/SharedRingVideoEncoder.cpp.

## Viewer demand and backend/UI

Web UI -> MediaMTX sourceOnDemand -> RTSP ring consumer -> AI encoder submissions.
MediaRuntimeService advertises `cam0-ai` before the first encoded frame; gating path
discovery on ring sequence > 0 deadlocks cold start. Web preview currently selects
detect0; RTSP exposes both detect mounts. Do not promise detect1 UI selection.
No AI-owned RTSP server or new backend video route is needed for this replacement.

Evidence: application/web_server/src/media_runtime_service.cpp read_active_rtsp_streams;
application/web_server/src/camera_handlers.cpp; application/rtsp_server/src/rtsp_service.cpp.

## AI command compatibility

Service `com.vnpt.camera.AI`; object `/com/vnpt/camera/AI`; interface `com.vnpt.camera.AI1`.
Preserve string-valued fields and exact method/signal spelling:

| Method/signal | Request | Response/event |
|---|---|---|
| SetModelEnabled | task, enabled = "true" or "false" | ok = "true"; failure ok = "false", error |
| QueryModel | task | ok, task, enabled |
| ListModels | no task fields | ok, count, modelN.task, modelN.enabled (N starts at 0) |
| ModelStateChanged | server signal on successful set | task, enabled |

Legacy tasks: object, face, firesmoke, person. These are model tasks, NOT the 13
commercial feature IDs. Server starts on the multi-model path; legacy single-model
path is not equivalent. Current SetModelEnabled updates live state and attempts to
save ai_pipeline.json; persist failure logs a warning but still returns ok. Preserve
wire behavior during replacement; a stronger transactional persistence/error contract
needs explicit FW/client agreement. Record persistence health separately meanwhile.

Endpoint implementation is confirmed; direct backend/UI callers of these model methods
were not found in the application/shared search. Real client traffic must be captured
before claiming full control-plane equivalence. Multiple processes cannot independently
own this same well-known bus name: initial service has one control owner.

Entitlement remains a separate reviewed extension. Do not invent grants from enabled
booleans, disable existing tasks because no new license server exists, or bypass the
new policy gate. Deployment must explicitly define authorized compatibility scopes
and the transition to signed grants before enabling production output.

Evidence: application/ai_app/src/utils/DbusCommandServer.cpp;
application/ai_app/ai_models/ai_pipeline.json; shared/common/src/rpc_endpoint_catalog.cpp.

## Deployment and next-stage inputs

Preserve launcher-visible cameraai_app, default ai_pipeline.json/model-relative paths,
camera selection, output-ring selection, consumer identity and shutdown behavior.
Inventory exact CLI flags before writing main; do not rename binary with the source prefix.
Evidence: scripts/run_services.py, application/ai_app/README.md and apps/dmabuf_main.cpp.

FW BSP/FW software review requests (not implemented here):

1. P0: FD allocation/import/cache/completion and disconnect/crash recovery guarantees;
   shared input must not be recycled while AI hardware can still read it.
2. P0: versioned ring/IPC SDK and reproducible reader/writer ABI qualification.
3. P1: stale/dead consumer handling; active slot counts alone can keep encoding enabled.
4. P1: bounded slow-client isolation, reconnect/profile transitions, timestamp clock mapping.
5. P1: first-viewer keyframe policy and jointly measured preview/inference/recording budgets.
6. P1: persistence failure semantics and single control owner for multi-instance deployments.
7. P1: unified `raw_source_ref` registry for 1..16 logical sources. On AI Box, FW owns
   RTSP credentials/demux/decode and exports the same RAW lease contract seen on AI Camera.
8. P1: versioned preview-output registry beyond fixed detect0/detect1, with unique writer,
   source correlation, viewer discovery and stale-consumer semantics.

Acceptance is end-to-end with released FW/RTSP/Web UI and representative model kits,
not just a successful GStreamer graph. No external endpoint/ABI change without owner review.
