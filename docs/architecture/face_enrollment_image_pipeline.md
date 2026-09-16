# Face enrollment image pipeline

FW `BeginEnrollment` may carry one authorized image path. The D-Bus adapter still calls
the ordinary enrollment port; the controller rejects a non-empty path unless it is wrapped
by `face_enrollment_image_pipeline`. The wrapper holds exactly one pending request and the
serialized service loop advances it, so a D-Bus callback never blocks on JPEG or QNN work.

The pipeline performs these bounded stages:

1. resolve the requested path through `image_path_authorizer_port`;
2. decode one owned NV12 `raw_frame` through `face_enrollment_image_source_port`;
3. run the configured detector through `face_image_detector_port`;
4. require exactly one landmark-bearing face;
5. use the immutable image buffer ID as the request-local track identity when the
   detector has no live-tracker identity;
6. align and embed it through `face_image_cascade_port`;
7. commit one template through `face_enrollment_port` and publish completed status.

Every failure transitions the request to `failed` with a stable status code. One image
produces one template; FW enrolls several photos for one subject with separate request IDs
and the current gallery revision. The recognition session enforces the configured maximum
templates per subject.

The production service composes dedicated SCRFD and EdgeFace graph owners for this path.
They reuse the catalog/package contracts and Qualcomm adapters while remaining isolated
from live camera submissions. The service starts both graphs before publishing D-Bus,
advances one pending job outside the callback, and drains/unloads both graphs on shutdown.
Allowed roots, maximum JPEG bytes, decode timeout and every GStreamer factory/engine are
required command-line deployment inputs; missing policy fails startup.

The POSIX authorizer opens the requested file with `O_NOFOLLOW`, verifies regular-file
type, byte ceiling, JPEG signature and canonical containment under configured roots, then
returns a retained `/proc/self/fd` path. The open inode remains owned through decode, so a
rename or symlink swap after authorization cannot redirect `filesrc` to another file.
