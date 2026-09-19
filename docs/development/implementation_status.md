# Implementation status — 2026-09-19

2026-09-19 spatiotemporal metadata implementation (Plan 2: **ACCEPTED**):

- M01–M05 now deliver neutral version 1 frame/track/trajectory, association, episode, aggregate,
  live snapshot/delta and typed-query contracts; checked packed trajectory codec; SQLite WAL
  catalog plus time shards; stable snapshots; quota rejection; atomic outbox; seal/recovery;
  exact gap-aware spatial verification; revisioned cross-camera associations; episode correction;
  contribution retract and materialized latest-corrected rollups.
- A bounded service owns one blocking writer thread, projection batches and bounded live state.
  Producers receive explicit `resource_exhausted`; query work is serialized outside frame/DSP
  threads. A validated `--metadata-profile` composes it into the production executable, and
  background persistence errors become required-runtime health/exit failures.
- Machine-readable version 1 coverage fixes D01–D18, Q01–Q30, all S01–S18 security
  mappings, nine traffic extension profiles and five outcome classes per query. The checker
  cross-validates the stable usecase identities against the three-team registry.
- The neutral contract and SQLite WAL adapter validate append-only revisions, commit each
  fact with its outbox rows, reject conflicting retries, authorize scopes and projections,
  preserve snapshot/keyset paging, evaluate source coverage and recover after reopen. Q02
  checks attribute validity at passage time and requires both attribute and trajectory scopes.
- The approved expanded eSDK suite passes 142/142 under QEMU. The isolated QCS6490 benchmark
  (`c9945697…a22190`) ran `FULL` sync for 300 seconds beside the full AI workload: 45,081 durable
  records, 9,952 queries and zero reject/write/query/oracle failure across 27 security/traffic
  profiles. Query p50/p95/p99 was 2.688/12.454/18.546 ms; metadata used 30.110% of one core,
  37,120 KiB maximum RSS and 32,567,296 bytes. AI APP averaged 12.88% of one core. H.264 preview
  measured 30.000 packet-PTS FPS and the current overlay contact sheet passed visual review.
- The final composed service at commit `7e8538b9f3e15de4fc9da102e0efbf1ed419090b` has SHA-256
  `d02770e…a63`; its profile digest is
  `79cfc9…94ce`. Over an exact 300-second interval it averaged 13.16% of one core and 345,497 KiB
  RSS, with H.264 1920x1080 preview at 30.124 FPS. Metadata committed 26/26 work items, including
  18 unique trajectory chunks, with zero reject/failure. Drain reported `stopped=true`,
  `first_error=0`, `cascade_failed=0`.
- Target-native service/runtime/store regression passes. The fault suite recovers after SIGKILL
  and after an actual full tmpfs, rejects a zero-overwritten detail DB, cancels queries and keeps
  pending outbox data through retention. Conflicting asynchronous writes now poison health rather
  than being hidden in counters.
- Packed SQLite shards are the accepted v1 edge choice; Parquet is center interchange/future cold
  tier. ADR 0009 is accepted. Kafka transport is Plan 3 and concrete usecase producers/golden are
  Plan 5; unsupported Q01–Q30 capabilities remain explicit rather than implied by P2 closure.

2026-09-18 three-team contract baseline (Plan 1: **ACCEPTED**):

- A normative machine registry now fixes C01–C10 schema owners, authoritative producers,
  consumers, v1 bounds, clocks/units, lifetime/completion, delivery, security, required fields
  and error taxonomy. S01–S18 have stable IDs and truthful dependency/status records.
- The standard-library checker validates the registry, 20 valid/rejected baseline cases,
  producer receipt envelopes and six negative mutations. Producer receipts bind exact registry,
  artifact/report SHA-256, case coverage, expiry/deviation and an AI APP-owned disposition.
- Approved eSDK/QEMU neutral configuration passes 100/100 CTest. QCS6490 `.98` runs the same
  registry/self-test checker successfully; this is contract-pack portability evidence, not
  released-FW, hardware-completion or model-quality acceptance.
- The clean neutral build also exposed that mandatory hardware admission linked nlohmann JSON
  while dependency discovery was incorrectly gated by optional catalog loaders. CMake now resolves
  the pinned installed-or-vendored JSON target in every configuration; no runtime policy changed.
- BSP+FW C01/C02/C10 target receipts and AI Model C03 golden/quality receipts remain external
  release gates. AI APP does not fabricate those owners' attestations.

See the [integration registry](../contracts/integration_contract_registry.md) and
[closed plan](../planning/architecture_improvement/contract_and_team_scope.md).

2026-09-17 subsequent Plan 0 corrective verification (AI APP development: **UNBLOCKED**):

- Corrective source candidate: local commit `5237c18`; exact binary/profile hashes in review.
- Production scoped feature projection is now passed through reconciliation with exact
  model slot, attribute scopes and immutable config/policy revisions.
- Production cascade is wired to a bounded worker, preserving frame/geometry/observations
  and captured output revision. FR-only binds its policy gate; clean shutdown joins workers
  and releases completed pump primary owners only after graph reconciliation.
- Admission sums source tensor budgets and rejects preview pool CLI/deployment mismatch.
  The observed `.98` profile admits one source, not the 16-source schema ceiling.
- Approved eSDK/QEMU: 127/127. QCS6490 `.98`: native 121/121; live exit 0,
  `service stopped=true`, `first_error=0`, 282 cascade tasks/embeddings, no cascade failure
  or stale-policy FR denial. Ring reader probe: H.264 1920x1080 30/1.
