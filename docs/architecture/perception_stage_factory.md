# Perception stage factory

`perception_stage_factory` is the activation-time owner constructor for one
`(source, model)` perception chain, and `source_perception_factory` groups 1..16 of those
bundles. Together they resolve decoders/trackers and build a destruction-safe, serialized
perception group without exposing vendor types.

**Status:** source-delivered — activation-time perception chain and source group
constructors exist. **Layer:** app.
**Source:** `src/app/vqec_vision_perception_stage_factory.{hpp,cpp}`,
`src/app/vqec_vision_source_perception_factory.{hpp,cpp}`,
`src/perception/detection/vqec_vision_model_decoder_registry.{hpp,cpp}`.

## Responsibility

- Verify that the deployment source assigns the model, bind the resolved output-manifest
  reference and parsed tensor metadata to the catalog entry, and validate that metadata
  through the registered decoder.
- Create a distinct tracker through the selected tracker contract and configure the
  decoder, tracker and result stages with the source camera/channel and geometry.
- Must not load model artifacts, authenticate catalogs, choose algorithms, or expose
  Qualcomm/GStreamer types.

## Per-source/model chain

The factory verifies that the deployment source assigns the model, binds the resolved
output-manifest reference and parsed tensor metadata to the catalog entry, asks the
registered decoder to validate that metadata, creates a distinct tracker through the
selected tracker contract, then configures the decoder, tracker and result stages with the
source camera/channel and geometry.

The returned `perception_stage_bundle` owns the tracker and all coordinating stages in
destruction-safe order. The decoder is borrowed from `model_decoder_registry`, so its
implementation and registry must outlive the bundle. Failed resolution, validation,
allocation or stage configuration leaves the caller's previous bundle unchanged.

The tracker contract is an authenticated activation input rather than a hardcoded model
or platform choice. The output metadata remains parsed, unauthenticated data; the caller
must obtain the manifest reference from its authenticated model package and must verify
the artifact before composition. This factory checks exact model/version/digest/decoder
identity, manifest reference, tensor bounds and decoder-specific schema. A concrete model
package registers its decoder and tracker factory, then supplies the selected contracts and
bound output metadata to composition.

## Source perception group

`source_perception_factory` groups 1..16 bundles and their result router in source
model order. Activation identities must match every deployment model slot; a failed
slot destroys the candidate group and preserves the previous caller bundle. Catalog
validation and output-manifest/decoder compatibility validation are performed before
each tracker is created. All borrowed decoders must outlive the group and share an
appropriate serialized executor.

## Limits and next work

- Deployment authentication, full source-envelope and aggregate source-budget admission
  remain caller prerequisites.
- The factory treats output metadata as parsed, unauthenticated data; artifact verification
  must happen before composition via the authenticated model package.
- The factory does not load model artifacts, authenticate catalogs, choose algorithms, or
  expose Qualcomm/GStreamer types.

## See also

- [perception result stage](perception_result_stage.md)
- [tracker registry](tracker_registry.md)
- [tracking stage](tracking_stage.md)
- [model output manifest](model_output_manifest.md)
