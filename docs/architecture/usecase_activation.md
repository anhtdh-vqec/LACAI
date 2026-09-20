# Usecase activation before model loading

`vqec_vision_usecase_activation` is the vendor-neutral cold-path resolver between the FW
commercial control plane and runtime composition. Its base deployment is the maximum
authenticated source/model assignment. A complete activation snapshot applies installed,
entitled, desired, supported, compatible and resource-admitted gates to every
`(source_id, usecase_id)` association.

**Status:** source-delivered — resolver, strict startup loader, serialized desired-plan
manager and D-Bus v1 transport are source-delivered; signed provisioning and runtime health
observation remain open. **Layer:** runtime.
**Source:** `src/core/features/vqec_vision_usecase_activation.cpp`,
`src/runtime/feature_manager/vqec_vision_usecase_config.{hpp,cpp}`,
`src/runtime/feature_manager/vqec_vision_usecase_control_manager.{hpp,cpp}`,
`src/adapters/fw_control/usecase/vqec_vision_usecase_control_dbus.{hpp,cpp}`.

## Responsibility

- Resolve the effective root-model deployment from the FW commercial control plane before
  any vendor graph loads.
- Apply installed, entitled, desired, supported, compatible and resource-admitted gates
  to every `(source_id, usecase_id)` association.
- Must not authenticate catalogs or entitlements, measure hardware capacity, load models or
  mutate hardware directly; those belong to the trusted provisioning boundary, admission
  provider and serialized generation reconciler.

## Effective deployment

Only `ready` associations contribute root models. The resolver preserves base source and
model order, unions shared roots, and removes inactive sources. Secondary models are not
listed as roots; the model catalog activates them through immutable dependencies. Cascade
retention is removed when the filtered source activates no secondary model.

An empty effective deployment is a valid idle result. The generation owner must handle it
without calling normal deployment validation, platform prepare or camera acquisition. A
non-empty result is passed through deployment and model-catalog validation before publish.

## Transactional behavior

Single-model feature projection selects one complete usecase association whose declared
feature and root-model dependency match the source assignment. Its immutable record includes
the exact model slot (an unset slot is invalid), attribute schema scopes and nonzero
configuration/policy revisions. No ready record may combine gates from unrelated usecases.
Missing model assignment leaves the caller's prior projection unchanged. Production passes
this record through feature reconciliation and derives output scopes from it.

The resolver is transactional: malformed, incomplete or duplicate snapshots leave its
output objects unchanged. For a compatible prepared-capacity snapshot, the service derives
per-application feature changes and reference-counted primary/cascade dependency deltas inside the
running generation. It first replaces output authority and feature bindings, then reconciles only
the affected model owners. Artifact/capacity/source identity changes and all-off remain explicit
generation-replacement boundaries.

## Startup loader

The strict startup loader `vqec_vision_usecase_config` accepts one authenticated snapshot
containing independent control, entitlement and deployment revisions. The service option
`--usecase-snapshot` resolves it before model-package resolution and
`production_platform::prepare`. A person-only snapshot therefore never prepares SCRFD or
EdgeFace; an all-disabled snapshot stays in the idle service loop without opening model
packages or acquiring the camera. The document's gate booleans are trusted inputs from the
provisioning/admission boundary, not self-asserted D-Bus authority.

## Desired-plan control manager

`vqec_vision_usecase_control_manager` owns the serialized desired-plan cold path. It:

- preserves installed, entitled, supported, compatible and admitted gates from the
  trusted startup snapshot while permitting FW to replace only `desired`;
- rejects unknown/duplicate associations, stale revisions and conflicting idempotency-key
  reuse before producing a candidate;
- composes a complete immutable effective deployment, preserves shared roots and retains
  a bounded receipt history;
- distinguishes the active generation from a candidate, reporting old work as running or
  draining and new work as loading until the runtime owner publishes it.

`vqec_vision_usecase_control_dbus` implements the exact v1 wire methods behind the neutral
port. It resolves a configured FW peer to one unique D-Bus sender, bounds request bytes and
callback progress, and never accepts entitlement/admission fields from the caller. Bus
name and object path are installation configuration.

## Executable generation ownership

The executable now owns manager/transport across runtime generations. `--usecase-dbus`
(or `--usecase-dbus-session`) enables desired-plan commands with explicit installation
names/timeouts/budgets. Initial status remains loading until all source-session phases are
running. New desired commands are rejected during initial loading or pending reconciliation.
Recovery-required prevents constructing another generation. All-off publishes an empty
runtime and keeps the control object live without platform preparation or source acquisition.
Live desired plans/receipts are process-local; synchronous prepare/enrollment can delay
D-Bus replies.

## Limits and next work

- Runtime health observation and signed provisioning remain open. Incremental shared-owner
  replacement is source-delivered; QCS6490 multi-app acceptance remains open.
- Live desired plans/receipts are process-local; synchronous prepare/enrollment can delay
  D-Bus replies.
- The resolver does not authenticate catalogs or entitlements, measure hardware capacity,
  load models or mutate a live runtime.

## See also

- [FW usecase activation](../contracts/fw_usecase_control.md) — wire contract and
  lifecycle requirements
- [runtime feature activation](runtime_feature_activation.md)
- [FR validation](../testing/face_recognition_production_validation.md)
