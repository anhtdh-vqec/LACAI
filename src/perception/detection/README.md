# detection

`model_decoder_port` now defines the neutral boundary for model-specific tensor decoding.
It validates the catalog/output identity and decodes one owned tensor result into a bounded
`observation_batch` tied to the expected frame key. Qualcomm/GStreamer types and model
algorithms stay outside this contract. Concrete detector decoders, geometry/NMS semantics,
tracking and feature integration remain unimplemented.

`model_decode_stage` is the first portable consumer of that boundary. It decodes into a
temporary batch, validates frame identity/geometry and observation limits, then publishes
the batch atomically. A decoder error or invalid observation never overwrites the previous
published batch.
