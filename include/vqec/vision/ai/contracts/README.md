# contracts

Neutral public C++17 descriptors and interfaces. No vendor, GStreamer or OpenCV types cross
this directory.

- **Status:** source-delivered contract surface — built with the eSDK configuration
- **Layer:** contracts
- **Naming registry:** `cntr`
- **Used by:** `src/core/` validation and every adapter

## Responsibility

- Describe frames, inference/source plans, submission ledger, tensor results/output metadata
  and pure output policy.
- Define overlay/encoded-AU contracts, the encoder-backend lifecycle port, observation
  batches and bounded feature events with source/frame/config/schema/model provenance.
- Define multi-source deployment/model-catalog contracts and inference execution
  capability/policy/domain/shared-buffer/model-update descriptors.
- Define the version 1 D01–D18 metadata envelope and Q01–Q30 typed request/page contract,
  including explicit value state, authorization revision, scope and completeness.
- Use the machine-readable three-team registry for external C01–C10 ownership; this directory
  remains the neutral C++ surface and does not absorb FW/vendor types.

## Contents

| Path | Purpose |
|---|---|
| `base/` | Status, identifier and canonical LACAI version primitives used by every layer. |
| `lifecycle/` | Application lifecycle, deployment and top-level composition descriptors. |
| `inference/` | Model, tensor, decoder, execution, submission and source-binding contracts. |
| `media/` | Frame, preview, color, overlay/encode and FW ring descriptors. |
| `perception/` | Observation, embedding, recognition and face-gallery value types. |
| `features/` | Feature catalog, usecase activation and bounded feature-event contracts. |
| `output/` | Authorization, evidence, metadata, trajectory and output-generation contracts. |

The domain directory is part of the public include path. New contracts are placed in exactly
one domain; dependencies point toward `base/` and never toward adapters or application owners.

## Limits and next work

- Decoder output may use zero track IDs; tracked/feature input requires nonzero IDs.
- Concrete hardware encoder and feature algorithms remain pending.
- Metadata retention execution, asynchronous query jobs and network export remain outside the
  neutral contract delivered here.
- Declaration method names follow the logical owner rule in the naming registry, independently
  of `vqec_vision_` filenames.

## See also

- [Ports](../ports/README.md), [naming registry](../../../../../docs/development/naming_registry.md)
- [Three-team integration registry](../../../../../docs/contracts/integration_contract_registry.md)
- [Metadata query foundation](../../../../../docs/architecture/metadata_query.md)
