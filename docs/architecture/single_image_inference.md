# Single-image inference

`single_image_inference` runs one already-owned `raw_frame` through the same neutral
`image_processor_port`, `inference_graph_port` and `model_decoder_port` used by live
sources. It introduces no model-name branch and no vendor type. The caller owns the graph
lifecycle and provides an already running dedicated graph; sharing a live graph without
external serialization is invalid.

The runner resolves the single fixed input once, keeps one bounded tensor buffer, arms by
source epoch, preprocesses, submits, polls the synchronous result and publishes decoded
observations only after full identity and geometry validation. Current production QNN is
synchronous. An asynchronous backend needs a future explicit progress state instead of
being silently treated as synchronous.

For file enrollment this runner is the FD stage between the authorized image source and
the cascade alignment/embedding stage. It does not authorize filesystem paths or mutate
the recognition gallery.
