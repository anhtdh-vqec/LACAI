# Capability matrix

Snapshot 2026-09-19, synchronized with the clean source layout. Board smoke differs from
product acceptance. Test/configuration counts live in
[implementation status](implementation_status.md); do not infer capability from an
installed SDK/plugin. Evidence rules: [documentation style](documentation_style.md).

| Capability | Source / logic | Native evidence | Not yet accepted |
|---|---|---|---|
| Three-team contract authority | C01–C10 registry, producer receipt schema/checker and S01–S18 stable IDs; eSDK/QEMU 100/100 | `.98` registry/self-test smoke | BSP+FW target receipts and AI Model golden/quality receipts |
| Metadata/query P2 | Accepted D01–D18/Q01–Q30 capability catalog; v1 contracts/codec; composed bounded writer/live service; packed SQLite shards; receipt-safe retention; association/episode revisions and corrected rollups | 142/142 eSDK; isolated 45,081-record benchmark; exact composed `.102` candidate at 30.124 FPS/13.16% CPU with 26/26 commits; SIGKILL, disk-full, corruption and cancellation gates | Unsupported query groups remain explicit; Kafka transport P3, concrete producers/golden P5, long power/thermal soak and cross-device center analytics remain |
| Static single-image catalog/manifest | Implemented, tested | Model-specific probes | Generic model accuracy |
| Dynamic/multi-input/stateful/batch | Rejected where unsupported | None | Implementation/capacity |
| Synchronous owned QNN | Implemented, tested | SCRFD/YOLO execution + parity; EdgeFace probe | End-to-end FR accuracy |
| Async/shared/registered QNN, LoRA | Contract/unsupported | None | Lifecycle + performance |
| Multiple independent models | Session/pump source; dependent model activation | `.98` one-source person + face + secondary embedding corrective smoke | Shared context, concurrent multi-source capacity, sustained workload |
| Plugin graph lifecycle | Implemented + logic tests | Installed-plugin/lifecycle smoke | Full model/BSP matrix |
| Generic cDSP preprocessing / ROI alignment | Production primary preprocess uses descriptor-driven v1 image transform; EdgeFace retains bounded host alignment | `.98` full workload at 30.008 FPS; v1 image conformance and live smoke | Additional transform semantics, FR golden parity and released-FW completion |
| FastRPC v1 generic host protocol | QAIC client + capability/generation/request/response conformance; production image, dense and overlay operations use descriptor data, not model IDs | `.98` mask 19, all three operations live, full-workload acceptance; eSDK conformance tests | BSP-signed skeleton, registered multi-tensor transport and in-flight reset/cache/fence completion |
| Camera lease/wire | Implemented + socket fixtures | Compatibility input | Released FW DMA/cache/fence semantics |
| Worker/pool/QoS helpers | Implemented + logic tests | Scope-specific evidence only | Whole production integration/soak |
| Portable IoU tracker / reference feature | Distinct production IoU baseline; reference feature is fixture only | Earlier person smoke predates this separation | MOT identity continuity and production feature algorithms/golden |
| Primary anchor-distance FD | Core + production kind selection | Compatibility live cascade `.98` | Golden real-model/released-FW acceptance |
| Cascade coordinator/worker | Production bounded async worker, exact-frame store, captured revision, joined drain | `.98` 282 embeddings, zero cascade failure, clean exit 0 | BSP completion/reset fault acceptance, golden parity and sustained multi-face latency |
| Cascade frame store | Pump/session/executor integration + logic tests | Native synthetic lifetime/ticket test | Dependent hardware completion + epoch recovery |
| Zvec index | C API adapter, default build, private tmpfs lifecycle | Real library synthetic search/mutation test; `.98` private-storage pass | Capacity/performance/load benchmark |
| Recognition matching policy | Subject aggregation + configurable threshold/margin; durable encrypted gallery | eSDK logic test; `.98` clean-restart recovery | Calibration, temporal state and attendance events |
| cDSP overlay/H264/ring | Bounded v1 NV12 overlay compose, rpcmem surface pool and direct `v4l2h264enc` DMA-BUF import; no `qtivoverlay` | Full candidate at 30.125 preview FPS with visual box/label review | Released-FW conformance, generic output ports, long-run completion/recovery |
| Prepared overlay authorization | Prepared-only renderer and scoped source binding, eSDK tests | Pre-audit native helper smoke | FW demand/PTS/revoke and multi-stream output conformance |
| Event delivery seam | Acceptance/handoff/discard semantics logic-tested | Pre-audit native helper smoke | Durable UDS / outbox transport (Plan 3) |
| Model artifact resolver | Sealed verified model-byte owner, hostile-artifact tests | Native synthetic tests and corrective live model load `.98` | Signed provenance of digest/package/libraries; maps alone do not prove seals |
| Hardware admission envelope | Explicit loader, source tensor budget, preview count consistency; one-source profile | `.98` process/thermal observations and declared-workload ceilings; native admission tests | Complete device allocation, sustained DDR/thermal/coexistence and owner review |
| FD→embedding→attendance | Source-composed through typed embedding | FD→embedding compatibility smoke `.98`; attendance absent | Golden/released-FW cascade, enrollment, matching, recovery, events |
| Hardware zero-copy | Not established | No complete proof | Import/cache/fence/last-read trace |
| Performance target | Exact `.98` full workload with registered DMA-BUF, cDSP preprocess/dense/overlay: 30.008 FPS and 13.50% average of one logical core over five minutes | Declared AI APP gate accepted; visual overlay pass; FD/thread stable | 18-usecase capacity, cold-start phase, released-FW thermal/long-soak qualification |
| Recovery/metrics helpers | Partial source | No release soak | BSP reset, durable control, complete stage metrics |
| Fuzzing tools | Source exists | No new run in this review | eSDK-compatible instrumentation and recorded runs |

AI Camera/Box share the intended RAW boundary; this does not prove every FW origin has
been integrated. Zvec SDK execution is CPU/index evidence, not Adreno acceleration.
**Status:** accepted — current AI APP workload snapshot; external release gates remain explicit.


## See also

- [Implementation status](implementation_status.md), [architecture alignment review](architecture_alignment_review.md)
