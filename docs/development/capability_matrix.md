# Capability matrix

Snapshot 2026-09-15, synchronized with the current cascade runtime source. Board smoke khác product acceptance.
Số test/configuration nằm ở [implementation status](implementation_status.md); không suy
capability từ SDK/plugin đã cài. Quy tắc evidence: [documentation style](documentation_style.md).

| Capability | Source / logic | Native evidence | Chưa nghiệm thu |
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
| IoU tracker/reference feature | Implemented + logic tests | Person composition uses reference tracker | Identity continuity and real feature quality |
| Primary anchor-distance FD | Core + production kind selection | Compatibility live cascade `.99` | Golden real-model/released-FW acceptance |
| Cascade coordinator | Exact-frame alignment + sync embedding logic tests | Compatibility combined path `.99` | Post-fix multi-face + async worker/completion |
| Cascade frame store | Pump/session/executor integration + logic tests | Native synthetic lifetime/ticket test | Dependent hardware completion + epoch recovery |
| Zvec index | C API adapter, default build | Real library synthetic search/mutation test | Durable gallery/recovery/load benchmark |
| QTI overlay/H264/ring | Production compatibility path | Person 30 encoded FPS | Released FW conformance and generic output ports |
| FD→embedding→attendance | Source-composed through typed embedding | FD→embedding compatibility smoke `.99`; attendance absent | Golden/released-FW cascade, enrollment, matching, recovery, events |
| Hardware zero-copy | Not established | No complete proof | Import/cache/fence/last-read trace |
| Performance target | Person sample about 44.5%; short FR sample 29.53% with output disabled | Historical diagnostics | Requested CPU 15–25%, full FR output workload and thermal |
| Recovery/metrics helpers | Partial source | No release soak | BSP reset, durable control, complete stage metrics |
| Fuzzing tools | Source exists | No new run in this review | eSDK-compatible instrumentation and recorded runs |

AI Camera/Box share the intended RAW boundary; this does not prove every FW origin has
been integrated. Zvec SDK execution is CPU/index evidence, not Adreno acceleration.
