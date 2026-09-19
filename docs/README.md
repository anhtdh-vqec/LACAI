# Documentation map

Entry point for the LACAI documentation tree. Authority order, templates, naming and status
vocabulary are normative in [documentation style](development/documentation_style.md).
Delivery/evidence claims are owned by [implementation status](development/implementation_status.md)
and [capability matrix](development/capability_matrix.md); this map only routes.

**Status:** reference map — routes to the authority documents.

## Start here

| Document | Purpose |
|---|---|
| [AGENTS.md](../AGENTS.md) | Mandatory repository rules and change workflow |
| [Code convention](development/code_convention.md) | C++17 naming, layout, literal, ownership rules |
| [Documentation style](development/documentation_style.md) | File naming, language, template, status vocabulary |
| [Naming registry](development/naming_registry.md) | dir_id/file_id owners and function prefixes |
| [Repository source layout](development/source_layout.md) | Physical ownership map and dependency direction |
| [Review checklist](development/review_checklist.md) | Per-PR and per-boundary checks |

## Current status and priorities

| Document | Purpose |
|---|---|
| [Implementation status](development/implementation_status.md) | Current delivery/evidence claims |
| [Capability matrix](development/capability_matrix.md) | Per-capability source/native/open state |
| [Architecture alignment review](development/architecture_alignment_review.md) | Open architecture issues A01–A25 |
| [Plan 0 production composition review](development/production_composition_foundation_review.md) | Current gate audit and unblock decision |

## Architecture

`docs/architecture/` describes one scoped mechanism per file: contracts and ports, camera/FW
boundary, Qualcomm adapter, perception/feature pipeline, runtime composition and output.
Read [system architecture](architecture/system_architecture.md) first, then the module that
owns the boundary you are changing.
For production resource input, see the
[hardware admission profile](architecture/hardware_admission_profile.md).
For DSP ownership and the generic protocol requirements, see the
[Qualcomm FastRPC adapter](architecture/qualcomm_fastrpc_adapter.md).
For the target footprint, trajectory, cross-camera, event and aggregate design, see the
[spatiotemporal metadata architecture](architecture/spatiotemporal_metadata.md). The existing
[metadata transactional prototype](architecture/metadata_query.md) records narrower SQLite
transaction/outbox measurements.
For AI-owned application distribution, inventory and runtime-control authority, see the
[usecase application manager](architecture/app_manager.md).

## Contracts

`docs/contracts/` defines exact external boundaries (FW camera, FW control, usecase control,
face enrollment/gallery, model integration, FW release compatibility). A proposal is not a
released protocol; contract changes need owner review before source.

The [three-team integration registry](contracts/integration_contract_registry.md) is the
normative owner/version/handoff authority for C01–C10 and stable S01–S18 identities.

## Development

`docs/development/` holds repository rules, the naming registry, the review checklist and
the current-status documents above.

## Planning

`docs/planning/` holds forward-looking Vietnamese roadmaps and cohort plans. Plans describe
unfinished work and do not carry current-capability claims.

- [Architecture improvement review](planning/architecture_improvement/README.md): detailed
  source-based assessment, three-team ownership/handoff contracts, security/traffic data and
  query catalogs, storage options, Kafka, FW evidence IPC, video ownership migration and
  portable DSP offload with validation gates.
- [Usecase app distribution plan](planning/architecture_improvement/usecase_app_distribution_plan.md):
  app-as-SKU/shared-runtime packaging, entitlement-gated download and atomic install lifecycle.
- [Fire/smoke product slice plan](planning/architecture_improvement/fire_smoke_product_slice_plan.md):
  detailed sequence to split the service main, deliver the AI-owned App Manager and productize
  S04 through configuration, metadata and evidence.

## Testing

`docs/testing/` documents the eSDK/QEMU and QCS6490 board evidence, configuration matrix and
validation runbooks. Start with the [board workspace and workflow](testing/board_workspace.md)
for the standard `/opt/lacai` layout and the build/stage/test/run procedure; dated raw runs
are retained and never retroactively updated.

## Research

`docs/research/` is a dated reference inventory of vendor/source behavior. It documents
upstream facts, not LACAI capability. The
[metadata storage source review](research/metadata_storage_source_review.md) compares the
transactional, columnar and moving-feature sources used by the P2 redesign.

## ADRs

`docs/adr/` records decisions in numeric order; ADR 0001 baseline, 0002 plugin backend,
0003 owned QNN engine, 0004 FR gallery/index, 0005 scalable model integration,
0006 unwired execution infrastructure, proposed 0007 versioned FastRPC operations, and
0008 transactional metadata store baseline, accepted 0009 spatiotemporal metadata tiering, and
proposed 0010 AI-owned usecase application lifecycle.

## Operations

`docs/operations/` is a planned operational runbook area; see its README for current scope.

## Checks

- `tools/checks/vqec_vision_check_source_layout.sh` — source/header/tool filenames and includes.
- `tools/checks/vqec_vision_check_docs_layout.sh` — docs filenames, titles, status lines and links.
