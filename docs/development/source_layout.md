# Repository source layout

This document defines the physical repository map, ownership boundaries and dependency
direction used to keep LACAI scalable as models, features and platforms are added.

**Status:** board-smoke — the role-oriented tree passed eSDK 168/168; the prior clean-layout
QCS6490 run passed 128/128 native executables. Current S04 catalog/lifecycle behavior passed the
exact-candidate board gate; release acceptance remains open. **Layer:** docs.
**Source:** `src/`, `include/`, `tests/`, `tools/`, `config/`, `manifests/`.

## Responsibility

- Give each implementation file one discoverable owner and one review route.
- Preserve stable installed include paths, executable names, symbols, schemas and wire ABI.
- Keep portable policy and orchestration independent from vendor SDKs and external services.
- Define where new code belongs before it is added; a convenient include path is not ownership.

## Top-level map

| Path | Owner and purpose | Placement rule |
|---|---|---|
| `include/vqec/vision/ai/` | AI APP public contracts, ports and plugin ABI | Stable public include surface; regrouping is an API migration, not a cleanup |
| `src/core/` | AI APP pure validation/value/bookkeeping | No I/O, vendor types or runtime construction |
| `src/runtime/` | AI APP admission, activation, registries, lifecycle, scheduling | Portable state and policy only |
| `src/perception/` | AI Model + AI APP tensor interpretation boundary | Algorithms consume neutral tensor contracts |
| `src/features/` | AI APP business feature packages | One capability per directory; event policy only |
| `src/outputs/` | AI APP portable authorization and delivery preparation | No FW/vendor transport implementation |
| `src/app/` | AI APP process composition and bounded orchestration | Owns lifecycles; does not own model math or BSP ABI |
| `src/adapters/` | AI APP integration adapters with BSP/FW review | External types remain private to the adapter |
| `config/` | Product policy examples and schemas | Policy is validated data, never compiled magic values |
| `manifests/` | Model/feature integration declarations | Metadata only; artifact authenticity is separate |
| `tests/` | Verification grouped by evidence type and owner | Unit/contract paths mirror source ownership |
| `tools/` | Developer/CI utilities grouped by execution role | Use `checks`, `build`, `board`, `diagnostics` or `fixtures`; do not add flat tools |
| `third_party/` | Reviewed external source snapshots | Preserve upstream layout and provenance; never mix project source |
| `packaging/` | Deployment integration | Must consume built artifacts; no product logic |

## Source ownership map

```text
src/
├── core/
│   ├── configuration  features  inference  media  memory  output  perception
├── runtime/
│   ├── admission  feature_manager  graph  lifecycle  model_registry  scheduler
├── perception/
│   ├── detection  tracking  attributes  pose  embedding  ocr
├── features/
│   └── <one security or traffic capability per package>
├── outputs/
│   ├── events  media
├── app/
│   ├── cascade  composition  pipeline  platform  service  session  supervision
└── adapters/
    ├── camera  fw_output  zvec  reference
    ├── fw_control/{app_manager,enrollment,usecase}
    ├── storage/{app_lifecycle,evidence,identity,metadata}
    ├── qualcomm/
    │   ├── dsp/host  dsp/v1  dsp/legacy  gstreamer  media  qnn
    └── <future vendor adapter only after its SDK and contract are in scope>
```

`core/perception/` owns neutral perception value validation. `src/perception/` owns
algorithms and temporal state. This similarly named pair is deliberate and must not be
collapsed: value contracts can be used without linking decoder/tracker implementations.

Within `src/app/`:

| Directory | Owns | Must not own |
|---|---|---|
| `service/bootstrap/` | Minimal entrypoint, CLI parsing, process/generation lifecycle | Feature rules, graph algorithms |
| `service/generation/` | One generation's neutral composition and authority binding | Adapter implementation details |
| `service/enrollment/` | Offline enrollment adapter composition and graph lifecycle | Gallery semantics, live-camera cascade |
| `service/output/` | Metadata/evidence/output owner composition | Model execution, camera acquisition |
| `platform/` | Construction of fake/reference/production owner bundles | Frame stepping or feature policy |
| `composition/` | Wiring validated owners behind neutral ports | External ABI translation |
| `session/` | Acquisition and graph start/drain/release lifecycle | Global fairness or model decoding |
| `pipeline/` | One bounded frame/result/feature/media progress operation | Threads, CLI or platform discovery |
| `supervision/` | Worker ownership, source fairness and stop coordination | Vendor SDK objects |
| `cascade/` | Secondary/single-image task orchestration | Primary source acquisition |

