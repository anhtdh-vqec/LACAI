# model_registry

Bounded model/artifact metadata loaders and neutral cross-validation. Never a signature
authority or an execution executor.

- **Status:** source-delivered optional catalog/package loaders (JSON + SHA-256) — trusted resolution still open
- **Layer:** runtime
- **Naming registry:** `mreg` (`mdcat`, `otman`, `ardgt`, `artsr`)
- **Depends on:** `include/vqec/vision/ai/contracts/` model/output contracts
- **Used by:** runtime composition factory and the optional `vqec_vision_ai_manifest_check` tool

## Responsibility

- Load bounded Model-team catalog JSON and validate execution identity, input/preprocess,
  source envelope, cadence, concurrency and memory declarations.
- Load the strict model decoder package (`decoder.json`) for YOLO/anchor-distance packages.
- Cross-validate the catalog against the 1..16-source deployment and compose a plan only
  with a trusted resolver result.
- Load bounded output-manifest identity/digest/decoder metadata.
- Compare bounded stream bytes against a SHA-256 digest.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_decoder_package.cpp` | Strict per-model decoder package loader (all fields required, unknown keys rejected) |
| `vqec_vision_model_catalog.cpp` | Bounded catalog loader + neutral cross-validation |
| `vqec_vision_output_manifest.cpp` | Bounded output-manifest loader with duplicate/unknown-key, type and depth checks |
| `vqec_vision_artifact_digest.cpp` | OpenSSL SHA-256 stream comparison, optional from JSON parsing |
| `vqec_vision_artifact_resolver.cpp` | Trusted artifact path resolution with containment and digest |
| `vqec_vision_model_package_registry.cpp` | Strict per-model package/artifact deployment binding loader |

## Limits and next work

- No signature verification, target qualification, decoder lookup or load lifecycle.
- A digest match is not authentication and not a safe model-path loading handle.
- No model binaries are stored here.

## See also

- [Model catalog](../../../docs/architecture/model_catalog.md), [model output manifest](../../../docs/architecture/model_output_manifest.md)
- [Model package registry](../../../docs/architecture/model_package_registry.md)
- [Artifact digest](../../../docs/architecture/artifact_digest.md)
