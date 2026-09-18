# Single-image inference

`single_image_inference` runs one already-owned `raw_frame` through the same neutral
`image_processor_port`, `inference_graph_port` and `model_decoder_port` used by live
sources. This document defines its graph-lifecycle preconditions and its synchronous
result handling.

**Status:** source-delivered — the runner source exists in the tree. **Layer:** app.
**Source:** `src/app/cascade/vqec_vision_single_image_inference.cpp`,
`tests/unit/application/vqec_vision_single_image_inference_test.cpp`.

## Responsibility

- Runs one already-owned `raw_frame` through the neutral processor/graph/decoder ports.
- Resolves the single fixed input once and keeps one bounded tensor buffer.
- Publishes decoded observations only after full identity and geometry validation.
- Must not introduce a model-name branch or a vendor type.
- Must not share a live graph without external serialization.
- Must not authorize filesystem paths or mutate the recognition gallery.

## Execution

The caller owns the graph lifecycle and provides an already running dedicated graph.
The runner resolves the single fixed input once, keeps one bounded tensor buffer, arms by
source epoch, preprocesses, submits, polls the synchronous result and publishes decoded
observations only after full identity and geometry validation. Current production QNN is
synchronous. An asynchronous backend needs a future explicit progress state instead of
being silently treated as synchronous.

## Enrollment role

For file enrollment this runner is the FD stage between the authorized image source and
the cascade alignment/embedding stage. It does not authorize filesystem paths or mutate
the recognition gallery.

## Limits and next work

- Asynchronous backends need a future explicit progress state instead of being silently
  treated as synchronous.
- The runner does not authorize filesystem paths or mutate the recognition gallery.
- Sharing a live graph without external serialization is invalid.

## See also

- [Cascade inference](cascade_inference.md)
- [Face enrollment image pipeline](face_enrollment_image_pipeline.md)
- [Vendor-neutral inference-graph port](inference_graph_port.md)
