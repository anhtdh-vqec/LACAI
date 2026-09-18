# Architecture alignment review and remaining issue list

2026-09-16 follow-up: same-process desired-plan drain/rebuild/publication and private
volatile Zvec lifecycle are now delivered and compatibility-tested on `.98`. The initial
persistent plaintext index was removed after encrypted-gallery recovery passed. Current
measured cases, resource sample and remaining release gates are consolidated in
[FR validation](../testing/face_recognition_production_validation.md). Historical findings
below retain their review context; signed provisioning, durable receipts, asynchronous
control, hardware-key/swap policy and workload qualification remain open.

Date: 2026-09-15. Updated to match the current cascade/runtime source.
Scope: Markdown under docs, root rules/README and module READMEs, cross-checked against
source/CMake at the relevant boundaries. This is an architecture/documentation review, not
an audit of every code branch or a new benchmark/board qualification run.

**Status:** current — open architecture issue backlog A01–A25.

## 1. Conclusion on direction

**Not yet off the core goals, but there is a significant gap between the intended
architecture, the production composition and the inconsistently updated documents.**

Unchanged: AI receives RAW from FW, one acquisition per source, multi-model through
neutral ports, AI owns usecase and preview output, Qualcomm optimizes in adapters,
portability through vendor ports, no production OpenCV and no rewriting vendor kernels
just to own the implementation ourselves.

Two intentional changes need to be distinguished from deviations:

| Decision | Initially | Currently | Assessment |
|---|---|---|---|
| Qualcomm | Plugin-first ADR 0002 | Added owned QNN adapter ADR 0003; production uses FastCV/QNN | On track if capability/evidence exists; generic selection not finished |
| FR gallery/search | Proposed FW service | AI owns matching, encrypted gallery/key and Zvec per ADR 0004 | Correct new scope; hardware key/power-cut/performance still open |
| Push | Self commit + push | Commit per step, user pushes | Rule synchronized with the latest direction |

Portable does not mean every model/vendor is supported. A default build with Zvec does not
mean every flow uses FR; Zvec is an FR dependency, and the neutral targets must still remain
independent. 13 feature entries and a 16 source ceiling are not evidence that 13
features/16 cameras run for real.

## 2. Documentation contradictions fixed in this pass

- System architecture still said service, tracker, renderer and QNN direct did not exist.
- README reported base-ready, old test counts and default all-options-OFF even though Zvec
  is ON by default.
- ADR 0002 said direct SDK was not implemented; ADR 0004 kept a missing-library section
  that had already been resolved.
- The FW control/system plan still assigned matching/search to FW, contrary to the new FR
  decision.
- The cascade design said there was no package binding/decoder even though there was;
  separate primitive and integration.
- Rules still required automatic push; convention/delivery plan still mentioned host build.
- The capability matrix still reported FastCV/render as absent and called smoke a full
  qualification.
- Historical and current documents had no map, authority order and evidence vocabulary.

Older test reports keep their date/scope; do not repaint an old number as 97. A research
document describing an upstream API is not by itself a capability of the LACAI adapter. A
contract proposal is not by itself a released FW implementation. The document index helps
distinguish these categories.

## 3. Open architecture/code backlog

P0: correctness/ownership or blocking a usecase; P1: production generality/performance;
P2: maturity/tooling. Source references point to places needing review; they do not claim
every case has been reproduced by a test. Owner is a proposed role, not an assigned person
or acceptance date.

