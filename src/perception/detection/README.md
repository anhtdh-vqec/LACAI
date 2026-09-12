# detection

Neutral model-decoding boundary from owned tensor results to tracked observations. Keeps
model algorithms and vendor types out of `src/core` and `src/app`.

- **Status:** source-delivered contract + registry — concrete detector decoders missing
- **Naming registry:** `detec` (`mdstg`, `mdreg`, `tnrd`)
- **Depends on:** `model_decoder_port`, output-manifest identity, neutral observation contract
- **Used by:** `src/app/perception_result_stage` and `perception_stage_factory`

## Responsibility

- Validate catalog/output identity and decode one owned tensor result into a bounded `observation_batch`.
- Publish a decoded batch atomically; a decoder error or invalid observation never overwrites
  the previous published batch.
- Bind a catalog `decoder_contract` to a non-owning decoder at activation and reject unknown contracts.
- Provide bounded tensor lookup and manifest shape/value-count checks for decoders.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_model_decode_stage.cpp` | Transactional decode: temporary batch, identity/geometry/limit checks, atomic publish |
| `vqec_vision_model_decoder_registry.cpp` | Bounded contract-to-decoder mapping; validates output-manifest identity |
| `vqec_vision_tensor_reader.cpp` | Bounded name lookup and manifest shape/value-count validation |

## Limits and next work

- Concrete detector geometry/NMS semantics and model implementations remain unimplemented.
- Registry activation validation is validation-only and must not mutate live decoder state.
- Decoder exceptions are contained (`resource_exhausted`/`io_error`) without changing registrations.

## See also

- [Perception result stage](../../../docs/architecture/perception_result_stage.md)
- [Model output manifest](../../../docs/architecture/model_output_manifest.md)
- [Tensor output](../../../docs/architecture/tensor_output.md)