- Status is board-smoke, not product acceptance. Independent BSP+FW/AI Model reviews,
  released-FW coexistence, golden results and sustained DDR/thermal/resource measurement
  remain mandatory downstream gates. See the
  [exact-candidate review](production_composition_foundation_review.md).

Earlier 2026-09-17 audit (historical decision: **BLOCKED**, superseded above):
- F01: source/usecase gate projection no longer combines desired, entitlement and admission
  across unrelated associations. A complete immutable feature/model/attribute/policy record
  and trusted provisioning remain open.
- F02/F05: Qualcomm composition rejects reference tracker/feature contracts, uses an
  explicit portable bounded IoU baseline and distinct per-source/model graph/processor
  bindings. Production feature catalogs without real factories fail closed; feature/MOT
  quality is not accepted.
- F03: renderer accepts only prepared, scoped overlays; the service constructs the
  prepared payload for the current source frame. FW demand/PTS/revoke conformance and a
  generic encoded-dispatch vertical are not established.
- F04: model resolver copies verified bytes to a sealed, retained `memfd` and verifies the
  copy before QNN opens its descriptor path. This closes original-path replacement for
  model bytes, not signature/provenance for expected digest, package or backend libraries.
- F06: cascade pending alignment no longer completes the retained frame early. Cascade
  execution is still synchronous on the progress thread; worker/recovery/bounded stop
  remain open.
- F07: production has a bounded in-memory event seam; acceptance and explicit discard are
  not downstream delivery. Durable transport/ACK is Plan 3.
- F08: composition rejects a missing/invalid hardware profile; Qualcomm requires an
  explicit file with target/revision/measurement reference. Fake/reference use an identified
  fixture. No signed measured board envelope exists; DDR/thermal remain estimates and
  sealed model bytes/FR index require aggregate accounting.
- Current corrective source passes 126/126 eSDK/QEMU CTest (2026-09-17); the earlier
  125/125 eSDK and 118/118 board figures were historical pre-audit runs, not target
  acceptance for these commits. BatchMode SSH to `.98` was denied, so no post-fix native
  run was performed. See the
  [Plan 0 review](production_composition_foundation_review.md) for gate-level evidence and
  required three-team approvals.

2026-09-16 clean-base CB-E3 update: two more phases were extracted from `run_generation`.
`vqec_vision_ai_appl_svcmn_build_model_activations` now owns the per-source/model activation
loop (borrowing the platform owners that still outlive the bundle), and
`vqec_vision_ai_appl_svcmn_append_fr_policy_rules` removes the duplicated FR output-scope
rule construction shared by the feature-wiring and FR-only policy paths. `run_generation`
dropped to 1005 lines; behavior and exit codes are unchanged. The remaining run loop,
platform-owner setup and recognition/feature setup blocks stay in `service_main.cpp`
because their owners must outlive the runtime bundle; further reduction needs the larger
"service runtime owner struct" redesign rather than a local extraction. eSDK/QEMU suite
passes 123/123. Board `.98`: H.264 1920x1080 30/1 (177 frames in 6 s), `first_error=0`,
D-Bus 5/5, cascade `embedded=4 cascade_failed=0`.

2026-09-16 clean-base CB-G/CB-T update: CI and board evidence truth.
- `host-sanitizers`, `clang-tidy`, `fuzz` and `esdk-neutral` now configure with
  `VQEC_VISION_AI_ENABLE_ZVEC=OFF`, so a clean runner no longer hits the Zvec `FATAL_ERROR`
  when the untracked SDK is absent; `esdk-expanded` runs `vqec_vision_prepare_zvec.sh`
  before configure.
- Added `tools/board/vqec_vision_board_native_tests.sh`, the reproducible native board runner.
  It supplies the fixtures two device-free tests need: the staged `manifests/models` tree
  for `decoder_package_test`, and a non-tmpfs scratch path plus a current-UID mode-0700
  tmpfs directory for `zvec_embedding_index_test`. The Zvec test now reports its failing
  source line. On `.98` the cross-built native suite is **117/117 passed**; the earlier
  two "failures" were missing-fixture usage exits, not code faults.
- `tests/unit/README.md` no longer carries stale fixed test counts and links the runner.

eSDK/QEMU suite passes 123/123.

2026-09-16 clean-base CB-E2 update: two cohesive startup/report phases were extracted from
`run_generation`. `vqec_vision_ai_appl_svcmn_resolve_startup` now owns catalog load, usecase
composition, the idle no-source loop, FR/enrollment argument validation, model package
resolution and fail-closed platform selection, returning a `service_startup_resolution`.
`vqec_vision_ai_appl_svcmn_report_and_decide` owns the metrics/report print and the exit-code
policy. `run_generation` dropped from ~1272 to 1087 lines and no longer mixes startup
validation and exit policy with the run loop. Behavior is unchanged (same exit codes, same
smoke tests). The remaining run loop and the recognition/feature setup blocks are still in
`service_main.cpp`; further decomposition remains open. eSDK/QEMU suite passes 123/123.
Board `.98`: native 115 (two known environment-fixture failures) and a live compatibility
run published H.264 1920x1080 30/1 (167 frames in 6 s) with `first_error=0`, D-Bus 5/5 and
cascade `embedded=4 cascade_failed=0`.

2026-09-16 clean-base CB-F update: small correctness/security fixes.
- Removed the contracts -> ports layering inversion in `vqec_vision_recognition.hpp`: the
  embedding-index data types and limits (`embedding_metric`, `embedding_index_config`,
  `embedding_gallery_record`, `embedding_match`, `embedding_search_result`,
  `embedding_index_limits`) moved from the index port into `contracts/vqec_vision_embedding.hpp`;
  the port now declares only the interface, and `recognition_session.hpp` includes the port
  directly.
