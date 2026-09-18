# AI Model Integration Package — proposal v1

This document defines the package, catalog boundary, per-task contract and gate acceptance
required for an AI model to be integrated into LACAI. It is the AI Model team deliverable
contract; integration and acceptance belong to AI APP, with BSP as the target-compatibility
owner.

**Status:** source-delivered — the shared `tensor_contract` validator, the output-manifest
loader/checker and bounded SHA-256 comparison exist; full model-kit loading/authentication
remain unimplemented. **Layer:** contracts. **Source:** `n/a`.

Owner: AI Model; integration + acceptance: AI APP; target compatibility: BSP.

C03 in the [three-team integration registry](integration_contract_registry.md) is the normative
owner/version/bounds envelope. A package that omits any C03 required field is incompatible before
artifact load, regardless of whether its binary happens to execute.

## Responsibility

- Defines the required model package fields, resource envelope and acceptance gates.
- Keeps model IDs stable integration identities referenced through the feature catalog.
- Must not accept a standalone binary as a complete deliverable, and must not treat
  metadata validation as artifact authenticity or accuracy.

## Ordered packed output metadata

Ordered packed output metadata now has a shared pure validator, `tensor_contract`, called by
`camera_session` before `StartStream` and by the Qualcomm graph before `PLAYING`. Each
tensor declares one of int8, uint8, int16, uint16, int32, uint32, int64, uint64, float16,
float32 plus optional integer quantization
(`real = (stored - zero_point) * scale`); floating tensors must not be quantized. Limits:
1..16 unique nonempty names (<=128 bytes, no embedded NUL), rank 1..8, positive dimensions
<=INT32_MAX, total payload within an explicit budget <=64 MiB, sized by element dtype.
Dimension multiplication is checked before arithmetic. Order is preserved, never sorted or
inferred from model names. The adapter carries every reviewed element type, but a backend
must negotiate the matching caps type: the installed Qualcomm plugin reports FLOAT32
outputs, and genuine per-tensor mixed dtype is rejected by the single-type caps. This is
not a universal model schema or proof of native multi-dtype execution. Failure leaves the
computed-byte output unchanged and does not acquire Camera resources. It validates supplied
metadata, not agreement with artifact tensors, hashes/signatures, golden accuracy, decoder
semantics or entitlement. The output-only JSON loader and read-only checker now exist; full
model-kit loading/authentication remain unimplemented.
See [output manifest contract](../architecture/model_output_manifest.md).
Bounded SHA-256 comparison is available separately; read the
[artifact integrity boundary](../architecture/artifact_digest.md) before connecting it
to load. A matching digest is not a signature or protection against path replacement.

The neutral `model_decoder_port` is now the required AI APP boundary between tensor
results and observations. A decoder must validate the supplied model output identity,
preserve the expected source frame key, and write observations transactionally. It must
not infer geometry, labels, quantization or model version from an untrusted tensor alone.
Do not accept a standalone binary as a complete deliverable.

Feature packages reference model entries through the versioned
[feature catalog](../architecture/feature_catalog.md). Model IDs therefore remain stable
integration identities; feature source does not select a graph from a product name or
hardcoded feature table.

## Catalog boundary for multi-source deployment

AI Model delivers one authenticated catalog/package that may contain several model
entries. The deployment configuration references the catalog by `model_catalog_ref`
and assigns `model_ids` to each of 1..16 sources. The catalog is authoritative for
artifact/preprocess/tensor/decode/cadence constraints; deployment is authoritative for
source identity, exact effective source profile and memory ceilings.

Each model entry must additionally provide bounded resource requirements per supported
target/profile: artifact/context resident bytes, input/output pool bytes, minimum model
cadence, warmup, maximum objects/ROIs, and whether contexts/buffers are shareable across
sources or threads. “Supports 4K” is insufficient: the model consumes a declared tensor
shape through an approved source-to-tensor transform, and every source profile/config
combination is cross-validated before resources or Camera leases are acquired.

The model file must not contain FW transport origin/credentials, RAW-source references,
FW ring names, customer entitlement or camera ownership. AI APP must not copy source
dimensions into the model package as a second
authority. The source-delivered schema and validator are described in
[model catalog](../architecture/model_catalog.md) and
[multi-source configuration](../architecture/multi_source_configuration.md).

