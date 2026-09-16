# Usecase activation before model loading

`vqec_vision_usecase_activation` is the vendor-neutral cold-path resolver between the FW
commercial control plane and runtime composition. Its base deployment is the maximum
authenticated source/model assignment. A complete activation snapshot applies installed,
entitled, desired, supported, compatible and resource-admitted gates to every
`(source_id, usecase_id)` association.

Only `ready` associations contribute root models. The resolver preserves base source and
model order, unions shared roots, and removes inactive sources. Secondary models are not
listed as roots; the model catalog activates them through immutable dependencies. Cascade
retention is removed when the filtered source activates no secondary model.

An empty effective deployment is a valid idle result. The generation owner must handle it
without calling normal deployment validation, platform prepare or camera acquisition. A
non-empty result is passed through deployment and model-catalog validation before publish.

The resolver is transactional: malformed, incomplete or duplicate snapshots leave its
output objects unchanged. It does not authenticate catalogs or entitlements, measure
hardware capacity, own D-Bus, load models or mutate a live runtime. Those belong to the
trusted control adapter, admission provider and generation owner. Dynamic disable remains
incomplete until that owner blocks output, stops submissions, drains backend completion
and destroys the obsolete generation.

See [FW usecase activation](../contracts/fw_usecase_control.md) for the wire contract and
lifecycle requirements.
