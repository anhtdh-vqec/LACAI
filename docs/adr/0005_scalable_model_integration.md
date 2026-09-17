# ADR 0005 — Scalable model integration (packages, roles, alignment)

Status: **accepted; core source integration delivered** (owner-directed, 2026-09-15).
Catalog roles/dependencies, the alignment port/Qualcomm adapter, pump retention/drain,
secondary graph lifecycle and metadata-driven runtime invocation are delivered with logic
tests. Live golden parity, asynchronous scheduling, pooling and backend registry work remain.
Date: 2026-09-15. Owner: AI APP lead.

## Context

The first models are one primary detector (SCRFD) and one secondary embedding model
(EdgeFace), followed by more detectors, embeddings, pose, OCR and attribute models across
13+ features. Adding a model must not add a vendor branch or a model-name branch to
orchestration. The catalog, alignment boundary and retained cascade now implement that
generic path; production still constructs some Qualcomm owners directly and needs live
model evidence. This ADR defines the accepted architecture and its remaining gates.

## Decision

1. **Package is the unit of integration.** A new model is a reviewed package
   (`io_manifest`, `decoder.json` when applicable, `preprocess.json`, labels, digest), a
   catalog entry, a package-registry binding and, if its output shape is new, a decoder
   registered by `decoder_contract`. Orchestration changes only generically, driven by
   metadata — never per model.
2. **The catalog declares required role and dependency metadata.** `role`
   (`primary`/`secondary`) and `depends_on` are explicit, validated fields — not silent
   defaults. `depends_on` is required when `role=secondary` and forbidden when
   `role=primary`. A dependency references an immutable model identity (model id +
   version + target/catalog revision), not an arbitrary free string.
3. **Alignment is a separate, capability-gated neutral port** with a defined contract
   (below). A vendor processor that cannot honor a declared template fails activation; no
   silent CPU fallback.
4. **Retention follows role.** The pump retains the exact source frame
   only for catalog models with secondary dependents and releases it only after the last
   dependent hardware read completes. `cascade_frame_store`, pump/session integration,
   ticket-correlated coordinator invocation and dependent drain implement this in source.
5. **Backend selection by registry/capability is a target, not current state.**
   `production_platform` still constructs the QNN engine/FastCV processor directly and
   registers a reference tracker/feature. Capability-checked backend selection and a
   generic feature/tracker registry are open work.
6. **Per-model verification is separated from acceptance** (ladder below).

## 1. Catalog role and dependency (required change)

Delivered in schema version 2: `role` is a required validated field; `depends_on` is required
for `secondary` and forbidden for `primary`, references an immutable
`(model_id, model_version, target_id)` primary identity, and rejects self/duplicate/
secondary targets. `validate_deployment_models` rejects a secondary model as a full-frame
assignment. Version 1 documents are migrated to version 2 with primary roles. Contract,
loader, validator, schema, examples and tests (`model_catalog_validation`,
`model_catalog_loading`) are committed; see `docs/architecture/model_catalog.md`.

Runtime composition activates dependencies without inserting secondary models into the
full-frame submit mask and derives the cascade-root slot from exact catalog identity.

## 2. `image_alignment_port` contract

Delivered: `include/vqec/vision/ai/contracts/vqec_vision_image_alignment.hpp` and
`ports/vqec_vision_image_alignment.hpp`, with pure validators and the
`image_alignment_contract` unit test. The contract specifies:

- **Input ownership**: the caller keeps the source `raw_frame` owner alive until
  `poll_completion` reports complete; timeout/stop/disconnect/FD close are not completion;
- **Landmarks**: schema id/version, ordered reference points; mismatched schema, count or
  non-finite points are rejected;
- **Geometry**: source key + destination width/height from the template; source geometry in
  the returned transform;
- **Transform provenance**: row-major 2x3 source→destination transform returned with the
  result;
- **Buffer ownership**: the destination `tensor_blob` in `alignment_result` is owned by the
  caller; the port owns no pool;
