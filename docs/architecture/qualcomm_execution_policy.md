# Neutral execution policy and the Qualcomm engine

Status: A1 source-delivered (contracts + validators + tests). Later phases pending.
This document is normative for the owned QNN engine direction in
[ADR 0003](../adr/0003_owned_qnn_engine.md).

## Intent versus mechanism

The runtime owns **what** a model graph needs; the adapter owns **how** the accelerator
provides it. `inference_capabilities` (probed) and `inference_execution_policy` (validated
intent) are vendor-neutral. `vqec_vision_qnn_engine` and the other reserved Qualcomm
owners translate them to QNN mechanisms. No QNN, GStreamer, FastCV or QAI type may appear
in `contracts/`, `runtime/`, `perception/`, `features/` or `outputs/`.

`vqec_vision_ai_core_inexe_policy_is_supported` is fail-closed: an unsupported mode,
memory, perf profile, native-output request, affinity or inflight bound is rejected with a
reason. The adapter never silently downgrades; a permitted degraded mode must be an
explicit, observable policy.

## Build integration

`VQEC_VISION_AI_QAIRT_ROOT` (default `third_party/qairt`, a symlink to the installed
private SDK) supplies the QNN headers. `VQEC_VISION_AI_ENABLE_QNN_ENGINE=ON` builds the
LACAI-owned engine target; it is OFF by default so the neutral base does not depend on a
private SDK.

`qnn_sdk_libraries` dynamically loads the backend and system libraries and resolves the
QNN interface provider without exposing a QNN type in its header. `qnn_engine` then
creates the backend and, when the interface provides it, the device, and probes
capabilities (async via `graphExecuteAsync`, shared memory via `memRegister`, artifact
update via `contextApplyBinarySection`, perf profiles when a device exists). A failed
device creation is a fault on the admitted HTP path, not a silent CPU downgrade.

Neither step creates a context, composes a graph or executes, so they prove neither
accelerator availability nor execution; context/graph/execute are later A4 steps
qualified on the board. Callers must serialize engine calls on the backend worker.

## Contract mapping

| Neutral field | Meaning | Qualcomm mechanism |
|---|---|---|
| `execution_mode::synchronous` | One submission in flight, complete on poll | `graphExecute` synchronous |
| `execution_mode::asynchronous` | Bounded concurrent submissions | QNN async execute + signal/event |
| `memory_mode::copy` | Backend copies input/output | client-buffer execute |
| `memory_mode::registered_shared` | Imported/registered shared buffers | `QnnMem` registration of DMA-BUF/ION |
| `perf_profile` | Coarse latency/throughput intent | HTP perf infrastructure |
| `compute_unit_affinity` / `compute_unit_count` | Allowed accelerator units | HTP device/perf infra core affinity (NSP/HPASS) |
| `max_inflight_jobs` | Bounded concurrency | submission window capacity |
| `priority` | Scheduling intent | device/perf priority where supported |
| `prefer_native_output` | Keep native tensor dtype; skip dequant | typed output path in `qneng` |
| model-update descriptor (A6) | Adapter/LoRA update | `QnnContext_applyBinaryUpdate`/LoRA |
| `execution_domain` (A2) | Shared backend/device/context | one QNN backend/context, N graphs |

`compute_unit_count == 0` means the backend does not advertise topology; any policy that
requests units or affinity is then rejected rather than guessed.

## Execution domain

`inference_execution_domain` is the neutral identity and capacity of one shared accelerator
resource domain (one backend/device/context). Several model graphs may bind to one domain
so one HTP context is reused instead of one context per model. The adapter owns the real
domain object; the runtime admits the aggregate with
`vqec_vision_ai_core_inexe_domain_admits`, which rejects over-capacity graph counts,
aggregate inflight beyond the backend bound, and multi-graph admission when shared-context
support is absent. A domain is not an authorization decision and does not change buffer
ownership or completion semantics.

## Shared/registered buffers

`inference_shared_buffer` names one imported/registered allocation by
`allocation_id`/`generation`, not by a numeric FD, so a stale FD is never treated as
identity. `memory_mode::registered_shared` selects this path; the runtime checks the
descriptor and remaining capacity with
`vqec_vision_ai_core_inexe_shared_buffer_supported` and fails closed when the backend does
not advertise shared memory. A valid descriptor proves none of import, zero-copy, cache
coherency or completion; those require the BSP contract and board trace.

## Adapter / LoRA updates

`inference_model_update` binds an immutable update artifact to an exact base model
identity and revision. The runtime validates it and checks
`vqec_vision_ai_core_inexe_model_update_supported` against the backend capability before
use. An update is applied only while the graph is idle and drained; the artifact is
authenticated by the trusted resolver. An update never changes buffer ownership,
completion semantics or the effective output policy.

## Explicit ownership and bounds (unchanged)

Async or shared memory never changes completion semantics: a submitted job owns its shared
frame/tensor owner until the backend reports real completion. FD close, cache sync,
`appsrc` acceptance, `gst_buffer` finalization and timeout are not completion. Input stays
read-only and AI-owned surfaces stay separate. Drains are bounded and a non-quiescent
resource is quarantined and reported, not reused.

## Model and version inputs

- Artifact paths come from the trusted resolver; the adapter only loads resolved paths.
- Output tensor identity/dtype/quantization comes from the model output manifest and the
  typed tensor contract.
- QAIRT runtime/libs, hexagon-v68 skel and the model generation version must be pinned.
  Models observed built with QAIRT 2.35 and 2.43; cross-version loading is not assumed.

## Evidence gates before enabling a path

1. `qnn-platform-validator` and a runtime probe record backend/device/core/version on the
   target image.
2. Golden input/output tests including dtype, layout, quantization and stride.
3. Ownership/cache/fence/completion traces under repeated load, stream, drain and unload.
4. Copies, pool occupancy, p50/p95 latency, FPS, RSS and thermal on the agreed workload.
   A vendor sample or a source build is not performance evidence.
5. Per-file license/provenance review; no copied vendor implementation into neutral layers.
