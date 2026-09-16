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
5. align and embed it through `face_image_cascade_port`;
6. commit one template through `face_enrollment_port` and publish completed status.

Every failure transitions the request to `failed` with a stable status code. One image
produces one template; FW enrolls several photos for one subject with separate request IDs
and the current gallery revision. The recognition session enforces the configured maximum
templates per subject.

The component and contract tests are delivered. Production service composition still
needs a dedicated FD graph, a configured path authorizer and a call to `step`; until that
composition is present the service continues to reject image-path requests explicitly.