- **Completion**: `align` returns a ticket and `poll_completion` reports device completion;
  unknown tickets are `invalid_state`;
- **Errors/timeout**: structural validation and fail-closed capability checks
  (`unsupported`), no silent fallback.

The Qualcomm FastCV affine/RGB adapter is delivered and bound through this port. Still open:
crop/tensor pool sizing, golden crop parity and dependent hardware-completion evidence.

## 3. Retention and completion semantics (target)

- `cascade_frame_store` is delivered as a primitive: exact camera/channel/epoch/frame/PTS
  keys, byte budget, and completion tickets scoped by store domain so a stale callback
  from a retired store cannot release a replacement store's task. It is logic-tested.
- Pump/session integration retains before primary submit, rolls back a rejected submit,
  reconstructs the exact frame from the submission ticket and drains dependent tasks.
- The cascade coordinator owns acquired task completion; the session owns frame admission,
  retirement and the source-release gate. Domain-scoped tickets reject stale completion.
- Detailed ownership and stop ordering are normative in
  [pump cascade retention](../architecture/pump_cascade_retention.md).

## 4. Orchestration scope (corrected)

"Orchestration unchanged" is too strong. The correct statement is: **no per-model change**.
Generic orchestration now distinguishes primary from secondary models, retains a frame
before primary submit, invokes bounded dependent work from decoded observations and drains
by dependent graph. These changes are metadata-driven and model-agnostic.

## 5. Backend and registry status (corrected)

Target: backend selection by probed capability and validated policy, with decoders,
trackers and feature processors resolved through registries keyed by contract. Current:
the QNN engine and FastCV processor are constructed directly in `production_platform`, and
a reference tracker/feature are registered as stand-ins. This ADR records the target; the
capability/status docs must not present it as delivered.

## 6. Verification vs acceptance ladder (required)

Package verification is necessary but not sufficient. Closable stages, each with its own
evidence and owner:

1. **Package verification** — manifests/decoder/preprocess parse through the strict
   loaders and match the runtime ABI (see `verification_checklist.md`);
2. **Board model execution** — the exact artifact executes on QCS6490 with recorded
   parity/limits;
3. **FD→FR correlation** — the retained source frame maps to the correct face/embedding
   per source epoch/frame/track;
4. **Accuracy/calibration** — thresholds calibrated and measured on a representative
   dataset (false accept/reject, identification recall);
5. **Attendance acceptance** — enrollment, matching, delivery/retry, restart recovery and
   delete semantics accepted end to end.

Only stage 5 is usecase acceptance. Metadata or a single execution success never closes a
later stage.

## Alternatives

- Extend `image_processor_port` with an `align` operation: forces every vendor processor to
  implement landmark warp and couples two ownership models. Rejected.
- A model-name/role switch in orchestration: violates the no-model-branch rule. Rejected.
- Implicit `role=primary` default: conflicts with the no-hardcode/no-silent-default rule.
  Rejected.
- One thread/context per model or per secondary task: resource cost without measured need.
  Rejected; secondary work uses one bounded cascade execution domain.

## Consequences

- The catalog contract/validator/loader/schema gain required `role` and `depends_on` plus a
  documented migration; existing catalogs must be migrated explicitly, not defaulted.
- A new `image_alignment_port` and a Qualcomm alignment adapter are required for M4; the
  FastCV/QTI affine capability must be verified before it is claimed.
- Pump/session retention is an ownership change and needs the pump/session owner review.
- Until implemented, docs must keep these items in the open-work lists, not in capability
  claims.

## Approve after (gates)

- [x] Catalog/schema/validator migration with required `role`/`depends_on` is committed and
      tested (valid, missing, wrong-role dependency, cycle, self-reference).
- [x] `image_alignment_port` contract is defined with the ownership/completion/error fields
      above.- [x] Pump/session owner review of the retention and dependent-drain ownership is complete.
- [x] Retention generation/completion semantics and the `complete(ticket)` owner are
      finalized.
- [x] Capability/status docs are updated so none of the above is claimed as delivered
      before it is.
