# Released FW ring sink wrapper

Source-only optional adapter against the reviewed camera_ai_common API. No SDK files
copied; no configure/build/board test run. Baseline FW commit:
139d335913e19e5a33a36fa8f8d706009892db44.

ring_sink borrows an ALREADY OPEN SharedMemoryFrameRingBuffer, with independently
configured detect index (0/1), camera/channel, source epoch and even NV12 geometry.
Caller supplies the expected nonzero mapping_generation of that same ring instance.
Caller also supplies a separate nonzero dispatch_generation unique across output
bindings in the runtime. This ID, not the SDK counter, is returned in neutral demand
and checked against every write. Never recycle it while queued outputs can exist.
SDK mapping_generation still checks close/reopen of the borrowed object independently.
Both IDs are required; old configurations with only the SDK generation are rejected.
The source-only output_generation allocator now supplies monotonic dispatch IDs;
see output_generation.md. Runtime must share one allocator across all rebuilds;
allocation is not yet wired into a running ring supervisor.
No open/create/replace/unlink/retry occurs inside this wrapper. Runtime owns ring
creation before the first viewer, single-writer enforcement, lifetime and shutdown.

Before query/write, verify open state, fixed generation, expected ring ID, 16 slots,
2 MiB payload and valid profile. write also validates exact configured source epoch/
camera/channel/geometry, frame PTS availability and H264 envelope bounds. Frame ID/PTS
correlation to encoder jobs is checked upstream; this wrapper cannot infer that identity.
Do not map generation to camera epoch. All calls and external ring operations serialize.

Mapping generation detects close/reopen on the borrowed SDK object. It does NOT detect
another process unlinking/replacing the named object without this SDK writer remapping.
Prevent external replacement while active or implement a coordinated reopen protocol;
do not claim this wrapper solves cross-process split mappings. New ring instances need
fresh wrappers and independent runtime generation handling; do not reuse queued output
across instances just because both SDK counters start at 1.

Metadata matches legacy writer: stream_id=ai, codec=H264, format=byte-stream, stride=0,
camera/channel, width/height, original frame_id/timestamp_ns, keyframe and SPS/PPS.
Use push(header, data, size): SDK copies encoded bytes directly into the shared slot.
No intermediate encoded vector, no claim of zero-copy. Parameter sets are copied into
header vectors, within 512-byte bounds. Caller supplies cached parameter sets as the
legacy encoder did; wrapper does not parse/cache SPS/PPS or infer IDR.

FW errors map to neutral status; unknown errors become io_error. No silent success,
retry or per-frame RPC. active_consumer_count is the legacy SDK count, not independently
verified heartbeat freshness. Stale consumer optimization remains a FW review request.
Locks and notifications inside SDK may block; no bounded-latency guarantee is made.

## Open-options boundary and SDK recovery risk

`vqec_vision_ai_fwout_rgsnk_make_open_options` constructs the released writer options
without I/O: detect0/detect1, 16 slots, 2 MiB, create_if_missing=true,
replace_existing=false. Invalid selection leaves the destination unchanged.
This is NOT a safe-open implementation or single-writer lock.

Reinspection of shared/common/src/ring_buffer.cpp `open()` shows that even with
replace_existing=false, create_if_missing=true can unlink/recurse on short mappings,
version/slot layout mismatch or insufficient payload capacity. Invalid magic also
enters in-place initialization. Thus callers must not treat this flag as a promise
to preserve an incompatible live mapping. Do not automatically retry open on faults
or profile changes. Runtime must establish exclusive writer ownership and coordinate
any migration with FW readers. SDK reader remapping code is not board qualification.

FW SDK request: separate create-exclusive, attach-validated-without-mutation and
explicit coordinated replacement; return layout mismatch without initialization or
unlink and bound recovery attempts. AI must not duplicate the shared ABI to work around
this. Until that contract exists, startup using legacy options carries the reviewed
SDK recovery behavior and needs deployment/reader migration approval.

## Build integration details

Enable VQEC_VISION_AI_ENABLE_FW_RING only in a parent build that already defines a
version-pinned SDK CMake target selected by VQEC_VISION_AI_FW_RING_TARGET (default
camera_ai_common). That target must export headers AND all transitive platform/link
dependencies. No assumed find_package configuration or relative sibling includes.
Compile-time assertions check known ring version/constants, not full ABI equivalence.
SDK provenance/license, toolchain/layout and released RTSP reader test remain required.

Optional tests check unopened-ring rejection, configuration, and transactional metadata
mapping using the actual SDK RingFrameHeader. They create no shared memory. Coverage
includes frame/timestamp/source/geometry, codec/framing, keyframe, payload size, copied
SPS/PPS, stale-epoch rejection, empty parameter sets and original zero PTS preservation.
Live write/RTSP and real mapping replacement tests remain pending.
