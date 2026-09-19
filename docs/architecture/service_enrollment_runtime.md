# Service enrollment runtime

The service enrollment runtime owns the optional file-enrollment control plane and the
dedicated detector/embedding graph lifecycle used by that path. It removes Qualcomm,
filesystem-security and D-Bus composition from the generation controller.

**Status:** source-delivered and logic-tested — direct owner and service characterization
tests pass under the approved eSDK; no released-FW enrollment acceptance is claimed.
**Layer:** app service composition.
**Source:** `src/app/service/enrollment/vqec_vision_service_enrollment_runtime.cpp`,
`tests/unit/application/vqec_vision_service_enrollment_runtime_test.cpp`.

## Responsibility

- Own the enrollment controller and expose only its neutral enrollment port to generation.
- Compose the allow-listed path authorizer, bounded image source, detector, cascade and the
  two dedicated graph sessions when file enrollment is enabled.
- Publish D-Bus without configuring, loading or starting either Qualcomm graph.
- Poll one bounded control/image step from the serialized service loop.
- Start both graphs only after an authorized image has been decoded into a retained frame.
- Drain and unload both graph sessions during service shutdown.

It does not own the recognition gallery/index, live-camera cascade, model catalog, source
lease or feature entitlement. Those owners outlive the runtime and are borrowed only while
configuration and polling are active.

## No-data activation rule

Creating the service process, starting backend/FW before or after it, publishing the D-Bus
object, or receiving an invalid image path must leave the offline graph sessions idle. A
valid request advances through path authorization and image decode first. Only the retained
frame transition permits graph startup. This prevents a Qualcomm DSP/HTP load from being
kept alive with no consumable input.

The live-camera primary and secondary graph lifecycles use their own first-frame gates; this
owner applies the equivalent rule to offline enrollment data.

## Failure and shutdown

Graph-start failure marks the pending request failed with the same stable status and keeps
the service loop responsive. Shutdown handles never-started sessions as a successful stop;
started sessions follow the ordinary drain/unload lifecycle. A timeout or source disconnect
is never treated as hardware completion.

## See also

- [Single-image inference](single_image_inference.md)
- [Face enrollment image pipeline](face_enrollment_image_pipeline.md)
- [Cascade graph session](cascade_graph_session.md)
- [Runtime executor](runtime_executor.md)
