# FW-compatible replacement execution sequence

Baseline: [release contract](../contracts/fw_release_compatibility.md).
This sequence refines, not replaces, the 12-week/13-feature plan. Finish compatibility
before broadening features. No additional vendor backend is claimed implemented.

| Phase | Deliverable | Suggested owner | Exit gate |
|---|---|---|---|
| R0 | Filename migration, conventions, compatibility baseline, source-layout checks | Lead | All project source renamed, references resolve; no behavior change |
| R1 | Neutral preview/overlay/encoded-AU contracts, fixtures, pinned FW SDK agreement | Lead + runtime engineer | FW03–FW05 reviewed; no Gst/shared-ring ABI leaks into core |
| R2 | Bounded writable preview surfaces, renderer/encoder adapter, FW ring writer | Qualcomm engineer | Released RTSP reads deterministic synthetic overlays; no early ACK |
| R3 | Approved detector kit + decoder + geometry + tracking + cadence split | Model integration engineer | Golden results; preview independent of inference backpressure |
| R4 | Single cameraai_app main, config migration, legacy D-Bus, profile/reconnect supervisor | Runtime engineer | FW01–FW09, real UI first-viewer and restart scenarios pass |
| R5 | Packaging, permissions, install/rollback, board fault/performance/soak | Lead + FW owners | Jointly signed workload and release report |
| R6 | Shared perception + 13 feature/attribute scopes; traffic extension | Whole team | Model/dataset/business acceptance per feature |

Three-person team: lead also owns runtime/control; four-person team separates those roles.
These are dependencies and gates, not promises of dates without SDK/model availability.

## Current integration backlog (source inventory, 2026-09-07)

This table supersedes the chronological progress notes below. Existing source is not
evidence of a running system. Inventory used `rg --files src include packaging config`
and current CMake targets; no C++ build or device test has been performed.

| Workstream | Present evidence | Remaining implementation and proof |
|---|---|---|
| Camera input | camera adapter, lease lifecycle, camera_session, graph pump source | Runnable composition, released FW RPC/FD/ACK tests, reconnect/profile supervisor, BSP completion signoff |
| Preview preparation | CPU surface/pool, encoder_window, bound encoder_preparation | Full input copy/transform into private NV12; bounded rendering with authorized scope-to-pixel binding |
| Encoder | Correlated ledger, immutable AU owner | Actual backend pipeline, sealed owner retention, input/result callbacks, bus faults, EOS/drain and quarantined hardware ownership |
| Ring delivery | SDK options/header mapper, borrowed ring_sink, dispatch, ID issuer | Single-writer ownership, startup open, long-lived ring binding, ID publication, explicit migration policy and first-viewer RTSP test |
| AI control | Camera client only; fw_control is README | Released AI D-Bus server methods/signals, task config persistence, one control owner; no invented license grants |
| Runtime | Acquisition composition, bounded 1..16-source deployment loader, fixed-index activation snapshot and bounded round-robin session supervisor | authentication/FW RAW-source resolution, per-source executors, cameraai_app entrypoint, preview/inference cadence split, board capability admission and health |
| Model integration | Model catalog/output loaders, deployment cross-validation, plan composition and artifact digest source | Approved signed model kits, immutable resolver, decode/geometry golden runner, observations/tracking/attributes, model loading lifecycle |
| Features | Thirteen feature directories and traffic contain READMEs | Shared perception contracts and feature lifecycle/dependency/entitlement integration; no claim features are implemented |
| Delivery | Packaging/config README placeholders | Yocto/IPK metadata, launcher compatibility, permissions, upgrade/rollback and package tests |
| Verification | Structural checker plus C++ test source | Executed host tests, pinned cross-build, SDK ABI tests, board replay/fault/performance/soak evidence |

### Next cohesive implementation sequence

1. Complete the output backend lifecycle contract: actual memory owner handoff,
   terminal completion, no-output proof and drain/fault retention. Couple it to the
   existing encoder_window rather than introducing another uncorrelated job counter.
2. Implement the reviewed Qualcomm encoder path and private surface population/rendering
   boundary; preserve original frame/PTS and released H264 metadata. Any CPU path must
   be explicitly labeled and budgeted, not described as vendor hardware acceleration.