Within `src/adapters/qualcomm/`:

| Directory | Owns |
|---|---|
| `qnn/` | Direct QNN runtime loading, graph execution and neutral graph adapter |
| `gstreamer/` | Qualcomm plugin graph, DMA-BUF bridge, submission and tensor extraction |
| `media/` | Image source/alignment/color and preview-overlay-encode implementation |
| `dsp/host/` | ARM-side FastRPC transport, shared-buffer lifetime and port adapters |
| `dsp/v1/` | Canonical LACAI v1 generic DSP wire/service/skeleton |
| `dsp/legacy/` | Frozen pre-baseline compatibility material pending removal/owner decision |

Within the non-vendor integration adapters:

| Directory | Owns |
|---|---|
| `storage/app_lifecycle/` | Immutable application content and transactional lifecycle inventory |
| `storage/metadata/` | Transactional metadata plus high-rate spatiotemporal detail and projections |
| `storage/evidence/` | Durable evidence command/receipt outbox |
| `storage/identity/` | Encrypted authoritative face-gallery persistence |
| `fw_control/app_manager/` | AI-owned application lifecycle D-Bus facade |
| `fw_control/enrollment/` | Enrollment D-Bus and authorized image-FD acquisition |
| `fw_control/usecase/` | Legacy desired-state compatibility D-Bus facade |

These leaf directories are physical ownership boundaries. Existing `stor` and `fwctl` symbol
prefixes remain stable; a physical move does not create a new public ABI or rename functions.

## Dependency direction

The allowed direction is from process composition toward stable policy and contracts:

```text
service -> platform/composition -> supervision/session/pipeline/cascade
        -> runtime + perception + features + outputs
        -> core -> public contracts/ports

platform/composition -> adapters -> public contracts/ports + external SDK/ABI
```

Rules:

1. `core`, `runtime`, `perception`, `features` and portable `outputs` never include an
   adapter-private header.
2. `app` selects adapters only in platform/composition code. Pipeline/session code depends
   on neutral ports.
3. Adapters may use portable validation, but adapters do not define business activation or
   authorization policy.
4. Feature packages consume neutral perception and emit neutral events; evidence, Kafka,
   FW IPC and persistence remain output/adapter responsibilities.
5. A move does not change `dir_id`, `file_id`, function symbols, LACAI version or public ABI.

## Test ownership map

`tests/unit/` and `tests/contract/` mirror implementation ownership:

```text
tests/{unit,contract}/
├── core  application  runtime  perception  outputs
└── adapters/<camera|fw_control|fw_output|qualcomm|reference|storage|zvec>
```

Not every leaf exists in both evidence types. Create a leaf only when it has a test. CTest
names and executable names remain stable so CI and board runners do not depend on source path.

## Placement checklist

Before adding a file:

1. Identify the data/ownership boundary, not merely the caller that needs the code.
2. Select the lowest portable layer that can own it without an upward dependency.
3. Register the exact path and existing logical symbol owner in `naming_registry.md`.
4. Put vendor/FW/storage types in the matching adapter and expose a neutral port if needed.
5. Add tests under the mirrored owner path and update the closest module README.
6. Run source/docs layout checks and the eSDK build/test workflow.

## Limits and next work

- This refactor changes physical implementation paths only. It does not claim feature,
  released-FW, DSP deployment, accuracy, thermal or long-run acceptance.
- `include/` remains intentionally grouped by public API kind to avoid gratuitous include-path
  breakage. A future split requires an approved compatibility/migration decision.
- Header/source pairs count as one logical owner. Current implementation leaves contain at most
  nine logical owners and remain cohesive; a raw file-count target is not grounds for moving a
  stable public include or splitting one lifecycle across artificial directories.
- The large SQLite implementations remain isolated behind narrow ports, but splitting their SQL,
  migration and projection internals is separate behavior-sensitive work; directory cleanup does
  not pretend that such a split has already happened.
- Tool filenames remain stable, while role directories make host-only checks, reproducible
  builds, target probes, optional diagnostics and compatibility fixtures distinguishable.
- The Qualcomm legacy DSP subtree is isolated but not approved as a production generic ABI.

## See also

- [System architecture](../architecture/system_architecture.md)
- [Code convention](code_convention.md)
- [Naming registry](naming_registry.md)
- [Review checklist](review_checklist.md)
- [QCS6490 board evidence](../testing/qsc6490_board.md)
