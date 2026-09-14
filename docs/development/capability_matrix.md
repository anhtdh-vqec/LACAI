# Capability matrix

Truthful capability state. A capability is only called **supported** or **qualified** in
prose when the matching column is yes. "Implemented" is source in the tree; "Device-free
tested" is exercised by an eSDK QEMU test; "QCS6490 qualified" requires measured board
evidence. Update this table with any change that moves a row.

| Capability | Implemented | Device-free tested | QCS6490 qualified |
|---|---:|---:|---:|
| Static single-image model (via catalog + manifest validation) | yes | yes | no |
| Multi-input / dynamic-shape / stateful model | no (rejected at activation) | yes (rejection) | no |
| Capability/policy admission (fail-closed) | yes | yes | no |
| Synchronous QNN client-buffer execution | yes | compiled only (no SDK runner in CI) | no |
| Async QNN execution | no | no | no |
| Shared/registered memory (QnnMem) | contract only | no | no |
| Artifact/LoRA update | contract only | no | no |
| Multi-model execution domain | no (single graph) | yes (rejection) | no |
| DMA-BUF camera frame input (protocol/decoder) | yes | yes (socket fixture) | no |
| Frame release dispatcher (bounded retry) | yes | yes | no |
| Hardware DMA completion semantics | no | no | no |
| Bounded non-blocking inference worker | yes | yes | no (not yet wired to the pump) |
| Tensor pool (bounded, double-release detection) | yes | yes | no (not yet wired) |
| Reference CPU preprocess (preprocess oracle) | yes | yes (conformance suite) | no |
| Qualcomm image processor / FastCV path | no | no | no |
| Reference IoU tracker | yes | yes | no |
| Reference ROI/dwell/line/count feature processor | yes | yes | no |
| Fake encoder + bounded ring sink | yes | yes | no |
| Secondary/ROI inference contract + scheduler | yes | yes | no |
| Production composition with fake platform | yes | yes (E2E smoke) | no |
| Overlay/renderer/hardware H264 encode | helpers only | metadata only | no |
| FW ring integration | wrapper source | no | no |
| Recovery backoff controller | yes | yes | no (reconnect loop not wired) |
| Metrics/tracing channel | partial counters | worker/pool/supervisor snapshots | no |
| Wire decoder fuzzing | yes | host Clang libFuzzer, crash-free | no |

## Rules

- A green host/QEMU test is not device, BSP, DMA, model-accuracy or performance evidence.
- "Zero-copy", "hardware acceleration" and "board compatibility" require board evidence.
- Installed, entitled, desired, supported, admitted and running are separate states.

See [implementation status](implementation_status.md),
[eSDK configuration matrix](../testing/esdk_configuration_matrix.md) and
[device-free basecode progress](LACAI_DEVICE_FREE_BASECODE_PROGRESS.md).