- Zvec `map_error`/`create_collection` are no longer `noexcept`, so an allocation failure
  propagates instead of calling `std::terminate` inside a `noexcept` function.
- `usecase_control_GetStatus` rejects a status `reason_` over
  `usecase_control_limits::g_max_reason_bytes` rather than emitting an over-bound D-Bus reply.
- The enrollment image path authorizer rejects the filesystem root `/` as an allowed root.
- Added the missing `src/adapters/storage/README.md`, documenting the encrypted-gallery
  contract and the tracked gap that the GCM AAD does not yet bind gallery/file identity.
- Verified `gst_bin_add_many` returns `void` in GStreamer 1.22, so the earlier "unchecked
  return" finding was not applicable.

eSDK/QEMU suite passes 123/123. Board `.98`: native 115 (two known environment-fixture
failures) and a live compatibility run published H.264 1920x1080 30/1 (179 frames in 6 s)
with `first_error=0`, D-Bus 5/5 and cascade `embedded=5 cascade_failed=0`.

2026-09-16 clean-base CB-E update (first slice): startup option parsing was extracted from
the 2203-line service main into a testable unit. `vqec_vision_service_options.{hpp,cpp}`
owns `parsed_arguments` and `vqec_vision_ai_appl_svopt_parse`, built as the
`vqec_vision_ai_service_options` library and covered by the `service_options_parsing`
CTest. `service_main.cpp` shrank to 1923 lines; the step-interval default now has one named
owner. Behavior is unchanged: the executable passes the same smoke tests. Decomposing the
remaining `run_generation` loop into phase builders is still open. eSDK/QEMU suite passes
123/123. Board `.98`: native 115 (two known environment-fixture failures) and a live
compatibility run published H.264 1920x1080 30/1 (181 frames in 6 s) with `first_error=0`
and D-Bus 5/5.

2026-09-16 clean-base CB-D update: the legacy `secondary_inference` contract and
`secondary_inference_scheduler` were removed (ADR 0006). They duplicated the delivered
retained-frame `cascade_coordinator` and left two cascade-scheduling designs in the tree.
`inference_worker` and `recovery_controller` are retained as explicit reserved modules with
named future wiring points and gates; they remain non-integrated and must not be presented
as delivered capability. CMake targets, CTest registration, naming registry and the
cascade/FR planning references were updated together. eSDK/QEMU suite passes 122/122 after
the removal.

2026-09-16 clean-base CB-C update: duplicated neutral validation now has one owner.
`vqec_vision_identifier.hpp` gained the shared lowercase SHA-256 hex check, used by the
model catalog, model package and model-update validators; `model_package` no longer carries
its own diverging identifier loop and owns its ceiling as
`model_package_limits::g_max_identifier_bytes`. A new header-only
`vqec_vision_nv12_geometry.hpp` (`vqec_vision_ai_cntr_nvgeo_*`) owns the packed-NV12
even-dimension rule and byte formula, and color, deployment, encoder contract/window,
preview surface/pool/contract, model catalog constraints and inference plan now call it
instead of re-deriving it. The `model_io_manifest` input/output label is included in error
messages instead of being discarded. A `nv12_geometry_contract` CTest pins the helpers.
eSDK/QEMU suite passes 123/123. Board `.98`: native 114/116 (same two environment-fixture
failures) and a live compatibility run published H.264 1920x1080 30/1 (181 frames in 6 s)
with `first_error=0`, D-Bus 5/5 and cascade `embedded=5 cascade_failed=0`.

2026-09-16 clean-base CB-B update: the Qualcomm production executable no longer carries
deployment defaults. `production_platform_config` starts empty and `configure()` rejects
missing backend/system library, camera socket dir, NV12 format value,
tracker contract, event schema id/version or consumer id prefix. `service_main` gained
`--qnn-backend-library`, `--qnn-system-library`, `--model-root`, `--tracker-contract`,
`--event-schema-id`, `--event-schema-version` and `--consumer-id-prefix`; the previous
`/usr/lib/libQnnHtp.so`, `/usr/lib/libQnnSystem.so`, `/opt/vqec/models/`, `/run/camera_ai`,
NV12 `23` and `reference.*` defaults are gone. The device-free reference/fake path uses
named non-existent fixture paths, and the duplicated `"fw.dmabuf.v1"`/`"qcom.dmabuf.v1"`
literals were replaced with the named constants. A new
`service_production_qualcomm_requires_config` CTest fails closed when qualcomm production
omits this configuration. eSDK/QEMU suite passes 122/122. Board `.98`: production qualcomm
ran with explicit configuration (`first_error=0`, D-Bus 5/5, cascade embedded=10
cascade_failed=0) and published H.264 1920x1080 30/1 (178 frames in 6 s).

2026-09-16 clean-base CB-03 update: the runtime latency metric no longer mixes clock
domains. `submission_ticket` now carries `submitted_steady_ns_` (monotonic time captured at
reserve) separately from `pipeline_pts_ns_` (vendor/pipeline PTS for encoder correlation),
and the executor computes its metric from reservation to result routing in the steady
domain only. The metric is renamed `route_latency_*` because it is not camera-to-output
latency. Before this change the executor subtracted the pipeline PTS from the steady clock,
producing nonsensical multi-second values. Board `.98`: `route_latency_avg_us=31186`
(min 15521, max 46779) across a live compatibility run whose RTSP output probed H.264
1920x1080 30/1 (181 frames in 6 s), D-Bus FR transitions passed and `first_error=0`.
eSDK/QEMU suite passes 121/121.

