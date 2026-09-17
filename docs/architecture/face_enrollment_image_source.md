# Enrollment image source

`face_enrollment_image_source_port` is the portable boundary for turning one authorized FW
enrollment image path into one bounded NV12 image. This document defines the adapter's
GStreamer graph, its memory contract and the limits that keep file enrollment from claiming
live-camera behavior.

**Status:** source-delivered — the Qualcomm adapter and contract test exist; on QCS6490
`.98` the production `qtivtransform engine=fcv` selection produced a validated DMA-BUF.
**Layer:** adapters. **Source:**
`src/adapters/qualcomm/vqec_vision_face_enrollment_image_source.cpp`,
`tests/contract/vqec_vision_face_enrollment_image_source_test.cpp`.

## Responsibility

- Turns an authorized `image_path` into one bounded NV12 image through a portable port.
- Accepts the immutable buffer and source epoch identity from the caller; it does not invent
  frame identifiers.
- Keeps GStreamer and Qualcomm types inside the adapter.
- Rejects empty/NUL paths and enforces the configured output bound; it does not grant
  filesystem authority.
- Does not claim end-to-end zero-copy or file enrollment acceptance.

## GStreamer graph and memory

FW enrollment requests carry an authorized `image_path`; the person does not need to stand
in front of the camera. The Qualcomm adapter owns a short lived GStreamer graph (`filesrc` +
JPEG decoder + `videoscale` + `videoconvert` + optional vendor output transform + NV12 caps
+ appsink), validates the configured output byte bound, and returns an immutable shared
image. In production mode the selected converter must allocate importable memory; the
adapter verifies that the resulting memory is DMA-BUF backed and exposes its FD through
neutral `raw_frame`; retaining the sample owner keeps the FD valid. Portable/reference mode
can instead return packed CPU NV12.

The current FastCV landmark aligner reads NV12 through a CPU-mappable FD, while the GBM
DMA-BUF returned by `qtivtransform` on QCS6490 is not directly `mmap`-able. For the cold
enrollment path only, the adapter maps that Gst buffer through its allocator and writes one
tightly packed retained memfd as `alignment_frame_`. SCRFD still consumes the DMA-BUF;
alignment consumes the packed frame with identical identity and geometry. This is one
explicit full-frame copy per enrollment request, outside the live camera hot path.

The adapter validates and enables the standard `videoscale` `add-borders` property.
Portrait and other non-matching aspect ratios are centered on the requested deployment
canvas instead of being stretched, preserving face geometry before detector preprocess.

## Authorization boundary

The image path must already have passed FW authorization and deployment policy checks.
This adapter only rejects empty/NUL paths and enforces the configured output bound; it
does not grant filesystem authority.

## QCS6490 evidence

On QCS6490 `.98`, the production selection `qtivtransform engine=fcv` after the explicit
I420-to-NV12 conversion produced a validated DMA-BUF. `memory:GBM` is not forced in caps
because the installed plugin pad template does not advertise that feature; the adapter
checks the actual returned GstMemory and fails closed when it is not DMA-BUF backed.

## Limits and next work

- The decoder's internal allocation for the original JPEG is not yet capped by this output
  limit.
- Input file size/dimensions admission and end-to-end FD/FR execution remain required
  before enabling non-empty paths in the enrollment controller.
- Hardware JPEG decode has not been established, and the converter/allocator path still
  needs board measurement.
- This source step does not claim end-to-end zero-copy or file enrollment acceptance.

## See also

- [Face enrollment image pipeline](face_enrollment_image_pipeline.md)
- [Image alignment port](image_alignment_port.md)
- [Cascade inference](cascade_inference.md)
