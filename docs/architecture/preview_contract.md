# Preview metadata boundary — R1 first slice

Status: source-only pure validation. No surface allocation, rendering, encoding,
ring SDK, dispatch, hardware completion or authorization integration is implemented.
External baseline: [FW release compatibility](../contracts/fw_release_compatibility.md).

## Identity and coordinates

preview_frame_key carries camera/channel, local source epoch, frame ID and original
source PTS. Epoch must be nonzero; frame ID and PTS zero are valid. UINT64_MAX PTS
means unavailable and is rejected on this exact-correlation preview path. A later
timestamp fallback must be explicit, never silently interpreted as UTC.

overlay_batch describes rectangles already transformed into output pixels; the first
slice supports only full-resolution identity geometry (output dimensions equal source).
Model-to-source inverse transforms belong upstream; no implicit letterbox restoration,
stretching or old-result tracking is performed. Source identity must match exactly.
Coordinates are finite, half-open extents inside the image, with positive dimensions.
RGBA is a neutral packed color value, not a vendor/native-endian pixel layout.
Labels are deliberately printable ASCII in this first slice; Unicode needs a defined
renderer/font and UTF-8 validator, not unchecked byte strings.

Limits: 128 boxes, 96 label bytes each, 4096 total label bytes, even NV12 geometry
up to 8192 on each axis. These are parser/metadata safety ceilings, not board capabilities.
Zero boxes is a valid clear-overlay batch. No policy can grant permission via this API.

The serialized output owner supplies an independent expected policy revision and
monotonic now/max-age. Revisions must be nonzero and equal; future or expired batches
are rejected. Revision equality is necessary but NOT sufficient authorization:
upstream must scope/filter feature and attribute payloads using the real output gate.
The batch timestamp is an app monotonic freshness timestamp, not source PTS.
Overlay preparation receives the freshness budget from deployment/runtime context;
it must not substitute a fixed age when building a batch.

## Encoded data

h264_access_unit_view borrows one Annex B access unit and cached SPS/PPS. Validation
checks exact source correlation, geometry, nonempty payload, <=2 MiB payload and
<=512-byte parameter sets, and a 3/4-byte initial start code. It does not parse H264,
prove AU completeness, validate SPS geometry or verify keyframe truth. The encoder
adapter must supply those guarantees using its negotiated caps/parser.
SPS/PPS representation is the FW parser's cached NAL payload; this validator does not
require start codes for parameter sets. Nonzero size always requires a nonnull pointer.

Views and metadata own no memory. Caller keeps underlying bytes immutable and alive
for the entire validation/copy operation. Async sinks MUST acquire an owner or copy
before returning; storing these borrowed pointers is not an ownership contract.

## Next slices

CPU writer/sealed-reader ownership now has source in [preview surface](preview_surface.md).
It is not a reusable pool or encoder completion mechanism; no rendering/encoding is wired.

Add move-only writable preview surface/pool ownership, independent encoder input and
result completion, bounded consumer-demand scheduling and private FW SDK ring sink.
Do not overload an encoded result as proof source/encoder input memory is reusable.
Do not bypass camera ACK/drain ownership in order to produce preview.