## Package required

| Group | Required content |
|---|---|
| Identity | model_id, version, task, ontology_version, checksum, license |
| Target | SoC, accelerator, compiler/export version, runtime/SDK compatibility |
| Artifact | bin/so/other, graph_name, input/output tensor names, sizes |
| Inputs | rank, dims, dynamic ranges if supported, layout, dtype, strides/packing |
| Preprocess | source color/range, crop policy, resize algorithm, coordinate convention, interpolation, rotation, pad value, normalization order |
| Quantization | per tensor/per channel scale + offset/zero-point convention, rounding, saturation |
| Outputs | each tensor's dtype/layout/shape/name/quantization; raw sample |
| Decode | activation, box encoding, anchors/strides if any, NMS/threshold/topK, coordinate map |
| Temporal | sampling FPS, window, warmup, missing-frame policy, gap reset, recurrent state |
| Quality | dataset conditions, min object pixels, distance/angle/light/occlusion, unknown thresholds |
| Performance | tested board/SDK, latency percentiles, memory, batch/concurrency and conditions |
| Golden | raw input, expected preprocess tensor, raw outputs, decoded outputs, tolerances |
| Release | changelog, known limitations, rollback compatibility, artifact hashes |

Normalization must state the formula and order, not just an ambiguous mean/std.
Distinguish pixel -> real model value -> quantized integer. The SDK quantization offset
must map per the SDK definition; do not treat every offset as -zero_point.
A small golden fixture carries a checksum; large videos live in artifact storage and are
not committed to Git.

## Additional per-task contract

- Detection/PPE/weapons/smoking: class dictionary, box association/body parts,
  min pixels, hard negatives, suppression semantics; detector != completed event.
- Pose/action: keypoint names/connections/confidence, person association, sequence
  FPS/window, multi-person context, state reset, action decision aggregation.
- FR: face landmarks, alignment template/warp, image quality, embedding dimension,
  normalization, similarity metric, threshold calibration, gallery model version.
  Do not compare embeddings from different model versions without migration/re-enrollment.
- ReID: camera/domain conditions, metric, retrieval quality, incompatible versions;
  do not call ReID identity authentication.
- Human attrs: attribute schema/version, value domain, multi-label/exclusive,
  confidence/unknown, observation quality, temporal fusion and expiry.
  Age/gender are estimates, not verified identity facts.
- OCR/ANPR: detector crop/rectification, alphabet/token map, blank index, decoder,
  orientation, confidence/format policy, plate-to-vehicle relation.
- Traffic speed: model output is not sufficient by itself; a separate calibration/time
  contract is required.

## Gate acceptance

M0 completeness -> M1 load and metadata -> M2 golden preprocess ->
M3 raw outputs tolerance -> M4 decoded geometry/labels ->
M5 replay feature quality -> M6 mixed-load board performance -> released.

AI Model owns model quality; AI APP owns integration parity/feature rules;
both share end-to-end acceptance. Good mAP does not replace counting accuracy,
track ID continuity or false alarms/hour.

Each feature needs an agreed dataset, metric/threshold, privacy-safe samples,
known failures and a signed report. If a model kit is late/does not pass, the feature status
is blocked/not_qualified; do not substitute a favorable demo video for a regression dataset.

## Version changes

Changing preprocess, ontology, tensor layout or embedding semantics is an integration
change and requires rerunning golden + downstream tests; do not just replace the binary
checksum. The manifest pins dependencies and rolls back both decoder/config when
incompatible. Model artifacts come from verified storage; validate provenance before
loading an executable .so.

## Limits and next work

- Full model-kit loading/authentication remains unimplemented; the delivered validator
  checks supplied metadata only.
- M2 preprocess golden and M4 decoded golden still need the model team's reference
  tensor/detections; a matching digest is not a signature or path-replacement protection.

## See also

- [Output manifest contract](../architecture/model_output_manifest.md)
- [Artifact integrity boundary](../architecture/artifact_digest.md)
- [Feature catalog](../architecture/feature_catalog.md)
- [Model catalog](../architecture/model_catalog.md)
- [Multi-source configuration](../architecture/multi_source_configuration.md)
- [Three-team integration contract registry](integration_contract_registry.md)
