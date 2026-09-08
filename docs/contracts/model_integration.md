# AI Model Integration Package — proposal v1

Implementation update: ordered packed FLOAT32 output metadata now has a shared pure
validator, tensor_contract, called by camera_session before StartStream and by the
Qualcomm graph before PLAYING. Limits: 1..16 unique nonempty names (<=128 bytes,
no embedded NUL), rank 1..8, positive dimensions <=INT32_MAX, total payload within
an explicit budget <=64 MiB. Dimension multiplication is checked before arithmetic.
Order is preserved, never sorted or inferred from model names. This is the current
plugin output subset, not a universal model schema or support for mixed/native dtype.
Failure leaves the computed-byte output unchanged and does not acquire Camera resources.
It validates supplied metadata, not agreement with artifact tensors, hashes/signatures,
golden accuracy, decoder semantics or entitlement. The output-only JSON loader and
read-only checker now exist; full model-kit loading/authentication remain unimplemented.
See [output manifest contract](../architecture/model_output_manifest.md).
Bounded SHA-256 comparison is available separately; read the
[artifact integrity boundary](../architecture/artifact_digest.md) before connecting it
to load. A matching digest is not a signature or protection against path replacement.

Owner: AI Model; integration + acceptance: AI APP; target compatibility: BSP.
Không nhận binary đơn lẻ làm deliverable hoàn tất.

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

| Nhóm | Nội dung bắt buộc |
|---|---|
| Identity | model_id, version, task, ontology_version, checksum, license |
| Target | SoC, accelerator, compiler/export version, runtime/SDK compatibility |
| Artifact | bin/so/other, graph_name, input/output tensor names, sizes |
| Inputs | rank, dims, dynamic ranges nếu support, layout, dtype, strides/packing |
| Preprocess | source color/range, crop policy, resize algorithm, coordinate convention, interpolation, rotation, pad value, normalization order |
| Quantization | per tensor/per channel scale + offset/zero-point convention, rounding, saturation |
| Outputs | mỗi tensor riêng dtype/layout/shape/name/quantization; raw sample |
| Decode | activation, box encoding, anchors/strides nếu có, NMS/threshold/topK, coordinate map |
| Temporal | sampling FPS, window, warmup, missing-frame policy, gap reset, recurrent state |
| Quality | dataset conditions, min object pixels, distance/angle/light/occlusion, unknown thresholds |
| Performance | tested board/SDK, latency percentiles, memory, batch/concurrency and conditions |
| Golden | raw input, expected preprocess tensor, raw outputs, decoded outputs, tolerances |
| Release | changelog, known limitations, rollback compatibility, artifact hashes |

Normalization viết công thức và thứ tự, không chỉ mean/std mơ hồ.
Phân biệt pixel -> real model value -> quantized integer. SDK quantization offset
phải map theo SDK definition, không tự coi mọi offset là -zero_point.
Golden fixture nhỏ có checksum; video lớn ở artifact storage, không commit Git.

## Contract bổ sung theo task

- Detection/PPE/weapons/smoking: class dictionary, box association/body parts,
  min pixels, hard negatives, suppression semantics; detector != completed event.
- Pose/action: keypoint names/connections/confidence, person association, sequence
  FPS/window, multi-person context, state reset, action decision aggregation.
- FR: face landmarks, alignment template/warp, image quality, embedding dimension,
  normalization, similarity metric, threshold calibration, gallery model version.
  Không so embedding khác model version nếu chưa migration/re-enrollment.
- ReID: camera/domain conditions, metric, retrieval quality, incompatible versions;
  không gọi ReID là xác thực danh tính.
- Human attrs: attribute schema/version, value domain, multi-label/exclusive,
  confidence/unknown, observation quality, temporal fusion and expiry.
  Tuổi/giới tính là ước lượng, không dữ kiện danh tính đã xác minh.
- OCR/ANPR: detector crop/rectification, alphabet/token map, blank index, decoder,
  orientation, confidence/format policy, plate-to-vehicle relation.
- Traffic speed: model output không tự đủ; calibration/time contract riêng.

## Gate acceptance

M0 completeness -> M1 load and metadata -> M2 golden preprocess ->
M3 raw outputs tolerance -> M4 decoded geometry/labels ->
M5 replay feature quality -> M6 mixed-load board performance -> released.

AI Model chịu chất lượng model; AI APP chịu integration parity/feature rules;
hai bên cùng chịu acceptance end-to-end. mAP tốt không thay counting accuracy,
track ID continuity hoặc false alarms/hour.

Mỗi feature cần agreed dataset, metric/threshold, privacy-safe samples,
known failures và signed report. Nếu model kit muộn/không đạt, feature trạng thái
blocked/not_qualified; không lấy demo video thuận lợi thay regression dataset.

## Version changes

Đổi preprocess, ontology, tensor layout hoặc embedding semantics = integration
change, phải rerun golden + downstream tests; không chỉ thay checksum binary.
Manifest pin dependencies, rollback cả decoder/config khi không tương thích.
Model artifacts từ verified storage, xác thực provenance trước load executable .so.
