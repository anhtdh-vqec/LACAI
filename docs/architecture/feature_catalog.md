# Feature catalog — 13 bài tối thiểu và extensions

Status: the generic feature integration catalog contract, pure validator and synthetic
schema/example are source-delivered and cross-compiled with the AArch64 eSDK. Every
concrete feature remains unimplemented and unqualified; target tests are not executed on
the x86 host.
Feature ID ổn định độc lập model ID/package. Dependencies chỉ share khi compatible.

## Integration catalog boundary

`feature_catalog` is the authenticated-package projection used before constructing a
specific usecase. It identifies each feature version, processor contract, configuration
schema, named model roles, exact attribute schema/version freshness requirements and
bounded temporal/event resources. The root pins the model catalog identity, and the pure
cross-validator rejects missing model dependencies.

`single_model` requires exactly one model role and can use the current per-model feature
pipeline. `temporal_join` requires at least two distinct model roles and declares that a
separate bounded source/frame join is necessary. The catalog never treats adjacent
round-robin results as synchronized.

The catalog does not express entitlement, desired state, per-source configuration values
or artifact paths. A synthetic example enables no feature. A later trusted activation
document binds source IDs and feature IDs, resolves feature-specific configuration and
checks accumulated temporal resources before owner construction.

Concrete compiled-in packages register a factory for `processor_contract`; see the
[feature processor registry](feature_processor_registry.md). The factory validates the
feature-specific configuration payload identified by `configuration_schema` and returns
an independent stateful processor owner.

The optional `feature_catalog` JSON loader applies the same bounded 512 KiB/depth-16,
duplicate-key and unknown-key checks as the deployment/model loaders before invoking the
core validator. Parsing is a metadata step: it does not authenticate the package, payload,
entitlement or artifact.

| # | Feature ID / bài | Compute + state cần có | Output / nghiệm thu chính |
|---|---|---|---|
| 1 | abnormal_behavior / ẩu đả, xung đột | person + pose/action temporal, multi-person context | alarm episode; precision/recall + false alarms/hour + onset delay |
| 2 | crowd_gathering / tụ tập | person tracking + zone density/dwell/group rules | crowd event; crowd recall, false alarms, dwell timing |
| 3 | smoking / hút thuốc | person/hand/face/object hoặc action model; ROI quality + temporal confirmation | smoking event; tiny-object visibility + hard negatives + false alarms |
| 4 | intrusion / xâm nhập | person/object tracks + polygon/line rules | entry/exit/intrusion event; geometry + duplicate suppression |
| 5 | abandoned_object / đồ bỏ quên | object track + stationary timer + person-object relation, occlusion | abandonment event; owner absence ambiguity + reset correctness |
| 6 | person_tracking / theo dõi người | detector + MOT, optional ReID | tracks; IDF1/HOTA hoặc metric thống nhất, ID switches |
| 7 | retrieval / tìm người/đồ thất lạc | attributes/embedding + track entity refs + external search index | search records/results; retrieval recall@k + latency + authorization |
| 8 | ppe / mũ, áo phản quang | person/PPE detection/classification + body association | PPE violation; per-class quality, missing vs not observable |
| 9 | suspicious_object / vũ khí | dedicated detector + person association + temporal policy | suspicious-object event; false positives/hour + minimum pixels |
| 10 | luggage_tracking / hành lý, xe đẩy | object detection/MOT + person-object relation | tracks/relations; continuity under occlusion |
| 11 | blacklist / người blacklist | face detect/align/quality/FR + versioned gallery | match candidate/alarm; FAR/FRR at agreed threshold + privacy |
| 12 | heatmap / mật độ theo thời gian | person positions/tracks + dwell grid + time buckets | heatmap; exposure, occupancy/dwell accuracy + gap handling |
| 13 | counting / vào-ra khu vực | person tracks + directional lines/polygons + hysteresis | windowed counts; count error, direction, duplicate/restart correctness |

Không hứa task1/3/9 hoàn chỉnh chỉ vì có person detector.
Abandoned object không thể luôn suy ra owner từ khoảng cách gần nhất.
Blacklist cần gallery service + enrollment/model-version workflow; không chỉ
cosine similarity một frame. Retrieval full backend thuộc FW software.

## Human attribute framework

attribute_type + schema_version, entity/track ref, typed value/candidates,
confidence, quality, observation time, freshness/expiry, model_version,
known/unknown/not_observable, provenance.
Mở rộng shirt/pants color/type, bag/accessory, PPE, face quality, estimated age/
gender và FR bằng registry, không thêm một field cố định mỗi model trong track.

FR identity candidate tách visual attributes và local track id. Estimated age/
gender không xác nhận danh tính. Không suy luận nhạy cảm ngoài product scope.
Per-attribute entitlement/privacy; purge/retain policy được FW chốt.
Temporal fusion theo quality/cooldown, tránh giữ nhãn sai suốt track.

## Traffic readiness

Entity category mở person/vehicle/baggage/cart; relations include carries,
near, associated_with, vehicle_has_plate; ontology version explicit.
Reusable detection/tracking/counting/heatmap; thêm vehicle attributes, plate OCR,
lane/polygon direction, calibration/time contracts.

Traffic triển khai theo thứ tự: vehicle count/class -> lane/direction -> plate OCR
association -> calibrated speed -> red-light/crossing event nếu có signal time.
Không suy ra tốc độ pháp lý từ pixel displacement; cần calibration, clocks,
accuracy và validation phù hợp product. Red-light cần authoritative signal input.
Chưa cam kết traffic full production trong 12 tuần đầu.

## Release workload profiles

- W1: person tracking + intrusion + counting + heatmap + gathering.
- W2: W1 + PPE hoặc luggage/abandoned, tùy resource budget.
- W3: W1 + FR/blacklist/attrs có ROI rate + gallery quota.
- W4: temporal action/smoking/weapon profile với temporal model constraints.
Mỗi profile pin board/image/models/cameras/FPS/ROI limits/KPIs.
Đây là candidates để benchmark, không phải các tổ hợp đã đạt hiệu năng.