2026-09-16 clean-base CB-02 update: the owned QNN engine now supports a defined reload
path. `release_model()` frees graphs, context, model library, output workspace and
registered buffers while keeping the backend/device open, and `qnn_inference_graph::unload`
calls it, so `release_model` + `prepare` can compose a new model library on the same engine.
`prepare` rejects any non-v2 tensor description; `execute` re-checks `tensor.version`
before touching the v2 union. The local ABI mirror of `qnn_model_graph_info` now carries
`static_assert` field-offset guards. The ION-registered output path is documented as
DMA-write plus one bounded copy into the neutral blob, not end-to-end zero-copy.
Board evidence on `.98`: `qnn_engine_smoke --reload-cycles` reloaded YOLOv8n-person
(2 outputs) and SCRFD (9 outputs) and executed again with unchanged tensor identity;
a live compatibility run through the canonical v5 ring published H.264 1920x1080 30/1
(167 frames in 6 s) with `cascade_failed=0`. eSDK/QEMU suite passes 121/121.

2026-09-16 throughput/lifetime update: production can select an explicit HTP
`low_latency` vote and persistent per-model workers while keeping orchestration behind
neutral ports. NV12 color conversion and RGB8-to-UFIXED16 quantization use byte-exact
lookup tables; cascade embedding reuses an activation-sized quantization/input workspace.
Completed model slots now transfer their retained RAW-frame owner to the single result
consumer. This fixes a live three-buffer compatibility-camera deadlock found during soak.
The final eSDK/QEMU suite passes 120/120 and affected ARM64 tests pass on `.98`. Sustained
hot-board samples remain roughly 20.8--24.6 FPS for FD+FR, so the 25--30 FPS and thermal
acceptance gate is still open; see [FR validation](../testing/face_recognition_production_validation.md).

2026-09-16 runtime-control validation: service-owned D-Bus desired-plan replacement now
stops scheduling/output, drains/unloads old owners and starts the effective candidate in
the same process. Publication waits for source-session readiness; recovery-required blocks
replacement. `.98` passed both → person → all-off → FR → both with native model-library
residency checks, all-method sender rejection, multi-template file enrollment, terminal
retry, payload conflict and deletion preserving the original gallery. eSDK/QEMU: 120/120;
native logic suite: 114 cases. See [FR validation](../testing/face_recognition_production_validation.md)
for reproducibility and release blockers, including swap/crash-dump/hardware-key protection,
durable receipts, signed provisioning, asynchronous control and performance qualification.

Current source inventory, checked against `src/`, public headers, test sources and
`CMakeLists.txt`. This replaces the incremental delivery log: earlier slice limitations
must not be interpreted as the current missing-feature list.

**Status:** accepted — delivery/evidence authority for the current AI APP workload.

## Current summary and evidence authority

Current delivery/evidence claims live in this file and
[capability matrix](capability_matrix.md). Raw dated board/QEMU runs live in
[QCS6490 target](../testing/qsc6490_board.md) and [eSDK emulation](../testing/esdk_emulation.md)
and are not restated here.

Delivered and current:

- Qualcomm production composition: one RAW acquisition drives 1..16 sources and up to 16
  models per source; the owned QNN engine composes/finalizes/executes SCRFD, YOLOv8n-person
  and EdgeFace on HTP, byte-identical to `qnn-net-run` for the synchronous path.
- Cascade: retained exact-frame FastCV alignment and EdgeFace embedding.
- Recognition: durable AES-256-GCM protected gallery, revision CAS, exact/Zvec index rebuilt
  at the durable revision, multi-template enrollment/search through D-Bus.
- Output: AI-owned overlay + H.264 encoded into the released FW v5 ring; compatibility
  camera/reader probes publish H.264 1920x1080 30/1.
- Camera lease Start/Stop, usecase desired-plan control, feature activation/fan-out/stage,
  output gates, preview pool and encoder ledgers are source-delivered.

Current evidence: source `c01b748` passes 162/162 expanded eSDK/QEMU CTest. On board `.102`, the
S04/App Manager/output candidate passes nine focused native tests and ten five-second
disable/enable cycles. The full current workload publishes H.264 1920x1080 at 30.124 FPS; its
five-minute steady process CPU is 10.56% of one logical core with RSS +8 KiB and 23 threads.
The 30-second cold-start average is 13.36%, with one 86% sample attributed by `perf` to
`libQnnHtpPrepare.so::GraphPrepare` after the first source frame. No QNN HTP library is mapped
while App Manager and camera are absent. This is board-smoke, not model-quality, released-FW,
leak-free or product thermal acceptance. Exact evidence and limitations are in
[S04 validation](../testing/fire_smoke_product_slice_validation.md).

Open release gates (not delivered): released-FW camera/ring/RTSP/evidence conformance, hardware
DMA completion and BSP recovery, golden/model accuracy calibration, attendance/liveness,
hardware-bound gallery key, production trust rotation, package operation journal/content-store
update/rollback, QNN context-binary cold-start qualification, async/shared QNN execution,
multi-vendor backends and the remaining feature packages. See
[alignment review](architecture_alignment_review.md) and
[capability matrix](capability_matrix.md).

## Current architecture

AI Camera and AI Box expose the same FW RAW NV12/FD lease boundary. Deployment supplies
unique opaque `raw_source_ref` values and exact dimensions/rational FPS. Sensor/ISP,
RTSP discovery/credentials/demux/decode and decoder allocations belong to FW.

