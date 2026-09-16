# Enrollment image source

FW enrollment requests carry an authorized `image_path`; the person does not need to
stand in front of the camera. `face_enrollment_image_source_port` is the portable boundary
for turning that path into one bounded NV12 image. The caller supplies the immutable
buffer and source epoch identity, so the source does not invent frame identifiers.

The Qualcomm adapter owns a short lived GStreamer graph (`filesrc` + JPEG decoder +
`videoscale` + `videoconvert` + NV12 caps + appsink), validates the configured output byte bound, and returns
an immutable shared image. GStreamer and Qualcomm types remain inside the adapter. The
decoded image is intentionally a neutral CPU-owned handoff; the next integration step
must pass it through the admitted Qualcomm DMA-BUF/converter path before QNN submission,
or use the equivalent vendor adapter on another platform.

The image path must already have passed FW authorization and deployment policy checks.
This adapter only rejects empty/NUL paths and enforces the configured output bound; it
does not grant filesystem authority.

The decoder's internal allocation for the original JPEG is not yet capped by this
output limit. Input file size/dimensions admission and end-to-end FD/FR execution remain
required before enabling non-empty paths in the enrollment controller. This source step
does not claim hardware decode, zero-copy or file enrollment acceptance.
