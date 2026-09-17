# Released FW ring sink wrapper

This document defines the AI-side boundary for the released FW encoded shared-memory ring: the
canonical v5 layout, the production writer, the dated board evidence, and the obsolete SDK
`ring_sink` wrapper that must stay clearly marked as stale.

**Status:** board-smoke — on 2026-09-16 the eSDK-built production binary wrote the v5 ring on
`.98` against the compatibility camera simulator and the FW v5 ring reader; released-FW reader
integration, DMA completion and thermal qualification remain open. **Layer:** adapters.
**Source:** `include/vqec/vision/ai/contracts/vqec_vision_fw_ring_layout.hpp`,
`src/adapters/qualcomm/vqec_vision_qtiv_renderer.cpp` (`fw_ring_writer`),
`src/adapters/fw_output/vqec_vision_ring_sink.{hpp,cpp}` (obsolete).

## Responsibility

- Define the single canonical AI-side ring layout and keep adapters from re-declaring it.
- Provide the production writer that creates/attaches the ring without unlinking another writer.
- Keep the obsolete SDK `ring_sink` wrapper documented as stale and out of production.
- Do not claim released-FW reader integration, DMA completion or thermal qualification.

2026-09-16 ABI decision. The canonical AI-side definition of the released FW encoded ring is
`include/vqec/vision/ai/contracts/vqec_vision_fw_ring_layout.hpp`: **version 5, 16 slots, 1 MiB
payload, 4096-byte header, 1232-byte slot header, magic `0x4C414341`**, matching the deployed FW
RTSP reader (`vqec_vision_ring_rtsp.py`, offsets documented there). Adapters must include this
header and must not re-declare offsets, sizes or the version. The production writer is
`fw_ring_writer` inside `vqec_vision_qtiv_renderer.cpp` (AI owns encoded ring production); it
creates with `O_CREAT|O_EXCL`, attaches and validates an existing ring, and never unlinks or
clobbers another writer's mapping. Slot publication uses a release fence before the seqlock closes
and before the header write sequence advances; the FW reader must use a matching acquire.

## Board evidence (2026-09-16, `.98`)

The eSDK-built production binary ran against the compatibility camera simulator and the FW v5
ring reader (`vqec_vision_ring_rtsp.py`). The reader self-check reported
`header=4096 slot_hdr=1232 slots=16 payload=1048576`; the ring file was 16,801,024 bytes
(4096 + 16 x (1232 + 1 MiB)). A host TCP `ffprobe` on
`rtsp://192.168.138.98:8554/live/ai/detect0` returned H.264, 1920x1080, `30/1`, and an 8 second
decode received 229 frames (~30 FPS) with in-band SPS/PPS, so the new writer filled
`frame_id`/`timestamp_ns` and the reader started from a real IDR. D-Bus FR enrollment/retry/delete
and runtime transitions passed, cascade reported `embedded=2 cascade_failed=0`, and the service
wrote the ring without unlinking or clobbering. This is compatibility-camera and synthetic-reader
evidence: released-FW reader integration, DMA completion and thermal qualification remain open.

## Obsolete SDK `ring_sink` wrapper

The rest of this section describes the **obsolete** SDK-based `ring_sink` wrapper. It was written
against FW commit `139d335` (v4 / 2 MiB). Its 2 MiB field below does NOT match the canonical v5
ring (1 MiB). It is **stale relative to the deployed v5 ring**, is not the production writer and is
not built in the default configuration.

Do not re-enable it as the production writer until its constants, `preview_limits` and this
document are re-baselined against a released `camera_ai_common` v5 header (which is not present in
the current eSDK sysroot). The earlier "AI must not duplicate the shared ABI" rule still holds:
the single duplicated layout now lives in one contracts header, not inline in the adapter.

Source-only optional adapter against the reviewed `camera_ai_common` API. No SDK files copied; no
configure/build/board test run. Baseline FW commit:
`139d335913e19e5a33a36fa8f8d706009892db44`.

- `ring_sink` borrows an ALREADY OPEN `SharedMemoryFrameRingBuffer`, with independently configured
  detect index (0/1), camera/channel, source epoch and even NV12 geometry.
- Caller supplies the expected nonzero `mapping_generation` of that same ring instance.
- Caller also supplies a separate nonzero `dispatch_generation` unique across output bindings in
  the runtime. This ID, not the SDK counter, is returned in neutral demand and checked against
  every write. Never recycle it while queued outputs can exist.
- SDK `mapping_generation` still checks close/reopen of the borrowed object independently.
- Both IDs are required; old configurations with only the SDK generation are rejected.
- The source-only `output_generation` allocator now supplies monotonic dispatch IDs; see
  [output generation](output_generation.md). Runtime must share one allocator across all rebuilds;
  allocation is not yet wired into a running ring supervisor.