Application orchestration depends on `raw_source_port`, `inference_graph_port` and
`source_session_port`. One `multi_model_session` acquires one source and fans one frame
owner out to its due graphs. `multi_source_supervisor` advances pre-composed sessions;
it does not construct them. Arrays support 1..16 sources and 1..16 models per source as
software ceilings. Actual admission must account for lower backend limits, including
one outstanding job per Qualcomm graph and four slots per graph-retention domain.

AI owns private preview pixels, overlay, H264 encoding and FW ring production. The current
Qualcomm path uses generic v1 cDSP image transform and dense decode, an AI-owned rpcmem pool,
cDSP `overlay_compose`, direct
`v4l2h264enc` DMA-BUF import and the released ring layout. A bounded latest-wins preview mailbox decouples camera output cadence from
each model's configured inference cadence. FW owns
RTSP/UI/recording and persistent evidence/search. Released preview routing remains limited
to detect0/detect1; multi-source inference does not imply 16 independent preview outputs.

## Source delivered and remaining integration

Paths in this table are relative to the repository root; source stems use `vqec_vision_`.

| Area / source owner | Delivered source behavior | Remaining boundary |
|---|---|---|
| `src/core/`, `include/vqec/vision/ai/contracts/` | Status, explicit frame/tensor metadata, inference/source-binding validation, source-frame-correlated submission ledger, typed landmark/embedding and observation/feature-event validation, output policy, preview and encoder contracts | Additional decoder/tracker/feature algorithms; device completion evidence |
| `include/vqec/vision/ai/contracts/inference/vqec_vision_model_decoder.hpp` | Neutral model-decoder port keeps model output identity and expected frame key at the tensor-to-observation boundary; contract test source is registered in CMake | Concrete detector decoders, model-specific geometry/NMS and tensor-to-observation implementation |
| `src/perception/detection/vqec_vision_model_decode_stage.cpp` | Transactional portable decoder stage validates decoded observations, geometry binding and exception containment before publication; the package-configured YOLOv8 decoder is live on QCS6490 | Anchor-distance/landmark decoder and additional model contracts |
| `src/perception/detection/vqec_vision_model_decoder_registry.cpp` | Bounded activation-time mapping from catalog decoder contracts to non-owning decoder ports; validates output-manifest identity through the selected decoder | Trusted decoder loading, lifecycle ownership and concrete model implementations |
| `src/perception/detection/vqec_vision_tensor_reader.cpp` | Bounded tensor lookup, manifest shape/value-count validation and shared typed scalar/dequantization for model decoders | Model-specific tensor semantics and postprocess |
| `include/vqec/vision/ai/ports/perception/vqec_vision_tracker.hpp`, `src/perception/tracking/vqec_vision_tracker_registry.cpp` | Neutral serialized tracker port plus bounded activation-time factory registry with distinct per-source/model owners | Concrete association/tracking implementation |
| `src/perception/tracking/vqec_vision_tracking_stage.cpp` | Transactional detection-to-tracker coordinator with monotonic-time enforcement, epoch reset and ambiguous-failure isolation | Concrete association/tracking implementation and replay qualification |
| `src/perception/embedding/vqec_vision_exact_embedding_index.cpp`, `src/adapters/zvec/vqec_vision_zvec_embedding_index.cpp` | Neutral revision-pinned index, exact cosine reference backend and Zvec C API adapter with explicit rebuild of disposable existing collections after authoritative load | Bounded worker, capacity/performance/thermal qualification and atomic generation publication under concurrent queries |
| `include/vqec/vision/ai/contracts/perception/vqec_vision_face_gallery.hpp`, `include/vqec/vision/ai/ports/perception/vqec_vision_face_gallery_store.hpp`, `src/adapters/storage/identity/vqec_vision_encrypted_face_gallery_store.cpp` | Bounded authoritative snapshot plus AI-owned AES-256-GCM persistence, private files, interprocess lock, disk CAS and atomic fsync+rename | Hardware-bound key provider, schema migration, power-cut/backup/restore and board qualification |
| `src/perception/embedding/vqec_vision_recognition_session.cpp`, `src/perception/embedding/vqec_vision_face_enrollment_controller.cpp`, `src/adapters/fw_control/enrollment/vqec_vision_face_enrollment_dbus.cpp` | Persistent multi-template recognition, revision-CAS enrollment/remove, exact-frame label correlation, authenticated DBus and gallery health/status without biometric disclosure | Durable request receipts, temporal/liveness policy, display-name metadata authorization, attendance output and calibration |
| `src/perception/attributes/vqec_vision_attribute_reader.cpp` | Exact tracked-attribute lookup with schema/version identity, bounded values, source-clock freshness and borrowed-result lifetime | Concrete typed attribute producers, temporal fusion and calibration |
| `include/vqec/vision/ai/contracts/features/vqec_vision_feature_event.hpp`, `include/vqec/vision/ai/ports/features/vqec_vision_feature_processor.hpp` | Config-bounded neutral feature events and serialized algorithm port with source/frame/config/schema provenance | Effective-state manager, concrete rules, output router, replay and delivery qualification |
| `include/vqec/vision/ai/contracts/features/vqec_vision_feature_catalog.hpp`, `src/core/features/vqec_vision_feature_catalog.cpp` | Versioned feature integration metadata validates processor/configuration contracts, model roles, attribute freshness and bounded temporal/event resources against the model catalog | Authenticated loader, per-source desired/entitled activation, processor registry and concrete feature packages |
| `src/runtime/feature_manager/vqec_vision_feature_stage.cpp` | Activation-validated transactional processor coordinator with monotonic time, strictly increasing epoch reset and ambiguous-failure isolation | Concrete package factories, output router and replay qualification |
| `src/runtime/feature_manager/vqec_vision_feature_processor_registry.cpp` | Bounded compiled-in factory registry resolves processor contracts, validates schema/revision-bound configuration and creates distinct transactional owners | Concrete package factories, authenticated configuration and activation owner graph |
| `src/runtime/feature_manager/vqec_vision_feature_catalog.cpp` | Optional strict bounded JSON loader preserves output on parse/validation failure and feeds the neutral feature catalog validator | Authenticated feature package/config resolution and activation owner graph; eSDK sysroot currently lacks nlohmann_json 3.12.0 for this optional target |
| `src/runtime/feature_manager/vqec_vision_feature_activation_manager.cpp` | Validated cold-path reconciliation of desired, entitlement, resource and model-dependency gates; explicit effective states, per-association processor/stage ownership and slot-to-catalog mapping accessor | Runtime fan-out/pipeline owner composition, authenticated catalog/configuration source, concrete package factories and output policy |
| `src/outputs/events/vqec_vision_feature_event_dispatch.cpp`, `src/app/service/output/vqec_vision_evidence_service.cpp`, `src/adapters/storage/evidence/vqec_vision_sqlite_evidence_outbox.cpp`, `src/adapters/fw_output/vqec_vision_evidence_uds_client.cpp` | Validates exact field scopes, rechecks captured/retry policy, commits a bounded durable command, retries over version 1 seqpacket UDS and reconciles terminal receipts | Released-FW durable inbox, prebuffer/media receipt and C07 owner acceptance |
| `src/runtime/lifecycle/vqec_vision_deployment_config.cpp` | Optional strict bounded deployment JSON loader; schemas/examples; pure deployment validation in core | Authenticated configuration activation and service lifecycle |
| `src/runtime/model_registry/` | Optional model catalog/output manifest/package-registry loaders and bounded OpenSSL SHA-256 stream comparison; the package registry gives every catalog model an exact package/artifact binding | Signature verification, trusted immutable path opening and decoder lookup |
| `src/runtime/admission/vqec_vision_activation_snapshot.cpp` | Fixed numeric source/model indices tied to immutable deployment/catalog revisions; assignment/context counts and resident estimate | Measured board-wide accelerator/memory/encoder/thermal admission and owner construction |
| `src/core/features/vqec_vision_usecase_activation.cpp`, `src/runtime/feature_manager/vqec_vision_usecase_config.cpp`, `vqec_vision_usecase_control_manager.cpp`, `src/adapters/fw_control/usecase/vqec_vision_usecase_control_dbus.cpp` | Transactional pre-load gate composition, strict authenticated-startup parsing, bounded CAS/idempotent desired-plan ownership and exact D-Bus v1 adapter; service startup filters before graph preparation, preserves shared roots and idles without camera/model load | Signed entitlement verification, durable desired receipts, nonblocking preparation and runtime-health observation |
| `src/adapters/camera/` | Strict 104-byte legacy wire decoder; SOCK_SEQPACKET/SCM_RIGHTS receiver; session-owned ACK; Start/Stop reconciliation; optional GIO D-Bus client; source lifecycle and bounded RAW-reference resolver | Authenticated FW registry RPC, live transport validation, sync/recovery sign-off and automatic source restart |
| `include/vqec/vision/ai/ports/` | Neutral RAW-source, inference-graph and image-processor interfaces; source carries shared frame owner and native handle; processor turns a borrowed NV12 view into the exact model input tensor | Additional platform implementations and pipeline tensor wiring |
| `include/vqec/vision/ai/ports/inference/vqec_vision_image_processor.hpp`, `src/adapters/reference/vqec_vision_reference_processor.cpp`, `src/adapters/qualcomm/dsp/host/vqec_vision_dsp_preprocessor.cpp` | Neutral image-processor port, device-free CPU baseline and production Qualcomm generic v1 cDSP transform; exact contract/capability validation and reusable registered buffers stay private to the adapter | Independent model golden approval, released-FW DMA-BUF completion and additional dtype/layout semantics |
| `src/adapters/qualcomm/` | Generic cDSP preprocessing/dense/overlay, typed tensor extraction, owned QNN engine and V4L2 H.264 ring output; exact full candidate sustained 30.008 FPS at 13.50% average CPU with visually reviewed boxes on `.98` | Released-FW camera/ring/RTSP acceptance, registered QNN output, multi-graph QNN, thermal qualification and BSP recovery |
| `src/adapters/qualcomm/dsp/host/`, `src/adapters/qualcomm/dsp/v1/` | Private host client opens only generated ABI v1, negotiates capability/limits/domain generation and distinguishes completed from uncertain; descriptor-driven image, dense and overlay operations use no model-id dispatch; live mask 19/full workload pass on `.98` | BSP-signed release skeleton, registered/scatter-gather tensor transport, released-FW completion and reset-under-in-flight-work/cache/fence evidence |
| `src/adapters/qualcomm/qnn/vqec_vision_qnn_engine.cpp`, `vqec_vision_qnn_inference_graph.cpp`, `vqec_vision_backend_factory.cpp` | Private optional LACAI-owned QNN engine: dlopen backend/system, backend/device, capability probe, context + single-graph model-lib compose, typed tensor metadata, synchronous client-buffer execute, explicit HTP balanced/low-latency policy and an `inference_graph_port` binding; the factory fails closed on unsupported policy | Async/shared-memory/LoRA execution, shared multi-graph domain and sustained thermal qualification |
| `src/app/pipeline/vqec_vision_camera_graph_pump.cpp`, `vqec_vision_camera_session.cpp` | Portable single-model receive/submit/result progress and validate/start/drain/release lifecycle | Executable composition, live FW/model integration and automatic recovery |
| `src/runtime/scheduler/vqec_vision_model_cadence.cpp` | Fixed 16-slot rational cadence, sequence-gap accounting and numeric due masks | Measured workload policies, ROI/temporal scheduling |
| `src/app/pipeline/vqec_vision_multi_model_pump.cpp`, `vqec_vision_multi_model_session.cpp` | Receive once/share owner across due graphs; one latest-wins preview mailbox; optional persistent per-model workers; completed-slot owner transfer; busy-skip; round-robin results; validate all graphs before one FW acquisition; partial-start rollback and all-graph drain | Released-FW DMA ownership/recovery and sustained multi-model admission/thermal validation |
| `src/app/pipeline/vqec_vision_perception_result_stage.cpp`, `vqec_vision_multi_model_result_router.cpp` | Correlates tensor pipeline PTS with retained source identity, routes by stable model slot, derives independent per-model gaps, then composes decode and tracking transactionally | Concrete decoders/trackers, multi-model temporal fusion and replay qualification |
| `src/app/composition/vqec_vision_perception_stage_factory.cpp`, `vqec_vision_source_perception_factory.cpp` | Transactionally construct one source/model chain or a 1..16-slot source group with distinct tracker ownership; exact manifest reference, model/version/digest/decoder identity, tensor bounds and decoder-specific output schema are validated before each tracker is created | Authenticated package/manifest resolution, tracker-contract selection and concrete decoder/tracker packages |
| `src/app/composition/vqec_vision_runtime_composition_factory.cpp` | Builds the validated admission snapshot, composes catalog-bound plans/cadence/output metadata, enforces application-wide graph/cycle uniqueness, constructs source sessions/perception groups and returns a validated application composition without acquiring hardware | Authenticated artifact/path/evidence resolution, platform owner factories, measured admission and service executor activation |
| `src/app/supervision/vqec_vision_runtime_executor.cpp` | Drives the composition round robin, rebuilds the pump report from source session progress, routes each tensor result through the per-source perception/feature pipeline, and retains one take-once output slot | Concrete package factories, output dispatch and a threaded service loop |
| `src/adapters/reference/vqec_vision_reference_source.cpp`, `vqec_vision_reference_graph.cpp` | Device-free synthetic NV12 source and zero-tensor graph implementing the neutral ports and the complete graph lifecycle | Any board, model, accuracy, zero-copy or DMA-completion claim |
| `src/app/service/bootstrap/vqec_vision_service_main.cpp` | Required executable `vqec_ai_vision_applications`: delegates process control to the service runtime | Process supervision and IPK packaging |
| `src/app/pipeline/vqec_vision_feature_fanout.cpp`, `vqec_vision_multi_model_feature_pipeline.cpp` | Bounded stable-slot feature fan-out with per-feature isolation; activation-time mapping routes each tracked model result only to its direct feature consumers | Multi-model temporal joins, effective-state manager and concrete feature rules |
| `src/app/supervision/vqec_vision_multi_source_supervisor.cpp` | Binds 1..16 borrowed sessions; round-robin progress, per-source fault isolation, snapshots and latched global stop | Service executors, automatic restart/backoff, epoch replacement and BSP recovery execution |
| `src/core/media/vqec_vision_preview_surface.cpp`, `vqec_vision_preview_pool.cpp` | Writable-to-sealed CPU NV12 ownership and preallocated 1..4-surface pool; final-reader reuse | Real pixel copy/overlay renderer and hardware allocation/import |
| `src/core/media/vqec_vision_encoder_window.cpp`, `vqec_vision_encoder_contract.cpp` | Bounded reservation/commit/one-shot submission; frame/PTS/generation checks; independent input/output completion, event preflight/application and fault retention | Concrete encoder backend and device conformance |
| `src/app/pipeline/vqec_vision_encoder_preparation.cpp` | Reserve/acquire/rollback/cancel, sealed backend envelope, guarded port submission and one-step combined backend/ledger drain | Pixel population, hardware encoder and process job-owner/event-loop composition |
| `src/core/media/vqec_vision_encoded_output.cpp`, `vqec_vision_output_generation.cpp` | Immutable owned H264/SPS/PPS and monotonic non-reused output binding IDs | Runtime-wide generation ownership and live reconnect lifecycle |
| `src/outputs/media/vqec_vision_encoded_dispatch.cpp` | Authorized synchronous AU dispatch; one-event backend polling/validation; preflight/dispatch/ledger handling with separate delivery status | Trusted rendered-scope binding, per-job context storage, event loop and event/evidence routing |
| `src/adapters/fw_output/vqec_vision_ring_sink.cpp` | Optional pinned FW SDK wrapper, released header/open-options mapping, generation/demand validation and synchronous payload write | Safe ring open/recovery/single-writer supervision, encoder wiring and live RTSP/UI qualification |
| `src/core/output/vqec_vision_output_gate.cpp` | Pure revisioned feature/attribute/source authorization and invalidation; used by encoded dispatch | Signed grants, full feature manager, activation enforcement and renderer integration |
| `src/outputs/media/vqec_vision_overlay_preparation.cpp` | Authorized, transactional observation-to-overlay metadata preparation before a renderer adapter | Concrete pixel renderer and Qualcomm overlay integration |

