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

`model_decoder_registry` binds the catalog's immutable `decoder_contract` to a
non-owning decoder implementation during activation. It has bounded capacity,
rejects duplicate/invalid contracts and returns `unsupported` for an unknown
contract; it never constructs a decoder or silently selects a fallback.
It can also validate a parsed output manifest against the selected catalog identity
and invoke the registered decoder's own manifest validation before activation.

`tensor_reader` provides bounded name lookup and manifest shape/value-count checks for
decoder implementations. It performs no model-specific postprocess and does not expose
Qualcomm or GStreamer types.
