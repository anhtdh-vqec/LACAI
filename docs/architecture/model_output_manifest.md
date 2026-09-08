# Model output manifest v1

Source candidate; not a verified model registry or complete Model Integration Package.
Root fields, all required: schema_version=1, model_id, model_version, artifact_sha256,
decoder_contract, max_output_bytes, outputs. Each output contains exactly name, dtype
(float32), shape. Order is preserved. No unknown or duplicate object keys are allowed.
Identity/decoder strings are 1..128 ASCII letters/digits/dot/colon/underscore/hyphen.
Digest is 64 lowercase hex characters. Names/shapes/budget follow tensor_contract.
Numeric fields must be unsigned JSON integers, never strings, booleans or floats.

Loader consumes a caller-opened stream (at most 65537 bytes), with a 64 KiB document
limit and depth <=16. Stream may block: use during startup only. Parser DOM overhead
is larger than document size. Failure leaves output unchanged; allocation failures
propagate. EOF exceptions on an otherwise valid stream are accepted. Stream ownership,
path authorization and file opening belong to the caller.

The optional CMake target requires locally installed nlohmann_json 3.12.0 EXACT;
no implicit downloads, Camera or vendor dependencies. JSON parsing uses the documented
[parse API](https://json.nlohmann.me/api/basic_json/parse/) and
[parser callback](https://json.nlohmann.me/api/basic_json/parser_callback_t/).
Pin changes require dependency review; no third-party source copied into this repo.

Loading does not hash files, verify signatures, authorize features, or resolve decoders.
The separate [artifact digest helper](artifact_digest.md) now performs bounded byte
hash comparison. It does not close the immutable-artifact/path association requirement.
Before session configuration, the application must match identity/hash/decoder to an
authenticated model kit and verify its target/preprocess contract. No artifact paths
are accepted here. Example metadata is synthetic, never a deployable model release.
Mixed dtype, dynamic output shapes and native quantized outputs remain unsupported.

The output document is referenced by, but deliberately not embedded in, the
[model catalog](model_catalog.md). Before activation, runtime must match model identity,
version, artifact digest, decoder contract and output reference across the authenticated
catalog/package. The current sources validate each document and deployment assignments;
the authenticated multi-document resolver remains pending.

camera_session exposes bind_model_outputs(manifest, selection, config) to correlate
the parsed identity/hash/decoder against independently supplied deployment selection,
revalidate outputs and transactionally set session outputs/budget. Call before session
construction; no active graph mutation. It does not validate the plan model_path against
the artifact or authenticate either input. The caller must verify that association.
Never construct the expected selection by blindly copying the untrusted manifest.

## Read-only handoff checker

Enable VQEC_VISION_AI_ENABLE_MODEL_MANIFEST and VQEC_VISION_AI_BUILD_MANIFEST_CHECK
with the pinned package available locally; no Qualcomm SDK is needed. Then run
`vqec_vision_ai_manifest_check path/to/output-manifest.json`.
Exit 0: metadata structurally valid; 1: malformed/unsupported manifest; 2: usage/I/O
or memory failure. No files are changed and no model libraries are loaded. The tool
prints the distinction between metadata validation and artifact qualification.
