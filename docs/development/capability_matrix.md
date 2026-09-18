# Capability matrix

Snapshot 2026-09-18, synchronized with the clean source layout. Board smoke differs from
product acceptance. Test/configuration counts live in
[implementation status](implementation_status.md); do not infer capability from an
installed SDK/plugin. Evidence rules: [documentation style](documentation_style.md).

| Capability | Source / logic | Native evidence | Not yet accepted |
|---|---|---|---|
| Static single-image catalog/manifest | Implemented, tested | Model-specific probes | Generic model accuracy |
| Dynamic/multi-input/stateful/batch | Rejected where unsupported | None | Implementation/capacity |
| Synchronous owned QNN | Implemented, tested | SCRFD/YOLO execution + parity; EdgeFace probe | End-to-end FR accuracy |
| Async/shared/registered QNN, LoRA | Contract/unsupported | None | Lifecycle + performance |
| Multiple independent models | Session/pump source; dependent model activation | `.98` one-source person + face + secondary embedding corrective smoke | Shared context, concurrent multi-source capacity, sustained workload |
| Plugin graph lifecycle | Implemented + logic tests | Installed-plugin/lifecycle smoke | Full model/BSP matrix |
| FastCV preprocessing/alignment | Implemented; production cascade binding | Person preprocess + alignment/color smoke | FR golden parity, pooling, released-FW zero-copy |
| FastRPC v1 generic host protocol | QAIC client + capability/generation/request/response conformance implemented | `.98` fake-service fixture plus isolated live unsigned dense operation and reopen generation | BSP-signed release skeleton, registered tensor transport, in-flight reset/cache/fence completion and production selection |
| Camera lease/wire | Implemented + socket fixtures | Compatibility input | Released FW DMA/cache/fence semantics |
| Worker/pool/QoS helpers | Implemented + logic tests | Scope-specific evidence only | Whole production integration/soak |
| Portable IoU tracker / reference feature | Distinct production IoU baseline; reference feature is fixture only | Earlier person smoke predates this separation | MOT identity continuity and production feature algorithms/golden |
| Primary anchor-distance FD | Core + production kind selection | Compatibility live cascade `.98` | Golden real-model/released-FW acceptance |
| Cascade coordinator/worker | Production bounded async worker, exact-frame store, captured revision, joined drain | `.98` 282 embeddings, zero cascade failure, clean exit 0 | BSP completion/reset fault acceptance, golden parity and sustained multi-face latency |
| Cascade frame store | Pump/session/executor integration + logic tests | Native synthetic lifetime/ticket test | Dependent hardware completion + epoch recovery |
| Zvec index | C API adapter, default build, private tmpfs lifecycle | Real library synthetic search/mutation test; `.98` private-storage pass | Capacity/performance/load benchmark |
| Recognition matching policy | Subject aggregation + configurable threshold/margin; durable encrypted gallery | eSDK logic test; `.98` clean-restart recovery | Calibration, temporal state and attendance events |
| QTI overlay/H264/ring | Production compatibility path | Person 30 encoded FPS | Released FW conformance and generic output ports |
| Prepared overlay authorization | Prepared-only renderer and scoped source binding, eSDK tests | Pre-audit native helper smoke | FW demand/PTS/revoke and multi-stream output conformance |
| Event delivery seam | Acceptance/handoff/discard semantics logic-tested | Pre-audit native helper smoke | Durable UDS / outbox transport (Plan 3) |
| Model artifact resolver | Sealed verified model-byte owner, hostile-artifact tests | Native synthetic tests and corrective live model load `.98` | Signed provenance of digest/package/libraries; maps alone do not prove seals |
| Hardware admission envelope | Explicit loader, source tensor budget, preview count consistency; one-source profile | `.98` process/thermal observations and declared-workload ceilings; native admission tests | Complete device allocation, sustained DDR/thermal/coexistence and owner review |
| FD→embedding→attendance | Source-composed through typed embedding | FD→embedding compatibility smoke `.98`; attendance absent | Golden/released-FW cascade, enrollment, matching, recovery, events |
| Hardware zero-copy | Not established | No complete proof | Import/cache/fence/last-read trace |
| Performance target | Canonical `.98` person + face + fire/smoke preview: 25.125 FPS and 26.45% of one logical core in a 15-second warm sample | Visual overlay smoke and `route_latency_*` steady metric | `<=12%` current-workload gate, 30-minute statistics, cold start, thermal and soak |
| Recovery/metrics helpers | Partial source | No release soak | BSP reset, durable control, complete stage metrics |
| Fuzzing tools | Source exists | No new run in this review | eSDK-compatible instrumentation and recorded runs |

AI Camera/Box share the intended RAW boundary; this does not prove every FW origin has
been integrated. Zvec SDK execution is CPU/index evidence, not Adreno acceleration.
**Status:** current — capability snapshot.


## See also

- [Implementation status](implementation_status.md), [architecture alignment review](architecture_alignment_review.md)
