# Capability matrix

Snapshot 2026-09-17, synchronized with the clean-base source. Board smoke differs from
product acceptance. Test/configuration counts live in
[implementation status](implementation_status.md); do not infer capability from an
installed SDK/plugin. Evidence rules: [documentation style](documentation_style.md).

| Capability | Source / logic | Native evidence | Not yet accepted |
|---|---|---|---|
| Static single-image catalog/manifest | Implemented, tested | Model-specific probes | Generic model accuracy |
| Dynamic/multi-input/stateful/batch | Rejected where unsupported | None | Implementation/capacity |
| Synchronous owned QNN | Implemented, tested | SCRFD/YOLO execution + parity; EdgeFace probe | End-to-end FR accuracy |
| Async/shared/registered QNN, LoRA | Contract/unsupported | None | Lifecycle + performance |
| Multiple independent models | Session/pump source; dependent model activation | Individual model probes | Shared QNN context and live multi-model workload |
| Plugin graph lifecycle | Implemented + logic tests | Installed-plugin/lifecycle smoke | Full model/BSP matrix |
| FastCV preprocessing/alignment | Implemented; production cascade binding | Person preprocess + alignment/color smoke | FR golden parity, pooling, released-FW zero-copy |
| Camera lease/wire | Implemented + socket fixtures | Compatibility input | Released FW DMA/cache/fence semantics |
| Worker/pool/QoS helpers | Implemented + logic tests | Scope-specific evidence only | Whole production integration/soak |
| Portable IoU tracker / reference feature | Distinct production IoU baseline; reference feature is fixture only | Earlier person smoke predates this separation | MOT identity continuity and production feature algorithms/golden |
| Primary anchor-distance FD | Core + production kind selection | Compatibility live cascade `.98` | Golden real-model/released-FW acceptance |
| Cascade coordinator | Exact-frame alignment; pending completion retention tested | Earlier compatibility combined path `.98` predates correction | Async worker, completion recovery and multi-face latency |
| Cascade frame store | Pump/session/executor integration + logic tests | Native synthetic lifetime/ticket test | Dependent hardware completion + epoch recovery |
| Zvec index | C API adapter, default build, private tmpfs lifecycle | Real library synthetic search/mutation test; `.98` private-storage pass | Capacity/performance/load benchmark |
| Recognition matching policy | Subject aggregation + configurable threshold/margin; durable encrypted gallery | eSDK logic test; `.98` clean-restart recovery | Calibration, temporal state and attendance events |
| QTI overlay/H264/ring | Production compatibility path | Person 30 encoded FPS | Released FW conformance and generic output ports |
| Prepared overlay authorization | Prepared-only renderer and scoped source binding, eSDK tests | Pre-audit native helper smoke | FW demand/PTS/revoke and multi-stream output conformance |
| Event delivery seam | Acceptance/handoff/discard semantics logic-tested | Pre-audit native helper smoke | Durable UDS / outbox transport (Plan 3) |
| Model artifact resolver | Sealed verified model-byte owner, eSDK tests | Pre-audit native helper smoke | Signed provenance of digest/package/libraries; post-fix board load |
| Hardware admission envelope | Explicit profile loader, missing profile fail-closed; estimated envelope | No approved measured profile or post-fix board run | Aggregate model/FR/index/DDR/thermal measurement and owner review |
| FD→embedding→attendance | Source-composed through typed embedding | FD→embedding compatibility smoke `.98`; attendance absent | Golden/released-FW cascade, enrollment, matching, recovery, events |
| Hardware zero-copy | Not established | No complete proof | Import/cache/fence/last-read trace |
| Performance target | `.98` compatibility: single-model 36-38% of one core; dual-model+FR ~60-65% | Historical diagnostics; `route_latency_*` steady metric | Requested CPU 15–25%, 25-30 FPS FR and thermal |
| Recovery/metrics helpers | Partial source | No release soak | BSP reset, durable control, complete stage metrics |
| Fuzzing tools | Source exists | No new run in this review | eSDK-compatible instrumentation and recorded runs |

AI Camera/Box share the intended RAW boundary; this does not prove every FW origin has
been integrated. Zvec SDK execution is CPU/index evidence, not Adreno acceleration.
**Status:** current — capability snapshot.


## See also

- [Implementation status](implementation_status.md), [architecture alignment review](architecture_alignment_review.md)