3. Compose one preview session: demand -> prepare/render -> commit/push -> completions
   -> authorize/dispatch. Ring exists before demand; no viewer suppresses preview
   submissions but does not disable inference. Keep ring layout stable across profiles.
4. Integrate sessions with the process-level source/profile supervisor and released
   control/launcher behavior. Load one authenticated deployment revision, resolve model
   catalog/FW RAW-source references, admit total memory, then instantiate 1..16 source owners. Bind
   generation at creation; never retag queued outputs.
5. Execute synthetic end-to-end FW reader tests before inserting model-dependent
   decoder/tracker behavior. Then proceed through R3-R6; they remain required scope.

Literal cleanup remains a cross-cutting review requirement, not a substitute for this
integration sequence. Shared limits require one semantic owner; vendor properties and
wire values require source provenance; runtime tuning requires validated configuration.
Structural PASS cannot close any runtime, model, feature or release acceptance gate.

## Historical source progress notes

Latest hardening: ring wrapper now distinguishes SDK mapping counters from dispatch
binding identity, and exposes tested-in-source transactional legacy header mapping.
Runtime generation issuance and live ring/restart tests remain open; no board claim.

Latest FW adapter slice: optional borrowed-ring sink wrapper source and closed-ring guard
tests delivered. No live ring creation/write test, SDK linkage qualification or encoder
runtime yet. See architecture/fw_ring_sink.md; earlier missing-wrapper notes are historical.

Latest: synchronous encoded dispatch and fake-sink test source wire the pure policy
gate into a delivery helper. No real ring/hardware output, signed policy provider,
renderer-scope binding or board qualification. Tests are not executed.

Latest output boundary: owned H264 copy and synchronous encoded_sink interface added
with source-only tests. Ring SDK implementation, bounded output dispatch, actual encoder
input/completion handoff and board verification remain open; no live delivery claim.

Latest composition slice: reserve/acquire/rollback/cancel helper and portable contract-test
source delivered, not executed. Real encoder owner handoff, rendering, ring/demand SDK
and hardware validation remain pending. See architecture/encoder_preparation.md.

Latest pool slice: bounded reusable CPU NV12 pool/lease source and unit tests delivered,
not executed. Encoder job owner wiring, hardware renderer/encoder, ring port/SDK,
runtime demand and global pool-generation admission remain open. R1 is not complete.

Latest: encoder_window source provides bounded jobs/aggregate byte reservation, demand
gating, exact AU correlation and separate input/result completion via submission_window.
Not executed. Pool, backend input ownership/completion integration, ring sink/demand
transport and FW SDK agreement remain pending. Earlier progress notes below are historical.

Additional source: move-only CPU NV12 writer/shared sealed reader with per-allocation
budget. Pool/global admission, encoder job ledger and ring/demand interfaces remain open.
See architecture/preview_surface.md. Test source is not executed; R1 remains incomplete.

Progress: preview metadata/borrowed-AU validators and unit-test source are delivered,
not executed. Surface ownership, encoder completion, ring sink/demand interfaces and
FW SDK agreement remain open; R1 is not complete. See preview_contract.md in architecture.

- Register neutral contracts: overlay commands in source pixels + explicit transform,
  bounded text/primitive counts, source epoch/frame/PTS, result freshness and policy revision.
- Define preview surface owner and encoder input-completion separately from encoded result.
- Define encoded AU view/owner, codec/framing, exact correlation, parameter sets and limits.
- Define ring sink port and demand snapshot; SDK layout remains private to fw_output adapter.
- Test invalid geometry, stale epoch/revision, AU limits and no-demand admission with fakes.
- Do not implement model decoding without an approved model kit/output mapping.

## Required integration scenarios

No viewers; first viewer from cold boot; multiple viewers; last viewer disconnect;
dead reader; ring missing/replaced; oversized AU; encoder stall; slow inference;
model disable during job; source FD release; camera restart; profile/FPS change;
old-epoch results; process stop; persist failure; repeated service restart;
concurrent main/sub/recording; rollback with old FW reader.

Measure preview FPS/latency, inference cadence, source hold time, queue age/drop reason,
copy bytes, encoder/AU size, FD/RSS/pool use and thermal behavior. Budgets require
FW/BSP agreement; source-only verification cannot pass these board gates.