| ID | Level | Issue / evidence | Work required | Closure criteria / owner |
|---|---|---|---|---|
| A01 | P0 | FD→exact-frame alignment→EdgeFace is wired at source but has not run live/golden end-to-end | Run the camera with an approved golden capture; compare crop/input/embedding and correlation | Camera → embedding on the correct frame/epoch, drain and parity; AI runtime/BSP/model |
| A02 | P1 | The FastCV aligner still maps/copies/allocates ROI and tensor per face; QTI color differing from the neutral golden is undecided | Golden color/border; pool destination/input/output; profile and choose offload based on evidence | Correct input/crop/tensor, real completion and copy/CPU budget; AI/BSP/model |
| A03 | P0 | Cascade execute is synchronous on the service progress thread; an epoch change requires a graph restart | Bounded worker/completion state machine, fair admission, stale-result cleanup and graph epoch reconciliation | Multi-face does not block camera/output; stop/restart does not ACK early; runtime |
| A04 | P0 | **Handled**: protected encrypted gallery + revision CAS + reopen/rebuild; Zvec derived index private tmpfs, destroyed on close | Remaining: hardware-bound key, capacity/load benchmark | Restart/delete/replay on the correct revision; AI/FW |
| A05 | P0 | **Handled (loader)**: strict decoder_package loader rejects unknown/duplicate/out-of-range and cross-checks the catalog | Remaining: real model golden | Reject invalid before activation; model/app |
| A06 | P0 | DMA-BUF retention/reference count does not prove hardware completion | Trace input/crop/tensor owners, fence/cache/import and drain protocol | No early ACK/reuse in native fault tests; BSP/adapter |
| A07 | P0 | Compatibility camera/RTSP != released FW acceptance | Validate released wire/ring/control, disconnect, demand, ACK, permissions | FW end-to-end conformance report; FW/AI |
| A08 | P0 | **Handled (source+board)**: protected gallery, multi-template enrollment/search, matching, clean-restart recovery | Remaining: calibration, liveness/PAD, attendance output | Delete/restart correct; AI/FW/model |
| A09 | P1 | QNN built through the `qnn_backend_bundle` factory; FastCV is still constructed directly | Generic capability/policy selection for every backend | Swapping a backend does not change orchestration; platform |
| A10 | P1 | Production reference tracker/zone registration removed in the 2026-09-17 Plan 0 audit; only a portable IoU baseline is wired | Register actual production feature factories and qualify tracker/processor contracts | Do not accept fixture success or baseline IoU as feature/MOT acceptance; app/features |
| A11 | P1 | **Handled**: production config drops defaults, required + fail-closed CLI (CB-B); YOLO defaults removed | Remaining: version compatibility schema | Missing policy is rejected; app |
| A12 | P1 | Renderer/feature geometry still uses sources_.front(), limited decoder sharing | Per-source owners/output routes; settle supported concurrency | Multi-source with a different profile runs or is rejected before acquisition; app |
| A13 | P1 | The generic native capability factory is not fully used by production | Reconcile capability/entitlement/admission and effective state from real execution | Do not advertise unsupported async/shared/dynamic; runtime |
| A14 | P1 | The anchor decoder still creates observation strings/vectors per batch | Pool/reuse output, stable numeric IDs, atomic delivery without losing an owner | Allocation counters bounded/measured; perception |
| A15 | P1 | QNN output tensor allocation/client staging, compatibility render copy | Profile each copy; reusable registered memory when the BSP supports it | Numeric parity + completion + measured CPU/copies; adapter |
| A16 | P1 | Zvec mutex wraps vendor calls, allocates query/doc, FLAT fixed | Bounded search worker, explicit queue/deadline, measured index selection | Does not block camera, latency/recall budget; FR/index |
| A17 | P1 | 30 FPS person does not yet meet the target CPU; FR has no workload budget | Measure stage p50/p95/p99, faces/gallery sweep, thermal, FR jobs/s | 25–30 FPS and 15–25% CPU per the settled workload/CPU convention; perf |
| A18 | P1 | Model-specific capacity/threshold is not yet calibrated | Admit ROI rates per measurements, version model/preprocess/policy | Do not lower the 1s refresh to hide backlog; model/runtime |
| A19 | P0 | Sealed verified model bytes now close original-path replacement, but digest/package/backend signer is unverified | Review resolver/load authority, signed bundle/revision trust boundary and sealed-byte admission | Signed provenance and correct backend consumption; platform/FW |
| A20 | P1 | Async/shared-QNN/update is either new contract or unsupported | Implement only when a usecase needs it; probe real capability, no silent fallback | Native correctness + throughput/lifetime evidence; adapter |
| A21 | P1 | Recognition has no liveness from SCRFD+EdgeFace | Settle the anti-spoof requirement, PAD or an approved mechanism, with its own budget | Accuracy + spoof acceptance if the product requires it; product/model |
| A22 | P1 | Feature/gallery output authorization must still be connected to the identity payload | Revoke by revision, cache invalidation, delete pending matches | Do not publish a revoked/deleted identity; FW/features |
| A23 | P2 | The public bootstrapped ARM64 SDK is not yet a portable deployment package | Target/ABI validation, redistribution notices, offline packaging and upgrade | Reproducible install/rollback per target; build/release |
| A24 | P2 | **Normalized** during the clean-base docs pass: removed stale records, fixed contract/architecture/readme drift from code | Maintain the docs map and link check each PR | No two current statuses contradict each other; every module owner |
| A25 | P2 | The structural checker does not check literal semantics/ownership/ABI | AST/literal lint with an allowlist; CI eSDK/native evidence | Do not use a grep pass to declare hardcode cleanliness; tooling |

### Source anchors

- A01–A03: `src/runtime/scheduler/vqec_vision_cascade_frame_store.hpp`,
  `include/vqec/vision/ai/ports/vqec_vision_image_alignment.hpp`,
  `src/app/cascade/vqec_vision_cascade_coordinator.cpp`, `vqec_vision_cascade_graph_session.cpp`,
  `vqec_vision_runtime_executor.cpp` and `vqec_vision_multi_model_session.cpp`.
- A04/A16: `src/adapters/zvec/vqec_vision_zvec_embedding_index.cpp` (fresh collection,
  in-memory revision, fault gate, query allocation and mutex).
- A05/A09–A12: `src/app/platform/vqec_vision_production_platform.cpp/.hpp` (decoder selection,
  catalog/source geometry, constructors, reference factories and output composition).
- A14: `src/perception/detection/vqec_vision_anchor_distance_decoder.cpp` (output batch).
- A15/A20: `src/adapters/qualcomm/qnn/vqec_vision_qnn_engine.cpp` and
  [preprocessing report](../architecture/qualcomm_preprocessing.md).
- A19: `src/runtime/model_registry/` and production package loading require end-to-end review;
  existence of a secure resolver helper alone does not prove every load path uses it.

## 4. Processing order

1. Run live/golden FD-to-embedding and fix every contract/correlation mismatch before
   publishing identity.
2. Separate cascade from the service thread, pool/trim copies per profile; gallery
   recovery using synthetic vectors can be done independently. Per the
   [FR completion plan](../planning/face_recognition_completion_plan.md).
3. Connect enrollment/matching/attendance and the FW storage/event boundary, then release
   fault/soak.
4. Measure performance from the start; prioritize real hotspots. Backend factory/multi-source
   ownership is fixed when scope expands, do not hide limitations with docs or flags.
5. Async/shared context is not a prerequisite for synchronous correctness.

## 5. Issue closure rules

Record the source commit, tests/configuration, evidence location, limits, owner and status
`planned / source-delivered / logic-tested / board-smoke / accepted` per documentation_style.
`accepted` must have workload/contract-specific evidence and owner review. Docs only reflect
source that exists; A01–A25 close only when the corresponding evidence criterion is met.

## See also

- [Implementation status](implementation_status.md), [capability matrix](capability_matrix.md)
- [FR validation](../testing/face_recognition_production_validation.md)