## Qualcomm implementation boundary

User-confirmed target: QCS6490 / Qualcomm Linux 1.8. [ADR 0002](../adr/0002_qualcomm_plugin_backend.md)
supports the private plugin inference graph:

```text
appsrc -> qtimlvconverter (engine=fcv) -> tensor capsfilter -> qtimlqnn -> appsink
```

The current production service uses the owned QNN backend and the neutral image-processor port.
On Qualcomm, the production primary path marshals validated activation metadata into the generic
FastRPC v1 image descriptor, runs FastCV scale/colour on cDSP and submits the returned owned
tensor through `inference_graph_port`. The plugin pipeline remains a compatibility/reference
adapter, not the selected full-workload path.

The graph validates factories/properties/plans, configures transactionally, loads to READY,
requires source binding before PLAYING, submits through `frame_submission`, correlates
internal ticket PTS and polls results/input completion independently. EOS plus job drain
gates unload. Armed destruction retains the graph in its reserved domain slot; restoration
allows late reconciliation, not DMA cancellation or safe process termination.

Input is a single linear NV12 image; model input is batch1 NHWC RGB/BGR with any reviewed
element type (INT8..FLOAT32); non-FLOAT32 input requires identity normalization and only
FLOAT32 accepts explicit plugin mean/sigma. Output extraction is dtype-generic across the
same set, sizes bytes by element type and copies exact packed bytes into owned typed blobs.
Genuine per-tensor mixed dtype and non-FLOAT32 output are limited by the single-type caps
and by the installed plugin's FLOAT32 output workaround, so they are rejected rather than
reinterpreted; native output dtype requires the direct-SDK backend. FD duplication/shared
ownership does not establish end-to-end zero-copy or hardware completion.
Factory and property probing report availability and mutability from the target GStreamer
registry without choosing a fallback backend; deployment policy remains responsible for
selecting an admitted path.

