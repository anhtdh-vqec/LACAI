# ADR 0005 — Scalable model integration (packages, roles, alignment)

Status: proposed for implementation, 2026-09-15. Owner/reviewer: AI APP lead plus the
affected contract owners (catalog, image processor, pump ownership).
This decision changes an external boundary (model catalog and a new port); it requires the
owner review named in AGENTS.md before the schema and port implementation land.

## Context

The first models are one primary detector (SCRFD) and one secondary embedding model
(EdgeFace), followed by more detectors, embeddings, pose, OCR and attribute models across
13+ features. Adding a model must not add a vendor branch or a model-name branch to
orchestration. Today the catalog has no notion of a model being primary or secondary, and
landmark-based alignment has no neutral boundary. Retention in the pump is not yet keyed to
which models actually need the source pixels.

## Decision

1. **Package is the unit of integration.** A new model is a reviewed package
   (`io_manifest`, `decoder.json` when applicable, `preprocess.json`, labels, digest),
   a catalog entry, a package-registry binding and, if its output shape is new, a decoder
   registered by `decoder_contract`. Orchestration, pump and session are unchanged.
2. **The catalog declares role and dependency.** Add `role` (`primary`/`secondary`) and
   `depends_on` (the primary model/decoder contract a secondary model consumes) to the
   model catalog. Role drives full-frame cadence membership, whether the source frame must
   be retained for dependents, and which output path applies. Secondary graphs never join
   the full-frame submit mask.
3. **Alignment is a separate, capability-gated neutral port.** Landmark-based similarity
   warp/crop is a distinct operation from full-frame preprocess. A vendor processor that
   cannot honor a declared template fails activation; there is no silent CPU fallback. The
   port describes source coordinates, destination tensor/lease ownership, interpolation and
   border policy, and completion.
4. **Retention follows role.** The pump retains the exact source frame only for catalog
   models that have secondary dependents, under the domain-scoped cascade ticket, and
   releases it only when the last dependent hardware read completes. Timeout, FD close and
   stop are not completion.
5. **Decoders and backends stay registry + capability driven.** Many models reuse one
   decoder contract and one backend bundle; a new decoder kind registers without touching
   the pump/session. Backend selection is by probed capability and validated policy.
6. **Per-model verification.** Every package is closed against
   `manifests/models/VERIFICATION_CHECKLIST.md` with golden data; metadata alone is not
   acceptance.

## Alternatives

- Extend `image_processor_port` with an `align` operation: forces every vendor processor to
  implement landmark warp and couples two different ownership models. Rejected.
- A model-name/role switch in orchestration: violates the no-model-branch rule and does not
  scale past a handful of models. Rejected.
- One thread/context per model or per secondary task: resource cost without measured need.
  Rejected; secondary work uses one bounded cascade execution domain.

## Consequences

- The model catalog contract/validator/loader/schema gain `role` and `depends_on`; existing
  entries default to `primary` with no dependency.
- A new `image_alignment_port` and a Qualcomm alignment adapter are required for M4; the
  FastCV/QTI affine capability must be verified before it is claimed.
- The pump and session must retain/release by role; this is an ownership change and needs
  the pump/session owner review.
- Portability is preserved: no vendor type crosses the neutral contracts; other vendors
  implement the same ports or return unsupported.
