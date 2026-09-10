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
| R4 | Single vqec_ai_vision_applications main, config migration, legacy D-Bus, profile/reconnect supervisor | Runtime engineer | FW01–FW09, real UI first-viewer and restart scenarios pass |
| R5 | Packaging, permissions, install/rollback, board fault/performance/soak | Lead + FW owners | Jointly signed workload and release report |
| R6 | Shared perception + 13 feature/attribute scopes; traffic extension | Whole team | Model/dataset/business acceptance per feature |

Three-person team: lead also owns runtime/control; four-person team separates those roles.
These are dependencies and gates, not promises of dates without SDK/model availability.

## Current integration backlog (source inventory, 2026-09-10)

This table supersedes the chronological progress notes below. Existing source is not
evidence of a running system. Inventory used `rg --files src include packaging config`
and current CMake targets. The expanded eSDK build succeeds and its 57 unit/contract
binaries pass on QCS6490; live FW/model and release qualification remain open.

| Workstream | Present evidence | Remaining implementation and proof |
|---|---|---|
| Camera input | camera adapter, lease lifecycle, camera_session, graph pump source | Runnable composition, released FW RPC/FD/ACK tests, reconnect/profile supervisor, BSP completion signoff |
| Preview preparation | CPU surface/pool, encoder_window, bound encoder_preparation | Full input copy/transform into private NV12; bounded rendering with authorized scope-to-pixel binding |
| Encoder | Neutral backend port, correlated ledger, sealed input submission/drain helpers, event polling/handling and immutable AU owner | Actual backend pipeline, sealed owner retention, input/result callbacks, bus faults, EOS/drain and quarantined hardware ownership |
| Ring delivery | SDK options/header mapper, borrowed ring_sink, dispatch, ID issuer | Single-writer ownership, startup open, long-lived ring binding, ID publication, explicit migration policy and first-viewer RTSP test |
| AI control | Camera client only; fw_control is README | Released AI D-Bus server methods/signals, task config persistence, one control owner; no invented license grants |
| Runtime | Acquisition composition, bounded 1..16-source deployment loader, fixed-index activation snapshot, RAW-reference resolver, cadence, multi-model session/fan-out and bounded round-robin supervisor | authenticated FW registry/owner construction, per-source executors, vqec_ai_vision_applications entrypoint, preview/inference cadence split, board capability admission and health |
| Model integration | Model catalog/output loaders, deployment cross-validation, plan composition and artifact digest source | Approved signed model kits, immutable resolver, decode/geometry golden runner, concrete observations/tracking/attributes and authenticated model loading composition |
| Features | Thirteen feature directories and traffic contain READMEs | Shared perception contracts and feature lifecycle/dependency/entitlement integration; no claim features are implemented |
| Delivery | Packaging/config README placeholders | Yocto/IPK metadata, launcher compatibility, permissions, upgrade/rollback and package tests |
| Verification | Structural checker, pinned eSDK cross-build, 47/47 neutral QEMU baseline and 57/57 QCS6490 unit/contract smoke tests | Live SDK/model/FW ABI tests, board replay/fault/performance/soak evidence |

### Next cohesive implementation sequence

1. Implement a concrete encoder conforming to the existing neutral lifecycle contract
   and submit/poll/handle/drain helpers: retain actual input owners, report independent
   input/terminal-output events and preserve faulted work until proven quiescence.
   The interface and fake-backend test sources already exist; device conformance is open.
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

## Source inventory reference

See [implementation status](../development/implementation_status.md) for the consolidated
current module inventory and remaining boundaries. Earlier incremental slice notes have
been removed here to avoid presenting superseded missing-component claims as backlog.
No release gate is closed by this documentation refresh.

## Required integration scenarios

No viewers; first viewer from cold boot; multiple viewers; last viewer disconnect;
dead reader; ring missing/replaced; oversized AU; encoder stall; slow inference;
model disable during job; source FD release; camera restart; profile/FPS change;
old-epoch results; process stop; persist failure; repeated service restart;
concurrent main/sub/recording; rollback with old FW reader.

Measure preview FPS/latency, inference cadence, source hold time, queue age/drop reason,
copy bytes, encoder/AU size, FD/RSS/pool use and thermal behavior. Budgets require
FW/BSP agreement; source-only verification cannot pass these board gates.