The SDK loader now selects the first QNN interface provider whose core API version matches
the headers the adapter was compiled against (major equal, minor not older) instead of
assuming `providers[0]` is compatible; an incompatible set returns `unsupported`. The
QCS6490 board smoke re-verified SCRFD composition and execution on HTP after the change
(`vqec_vision_ai_qnn_engine_smoke`, 9 outputs, exit 0).

The production platform now builds each model backend through `qnn_backend_bundle`
(`vqec_vision_ai_qcom_bfact_create`) instead of constructing `qnn_engine` directly, so the
engine open, capability probe and execution-policy validation happen before a graph
binding is handed to orchestration. `vqec_vision_model_runner` uses the same factory; the
board run of the YOLOv8n-person package through it passed preprocess, QNN execute and
decode (exit 0). Production preprocessing is selected through the neutral port and generic v1
capability, not by model identity.

## Build and test inventory

- CMake declares portable core, camera wire/control, orchestration, cadence, admission,
  encoded dispatch and encoder preparation libraries; optional flags enable camera, GIO
  D-Bus, GStreamer bridge, Qualcomm graph, FW ring SDK, JSON loaders, Zvec and artifact digest.
- JSON loaders require nlohmann_json 3.12.0; digest requires OpenSSL 3.0; FW ring requires an
  existing version-pinned SDK target; Zvec is on by default and its pinned SDK is acquired by
  `tools/build/vqec_vision_prepare_zvec.sh`. CMake downloads no sibling source tree implicitly.
