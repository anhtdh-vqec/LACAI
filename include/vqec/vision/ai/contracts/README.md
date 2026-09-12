# contracts

Neutral public C++17 descriptors and interfaces. No vendor, GStreamer or OpenCV types cross
this directory.

- **Status:** source-delivered contract surface — built with the eSDK configuration
- **Naming registry:** `cntr`
- **Used by:** `src/core/` validation and every adapter

## Responsibility

- Describe frames, inference/source plans, submission ledger, tensor results/output metadata
  and pure output policy.
- Define overlay/encoded-AU contracts, the encoder-backend lifecycle port, observation
  batches and bounded feature events with source/frame/config/schema/model provenance.
- Define multi-source deployment/model-catalog contracts and inference execution
  capability/policy/domain/shared-buffer/model-update descriptors.

## Limits and next work

- Decoder output may use zero track IDs; tracked/feature input requires nonzero IDs.
- Concrete hardware encoder and feature algorithms remain pending.
- Declaration method names follow the logical owner rule in the naming registry, independently
  of `vqec_vision_` filenames.

## See also

- [Ports](../ports/README.md), [naming registry](../../../../../docs/development/naming_registry.md)
