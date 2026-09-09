# Perception stage factory

`perception_stage_factory` is the activation-time owner constructor for one
`(source, model)` perception chain. It verifies that the deployment source assigns the
model, resolves the model's decoder contract, creates a distinct tracker through the
selected tracker contract, then configures the decoder, tracker and result stages with
the source camera/channel and geometry.

The returned `perception_stage_bundle` owns the tracker and all coordinating stages in
destruction-safe order. The decoder is borrowed from `model_decoder_registry`, so its
implementation and registry must outlive the bundle. Failed resolution, validation,
allocation or stage configuration leaves the caller's previous bundle unchanged.

The tracker contract is an authenticated activation input rather than a hardcoded model
or platform choice. This factory does not load model artifacts, authenticate catalogs,
choose algorithms, or expose Qualcomm/GStreamer types. A concrete model package only
needs to register its decoder and tracker factory, then supply the selected contracts to
composition.

`source_perception_factory` groups 1..16 bundles and their result router in source
model order. Activation identities must match every deployment model slot; a failed
slot destroys the candidate group and preserves the previous caller bundle. Catalog
validation is performed here, while deployment authentication, source-envelope and
output-manifest validation remain caller prerequisites. All borrowed decoders must
outlive the group and share an appropriate serialized executor. This source does not
yet establish decoder output-manifest compatibility during bundle construction.