- `vqec_ai_vision_applications` has reference, fake and Qualcomm production composition.
  `vqec_vision_ai_manifest_check` checks metadata only.
- The exact `c01b748` expanded eSDK configuration passes 162/162 CTest tests under SDK QEMU
  (2026-09-20). Nine focused S04/App Manager/metadata/evidence binaries pass natively on `.102`;
  this focused rerun is not a new full native-suite count.
- Golden, replay and live FW/model integration suites remain planned scaffolding.
- `.github/workflows/ci.yml` runs structural, host ASan/UBSan, advisory clang-tidy and
  scheduled fuzz jobs unconditionally; eSDK neutral/expanded jobs are gated on `vars.ESDK_ROOT`.
- `tools/checks/vqec_vision_check_source_layout.sh` checks physical filenames, quoted includes and
  CMake source paths only; it is not an AST naming, ABI or ownership checker.

## Not delivered and next integration work

1. Released-FW C07 evidence receiver/media receipt, natural S04 event correlation and remaining
   S01-S18 feature/model quality packages.
2. Model accuracy calibration and golden parity; concrete feature packages; production
   tracker and attribute producers; pose/OCR.
3. Released-FW camera/ring/RTSP/D-Bus conformance and hardware DMA completion / BSP recovery.
4. Trust rotation, asynchronous package operation journal, content-addressed update/rollback
   and backend conformance.
5. Automatic source/BSP recovery, fault/soak/golden tests and performance/thermal qualification.

Delivered component details live in the architecture and contract docs, not here:
[system architecture](../architecture/system_architecture.md),
[multi-model session](../architecture/multi_model_session.md),
[encoder preparation](../architecture/encoder_preparation.md),
[encoded dispatch](../architecture/encoded_dispatch.md),
[cascade inference](../architecture/cascade_inference.md),
[image alignment port](../architecture/image_alignment_port.md),
[recognition session](../architecture/recognition_session.md),
[FW release compatibility](../contracts/fw_release_compatibility.md) and
[FR validation](../testing/face_recognition_production_validation.md).

## See also

- [Capability matrix](capability_matrix.md), [architecture alignment review](architecture_alignment_review.md)
- [Architecture improvement plans](../planning/architecture_improvement/README.md), [QCS6490 target](../testing/qsc6490_board.md)
