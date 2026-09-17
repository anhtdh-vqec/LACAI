# Feature catalog — 13 minimum features and extensions

The feature catalog is the authenticated-package projection that declares each feature
version, processor contract, configuration schema, model roles, attribute freshness and
bounded temporal resources before a usecase is constructed. This document defines that
catalog boundary and the candidate feature set.

**Status:** source-delivered — the generic feature integration catalog contract, pure
validator and synthetic schema/example are source-delivered and cross-compiled with the
AArch64 eSDK. Every concrete feature remains unimplemented and unqualified. The generic
catalog/activation contract binaries pass on QCS6490; no concrete usecase is exercised by
those fixtures. Feature IDs are stable independently of model ID/package. Dependencies are
shared only when compatible. **Layer:** runtime. **Source:**
`src/runtime/feature_manager/vqec_vision_feature_catalog.cpp`,
`src/core/vqec_vision_feature_catalog.cpp`,
`tests/unit/vqec_vision_feature_catalog_test.cpp`,
`tests/unit/vqec_vision_feature_catalog_loader_test.cpp`.

## Responsibility

- Identifies each feature version, processor contract, configuration schema, named model
  roles, exact attribute schema/version freshness requirements and bounded temporal/event
  resources.
- Pins the model catalog identity and lets the pure cross-validator reject missing model
  dependencies.
- Distinguishes `single_model` from `temporal_join` and never treats adjacent round-robin
  results as synchronized.
- Does not express entitlement, desired state, per-source configuration values or artifact
  paths.
- Does not authenticate the package, payload, entitlement or artifact.

## Integration catalog boundary

`feature_catalog` is the authenticated-package projection used before constructing a
specific usecase. The root pins the model catalog identity, and the pure cross-validator
rejects missing model dependencies.

`single_model` requires exactly one model role and can use the current per-model feature
pipeline. `temporal_join` requires at least two distinct model roles and declares that a
separate bounded source/frame join is necessary. The catalog never treats adjacent
round-robin results as synchronized.

A synthetic example enables no feature. A later trusted activation document binds source
IDs and feature IDs, resolves feature-specific configuration and checks accumulated
temporal resources before owner construction.

Concrete compiled-in packages register a factory for `processor_contract`; see the
[feature processor registry](feature_processor_registry.md). The factory validates the
feature-specific configuration payload identified by `configuration_schema` and returns
an independent stateful processor owner.

The optional `feature_catalog` JSON loader applies the same bounded 512 KiB/depth-16,
duplicate-key and unknown-key checks as the deployment/model loaders before invoking the
core validator. Parsing is a metadata step: it does not authenticate the package, payload,
entitlement or artifact.

## Candidate feature set

| # | Feature ID | Required compute and state | Main output / acceptance |
|---|---|---|---|
| 1 | abnormal_behavior / brawl, conflict | person + pose/action temporal, multi-person context | alarm episode; precision/recall + false alarms/hour + onset delay |
| 2 | crowd_gathering | person tracking + zone density/dwell/group rules | crowd event; crowd recall, false alarms, dwell timing |
| 3 | smoking | person/hand/face/object or action model; ROI quality + temporal confirmation | smoking event; tiny-object visibility + hard negatives + false alarms |
| 4 | intrusion | person/object tracks + polygon/line rules | entry/exit/intrusion event; geometry + duplicate suppression |
| 5 | abandoned_object | object track + stationary timer + person-object relation, occlusion | abandonment event; owner absence ambiguity + reset correctness |
| 6 | person_tracking | detector + MOT, optional ReID | tracks; IDF1/HOTA or unified metric, ID switches |
| 7 | retrieval | attributes/embedding + track entity refs + external search index | search records/results; retrieval recall@k + latency + authorization |
| 8 | ppe / helmet, reflective vest | person/PPE detection/classification + body association | PPE violation; per-class quality, missing vs not observable |
| 9 | suspicious_object / weapon | dedicated detector + person association + temporal policy | suspicious-object event; false positives/hour + minimum pixels |
| 10 | luggage_tracking / luggage, cart | object detection/MOT + person-object relation | tracks/relations; continuity under occlusion |
| 11 | blacklist | face detect/align/quality/FR + versioned gallery | match candidate/alarm; FAR/FRR at agreed threshold + privacy |
| 12 | heatmap / time-based density | person positions/tracks + dwell grid + time buckets | heatmap; exposure, occupancy/dwell accuracy + gap handling |
| 13 | counting / zone entry-exit | person tracks + directional lines/polygons + hysteresis | windowed counts; count error, direction, duplicate/restart correctness |

Do not promise a complete task 1/3/9 just because a person detector exists.
An abandoned object cannot always infer its owner from nearest distance.
Blacklist needs a gallery service + enrollment/model-version workflow, not only
single-frame cosine similarity. The full retrieval backend belongs to FW software.

## Human attribute framework

An attribute carries attribute_type + schema_version, entity/track ref, typed
value/candidates, confidence, quality, observation time, freshness/expiry, model_version,
known/unknown/not_observable and provenance.
Extend shirt/pants color/type, bag/accessory, PPE, face quality, estimated age/gender and
FR through the registry, without adding a fixed field per model in the track.

An FR identity candidate separates visual attributes and local track id. Estimated
age/gender does not confirm identity. Do not infer sensitive attributes outside product
scope. Per-attribute entitlement/privacy and purge/retain policy are decided by FW.
Temporal fusion follows quality/cooldown to avoid keeping a wrong label for the whole
track.

## Traffic readiness

Entity category extends person/vehicle/baggage/cart; relations include carries, near,
associated_with and vehicle_has_plate; the ontology version is explicit.
Reusable detection/tracking/counting/heatmap are extended with vehicle attributes, plate
OCR, lane/polygon direction and calibration/time contracts.

Traffic is deployed in order: vehicle count/class -> lane/direction -> plate OCR
association -> calibrated speed -> red-light/crossing event if signal time exists.
Do not infer legal speed from pixel displacement; it needs calibration, clocks, accuracy
and product-appropriate validation. Red-light needs an authoritative signal input.
Traffic full production is not committed for the first 12 weeks.

## Release workload profiles

- W1: person tracking + intrusion + counting + heatmap + gathering.
- W2: W1 + PPE or luggage/abandoned, depending on resource budget.
- W3: W1 + FR/blacklist/attrs with ROI rate + gallery quota.
- W4: temporal action/smoking/weapon profile with temporal model constraints.
- Each profile pins board/image/models/cameras/FPS/ROI limits/KPIs.

These are benchmark candidates, not combinations that have reached performance.

## Limits and next work

- Every concrete feature remains unimplemented and unqualified.
- No concrete usecase is exercised by the generic catalog/activation contract fixtures.
- A later trusted activation document must bind source/feature IDs, resolve
  feature-specific configuration and check accumulated temporal resources before owner
  construction.
- Parsing and validation do not authenticate the package, payload, entitlement or
  artifact.

## See also

- [Feature processor registry](feature_processor_registry.md)
- [Feature activation manager](feature_activation_manager.md)
- [Feature stage](feature_stage.md)
- [Model catalog](model_catalog.md)
- [Multi-model feature pipeline](multi_model_feature_pipeline.md)
