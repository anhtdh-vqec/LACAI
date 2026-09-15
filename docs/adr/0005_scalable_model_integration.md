# ADR 0005 — Scalable model integration (packages, roles, alignment)

Status: **proposed — not approved.** This decision changes external boundaries (model
catalog schema, a new port) and runtime ownership (pump/session retention), so it must not
be implemented until the approve-after gates below are met and the affected owners have
reviewed it.
Date: 2026-09-15. Owner/reviewer: AI APP lead plus the contract owners for the catalog,
image processor and pump/session ownership.

## Context

The first models are one primary detector (SCRFD) and one secondary embedding model
(EdgeFace), followed by more detectors, embeddings, pose, OCR and attribute models across
13+ features. Adding a model must not add a vendor branch or a model-name branch to
orchestration. Today: the catalog has no role/dependency concept; there is no neutral
alignment boundary; production still constructs some owners directly; and retention is a
delivered primitive that is not connected to the pump. This ADR states the intended
architecture and the work required; it does not claim any of it is delivered.

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
4. **Retention follows role, as target behavior.** The pump retains the exact source frame
   only for catalog models with secondary dependents and releases it only after the last
   dependent hardware read completes. This is not delivered: the `cascade_frame_store`
   primitive (exact-key retention, domain-scoped tickets, byte budget) exists and is
   logic-tested, but it is **not wired into the pump**, and the pump does not yet create or
   drain secondary tasks.
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

Still open: the deeper activation/ownership integration (secondary models never entering the
full-frame submit mask at runtime) follows from the role field but is pump work tracked under
section 4.

## 2. `image_alignment_port` contract (must be defined before approval)

The port does not exist. Before approval it must specify:

- **Input ownership**: how the retained `raw_frame` owner is borrowed for the whole warp,
  and that it stays alive until device completion;
- **Landmarks**: schema id/version, point order, count and coordinate space (source pixels),
  and how a mismatched or degenerate set is rejected;
- **Geometry**: source ROI/key and destination tensor or crop geometry, with explicit
  coordinates and units;
- **Transform provenance**: the similarity/affine transform returned so results can be
  mapped back to the source frame;
- **Buffer ownership**: who owns the destination crop/tensor pool, pool bounds and reuse
  rules;
- **Completion**: cache/fence/device-completion semantics; timeout and stop are not
  completion;
- **Errors/timeout**: status taxonomy and deadline behavior, with no silent fallback.

## 3. Retention and completion semantics (target)

- `cascade_frame_store` is delivered as a primitive: exact camera/channel/epoch/frame/PTS
  keys, byte budget, and completion tickets scoped by store domain so a stale callback
  from a retired store cannot release a replacement store's task. It is logic-tested.
- Pump integration is target work: retain before primary submit, roll back on a rejected
  submit, look up the exact frame from the submission ticket (never the latest preview),
  bound secondary tasks, and drain by dependent graph.
- The final completion/domain semantics and the ticket owner (which component calls
  `complete`) must be reviewed with the pump/session owner before implementation.

## 4. Orchestration scope (corrected)

"Orchestration unchanged" is too strong. The correct statement is: **no per-model change**;
generic orchestration changes are required. `multi_model_pump`/`multi_model_session` must
gain the ability to distinguish primary from secondary models, retain a frame before a
primary submit, create bounded secondary tasks from a decoded detection, and drain per
dependent graph. These changes are metadata-driven and model-agnostic.

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
   loaders and match the runtime ABI (see `VERIFICATION_CHECKLIST.md`);
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
- [ ] `image_alignment_port` contract is defined with the ownership/completion/error fields
      above.
- [ ] Pump/session owner review of the retention and dependent-drain ownership is complete.
- [ ] Retention generation/completion semantics and the `complete(ticket)` owner are
      finalized.
- [ ] Capability/status docs are updated so none of the above is claimed as delivered
      before it is.
