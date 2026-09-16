# Enrollment image source

FW enrollment requests carry an authorized `image_path`; the person does not need to
stand in front of the camera. `face_enrollment_image_source_port` is the portable boundary
for turning that path into one bounded NV12 image. The caller supplies the immutable
buffer and source epoch identity, so the source does not invent frame identifiers.

The Qualcomm adapter owns a short lived GStreamer graph (`filesrc` + JPEG decoder +
`videoscale` + `videoconvert` + optional vendor output transform + NV12 caps + appsink), validates the configured output
byte bound, and returns an immutable shared image. GStreamer and Qualcomm types remain
inside the adapter. In production mode the selected converter must allocate importable
memory; the adapter verifies that the resulting memory is DMA-BUF backed and exposes its FD through neutral
`raw_frame`; retaining the sample owner keeps the FD valid. Portable/reference mode can
instead return packed CPU NV12.

The adapter validates and enables the standard `videoscale` `add-borders` property.
Portrait and other non-matching aspect ratios are centered on the requested deployment
canvas instead of being stretched, preserving face geometry before detector preprocess.

The image path must already have passed FW authorization and deployment policy checks.
This adapter only rejects empty/NUL paths and enforces the configured output bound; it
does not grant filesystem authority.

The decoder's internal allocation for the original JPEG is not yet capped by this
output limit. Input file size/dimensions admission and end-to-end FD/FR execution remain
required before enabling non-empty paths in the enrollment controller. Hardware JPEG
decode has not been established, and the converter/allocator path still needs board
measurement. This source step does not claim end-to-end zero-copy or file enrollment
acceptance.

On QCS6490 `.98`, the production selection `qtivtransform engine=fcv` after the explicit
I420-to-NV12 conversion produced a validated DMA-BUF. `memory:GBM` is not forced in caps
because the installed plugin pad template does not advertise that feature; the adapter
checks the actual returned GstMemory and fails closed when it is not DMA-BUF backed.