- No open/create/replace/unlink/retry occurs inside this wrapper. Runtime owns ring creation before
  the first viewer, single-writer enforcement, lifetime and shutdown.
- Before query/write, verify open state, fixed generation, expected ring ID, 16 slots, 2 MiB
  payload and valid profile. `write` also validates exact configured source epoch/
  camera/channel/geometry, frame PTS availability and H264 envelope bounds. Frame ID/PTS
  correlation to encoder jobs is checked upstream; this wrapper cannot infer that identity. Do not
  map generation to camera epoch. All calls and external ring operations serialize.
- Mapping generation detects close/reopen on the borrowed SDK object. It does NOT detect another
  process unlinking/replacing the named object without this SDK writer remapping. Prevent external
  replacement while active or implement a coordinated reopen protocol; do not claim this wrapper
  solves cross-process split mappings. New ring instances need fresh wrappers and independent
  runtime generation handling; do not reuse queued output across instances just because both SDK
  counters start at 1.
- Metadata matches legacy writer: `stream_id=ai`, `codec=H264`, `format=byte-stream`, `stride=0`,
  camera/channel, width/height, original `frame_id`/`timestamp_ns`, keyframe and SPS/PPS. Use
  `push(header, data, size)`: SDK copies encoded bytes directly into the shared slot. No
  intermediate encoded vector, no claim of zero-copy. Parameter sets are copied into header
  vectors, within 512-byte bounds. Caller supplies cached parameter sets as the legacy encoder did;
  wrapper does not parse/cache SPS/PPS or infer IDR.
- FW errors map to neutral status; unknown errors become `io_error`. No silent success, retry or
  per-frame RPC. `active_consumer_count` is the legacy SDK count, not independently verified
  heartbeat freshness. Stale consumer optimization remains a FW review request. Locks and
  notifications inside SDK may block; no bounded-latency guarantee is made.

## Open-options boundary and SDK recovery risk

`vqec_vision_ai_fwout_rgsnk_make_open_options` constructs the released writer options without I/O:
detect0/detect1, 16 slots, 2 MiB, `create_if_missing=true`, `replace_existing=false`. Invalid
selection leaves the destination unchanged. This is NOT a safe-open implementation or single-writer
lock.

Reinspection of `shared/common/src/ring_buffer.cpp` `open()` shows that even with
`replace_existing=false`, `create_if_missing=true` can unlink/recurse on short mappings,
version/slot layout mismatch or insufficient payload capacity. Invalid magic also enters in-place
initialization. Thus callers must not treat this flag as a promise to preserve an incompatible live
mapping. Do not automatically retry open on faults or profile changes. Runtime must establish
exclusive writer ownership and coordinate any migration with FW readers. SDK reader remapping code
is not board qualification.

FW SDK request: separate create-exclusive, attach-validated-without-mutation and explicit
coordinated replacement; return layout mismatch without initialization or unlink and bound recovery
attempts. AI must not duplicate the shared ABI to work around this. Until that contract exists,
startup using legacy options carries the reviewed SDK recovery behavior and needs
deployment/reader migration approval.

## Build integration details

- Enable `VQEC_VISION_AI_ENABLE_FW_RING` only in a parent build that already defines a
  version-pinned SDK CMake target selected by `VQEC_VISION_AI_FW_RING_TARGET` (default
  `camera_ai_common`). That target must export headers AND all transitive platform/link
  dependencies. No assumed `find_package` configuration or relative sibling includes.
- Compile-time assertions check known ring version/constants, not full ABI equivalence. SDK
  provenance/license, toolchain/layout and released RTSP reader test remain required.
- Optional tests check unopened-ring rejection, configuration, and transactional metadata mapping
  using the actual SDK `RingFrameHeader`. They create no shared memory. Coverage includes
  frame/timestamp/source/geometry, codec/framing, keyframe, payload size, copied SPS/PPS,
  stale-epoch rejection, empty parameter sets and original zero PTS preservation. Live write/RTSP
  and real mapping replacement tests remain pending.

## Limits and next work

- Released-FW reader integration, DMA completion and thermal qualification remain open.
- The stale SDK `ring_sink` wrapper must not be re-enabled as the production writer before its
  constants, `preview_limits` and this document are re-baselined against a released
  `camera_ai_common` v5 header absent from the current eSDK sysroot.
- Safe ring open/recovery and single-writer supervision, plus live RTSP/UI qualification, remain
  open.

## See also

- [Model output manifest](model_output_manifest.md)
- [Output generation](output_generation.md)
- [Preview contract](preview_contract.md)
- [FW release compatibility](../contracts/fw_release_compatibility.md)
