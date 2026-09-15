# Capability matrix

Snapshot 2026-09-15, source baseline 88c5a89. Board smoke khác product acceptance.
Số test/configuration nằm ở [implementation status](implementation_status.md); không suy
capability từ SDK/plugin đã cài. Quy tắc evidence: [documentation style](documentation_style.md).

| Capability | Source / logic | Native evidence | Chưa nghiệm thu |
|---|---|---|---|
| Static single-image catalog/manifest | Implemented, tested | Model-specific probes | Generic model accuracy |
| Dynamic/multi-input/stateful/batch | Rejected where unsupported | None | Implementation/capacity |
| Synchronous owned QNN | Implemented, tested | SCRFD/YOLO execution + parity; EdgeFace probe | End-to-end FR accuracy |
| Async/shared/registered QNN, LoRA | Contract/unsupported | None | Lifecycle + performance |
| Multiple independent models | Session/pump source | Individual model probes | Shared execution domain and live cascade |
| Plugin graph lifecycle | Implemented + logic tests | Installed-plugin/lifecycle smoke | Full model/BSP matrix |
| FastCV preprocessing | Implemented | Person 30 AI results/s compatibility run | FR alignment, released-FW zero-copy |
| Camera lease/wire | Implemented + socket fixtures | Compatibility input | Released FW DMA/cache/fence semantics |
| Worker/pool/QoS helpers | Implemented + logic tests | Scope-specific evidence only | Whole production integration/soak |
| IoU tracker/reference feature | Implemented + logic tests | Person composition uses reference tracker | Identity continuity and real feature quality |
| Primary anchor-distance FD | Core + production kind selection | Decoder synthetic test | Golden real-model/live FD acceptance |
| Secondary scheduler | Contract + logic tests | None for live FR | Typed alignment + async completion composition |
| Cascade frame store | Primitive + logic tests | Native synthetic lifetime/ticket test | Pump integration + hardware completion |
| Zvec index | C API adapter, default build | Real library synthetic search/mutation test | Durable gallery/recovery/load benchmark |
| QTI overlay/H264/ring | Production compatibility path | Person 30 encoded FPS | Released FW conformance and generic output ports |
| FD→FR→attendance | Partial primitives | Not end-to-end | Enrollment, matching, recovery, events |
| Hardware zero-copy | Not established | No complete proof | Import/cache/fence/last-read trace |
| Performance target | Person sample CPU about 44.5% | Historical measured sample | Requested CPU 15–25%, FR workload and thermal |
| Recovery/metrics helpers | Partial source | No release soak | BSP reset, durable control, complete stage metrics |
| Fuzzing tools | Source exists | No new run in this review | eSDK-compatible instrumentation and recorded runs |

AI Camera/Box share the intended RAW boundary; this does not prove every FW origin has
been integrated. Zvec SDK execution is CPU/index evidence, not Adreno acceleration.
