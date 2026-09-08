# model_registry

model_catalog now loads the bounded Model-team JSON v1 and validates execution identity,
input/preprocess, source envelope, cadence, concurrency and memory declarations. Neutral
core cross-validation matches it against the 1..16-source deployment and can compose the
current single-image plan only with a trusted resolver result. It still does not
authenticate artifacts or execute a graph. See docs/architecture/model_catalog.md.

output_manifest loads bounded JSON v1 identity/digest/decoder/output metadata, with
duplicate/unknown-key, type, depth and payload checks. It is NOT a verified registry:
no signature, target qualification, decoder lookup or load lifecycle.
Optional target uses nlohmann_json 3.12.0 from the build environment, no downloads.
See docs/architecture/model_output_manifest.md. No model binaries are stored here.

artifact_digest now compares bounded stream bytes against a SHA-256 digest using
OpenSSL Crypto, optionally built separately from JSON parsing. A match is not
authentication or a safe model-path loading handle. See docs/architecture/artifact_digest.md.
