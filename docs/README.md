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
| [Review checklist](development/review_checklist.md) | Per-PR and per-boundary checks |

## Current status and priorities

| Document | Purpose |
|---|---|
| [Implementation status](development/implementation_status.md) | Current delivery/evidence claims |
| [Capability matrix](development/capability_matrix.md) | Per-capability source/native/open state |
| [Architecture alignment review](development/architecture_alignment_review.md) | Open architecture issues A01–A25 |
| [Clean-base plan](planning/clean_base_plan.md) | Completed clean-base workstreams and residual work |

## Architecture

`docs/architecture/` describes one scoped mechanism per file: contracts and ports, camera/FW
boundary, Qualcomm adapter, perception/feature pipeline, runtime composition and output.
Read [system architecture](architecture/system_architecture.md) first, then the module that
owns the boundary you are changing.

## Contracts

`docs/contracts/` defines exact external boundaries (FW camera, FW control, usecase control,
face enrollment/gallery, model integration, FW release compatibility). A proposal is not a
released protocol; contract changes need owner review before source.

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

## Testing

`docs/testing/` documents the eSDK/QEMU and QCS6490 board evidence, configuration matrix and
validation runbooks. Start with the [board workspace and workflow](testing/board_workspace.md)
for the standard `/opt/lacai` layout and the build/stage/test/run procedure; dated raw runs
are retained and never retroactively updated.

## Research

`docs/research/` is a dated reference inventory of vendor/source behavior. It documents
upstream facts, not LACAI capability.

## ADRs

`docs/adr/` records decisions in numeric order; ADR 0001 baseline, 0002 plugin backend,
0003 owned QNN engine, 0004 FR gallery/index, 0005 scalable model integration,
0006 unwired execution infrastructure.

## Operations

`docs/operations/` is a planned operational runbook area; see its README for current scope.

## Checks

- `tools/vqec_vision_check_source_layout.sh` — source/header/tool filenames and includes.
- `tools/vqec_vision_check_docs_layout.sh` — docs filenames, titles, status lines and links.
