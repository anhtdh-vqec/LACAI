# Neutral execution policy and the Qualcomm engine

This document is normative for the owned QNN engine direction in
[ADR 0003](../adr/0003_owned_qnn_engine.md): it defines the vendor-neutral capability and
execution-policy contract and how the Qualcomm adapter maps it to QNN mechanisms, memory modes and
HTP performance votes.

**Status:** source-delivered — the synchronous owned-QNN path and the explicit HTP low-latency
policy are in the tree; async/shared-memory execution and release qualification remain pending.
**Layer:** adapters. **Source:** `src/adapters/qualcomm/vqec_vision_qnn_engine.{hpp,cpp}`,
`vqec_vision_qnn_inference_graph.{hpp,cpp}`, `vqec_vision_backend_factory.{hpp,cpp}`.

## Responsibility

- Keep `inference_capabilities` and `inference_execution_policy` vendor-neutral.
- Translate validated intent to QNN mechanisms only inside the adapter.
- Fail closed on unsupported mode, memory, perf profile, native-output request, affinity or
  inflight bound with a reason.
- Do not let QNN, GStreamer, FastCV or QAI types appear in neutral layers.

The runtime owns **what** a model graph needs; the adapter owns **how** the accelerator provides
it. `inference_capabilities` (probed) and `inference_execution_policy` (validated intent) are
vendor-neutral. `vqec_vision_qnn_engine` and the other reserved Qualcomm owners translate them to
QNN mechanisms. No QNN, GStreamer, FastCV or QAI type may appear in `contracts/`, `runtime/`,
`perception/`, `features/` or `outputs/`.

`vqec_vision_ai_core_inexe_policy_is_supported` is fail-closed: an unsupported mode, memory, perf
profile, native-output request, affinity or inflight bound is rejected with a reason. The adapter
never silently downgrades; a permitted degraded mode must be an explicit, observable policy.

## Build integration

`VQEC_VISION_AI_QAIRT_ROOT` (default `third_party/qairt`, a symlink to the installed private SDK)
supplies the QNN headers. `VQEC_VISION_AI_ENABLE_QNN_ENGINE=ON` builds the LACAI-owned engine
target; it is OFF by default so the neutral base does not depend on a private SDK.

`qnn_sdk_libraries` dynamically loads the backend and system libraries and resolves the QNN
interface provider without exposing a QNN type in its header. `qnn_engine` then creates the backend
and, when the interface provides it, the device. A failed device creation is a fault on the admitted
HTP path, not a silent CPU downgrade.

## Capability inventory (S01)

`qnn_engine` advertises only the operations the adapter implements, never every symbol the resolved
interface happens to expose. `available` (SDK symbol resolveable), `implemented` (the adapter has a
wired lifecycle path), `qualified` (board evidence exists) and `admitted` (policy accepted for a
model) are separate states; the probed capability is the intersection of adapter + model graph +
backend + policy.

| Capability | SDK symbol seen | Adapter implemented | Qualified on QCS6490 | Advertised now |
|---|---|---|---|---|
| Synchronous client-buffer execute | `graphExecute` | yes | **yes** (SCRFD/YOLOv8n, byte-identical to qnn-net-run, 2026-09-14) | `mode=synchronous`, `max_inflight_jobs=1` |
| Native (graph-dtype) output | `graphExecute` | yes | **yes** (uint16 UFIXED_POINT_16 returned) | `supports_native_output=true` |
| Async execute | `graphExecuteAsync` | no | no | `supports_async=false` |
| Shared/registered buffers | `memRegister`/`memDeRegister` | yes | **yes** (ION/rpcmem MEMHANDLE bound via libcdsprpc) | `supports_shared_memory=true`, bound `16` when rpcmem is available |
| Artifact / LoRA update | `contextApplyBinarySection` | no | no | `supports_artifact_update=false` |
| Multi-model execution domain | one context | no | no | `supports_multi_model_domain=false`, `graph_count=1` |
| Perf profile | HTP perf infra | `balanced` plus explicit DCVS V3 low-latency vote | integration-smoked on `.98` (2026-09-16); thermal acceptance open | `balanced`, `low_latency` when HTP perf functions are present |
| Compute-unit affinity/topology | HTP device infra | not probed | no | `compute_unit_count=0` |

An unimplemented operation is reported unsupported so `vqec_vision_ai_core_inexe_policy_is_supported`
rejects a dependent policy before load or submit; each row moves to `implemented` only with the
matching lifecycle path and negative tests. `prepare` must call `graphFinalize` after
`composeGraphs`: generated model libraries compose but do not finalize, and `graphExecute` fails
otherwise (board-discovered). Sync execute and native output are board-qualified. The HTP
low-latency mapping has live integration evidence, but no sustained thermal acceptance. Async,
update, file domains and topology still require a qualified lifecycle and board evidence.

`prepare` creates a context, `dlopen`s one QNN model library (`.so` from qnn-model-lib-generator),
resolves `QnnModel_composeGraphs`/`QnnModel_freeGraphsInfo`, composes its graphs and rejects a
multi-graph library. `prepare` resolves and validates the input/output tensor identity once (name,
shape, dtype, quantization with `zero_point = -offset`); `get_tensors` returns that cache and
`execute` reconstructs no tensor metadata on the hot path. `execute` re-checks each input against
the full cached identity (name, shape, dtype and quantization, not only byte count), binds client
buffers for the single graph, runs synchronous `graphExecute` and returns native-dtype output blobs.
The wrapper structures used by generated model libraries are mirrored as local ABI types instead of
including the restricted SDK example header. Output bytes use a pre-allocated workspace initialized
at `prepare` time; when `libcdsprpc.so` is available, output tensors bind directly to physical ION
pages (`QNN_TENSORMEMTYPE_MEMHANDLE`), eliminating heap reallocations and CPU staging during
steady-state inference. Read [qualcomm_qnn_ion_memory.md](qualcomm_qnn_ion_memory.md) for memory
architecture details.

`vqec_vision_ai_qcom_bfact_create` builds one owned bundle from the trusted `resolved_model_paths`:
it opens the engine with the resolved backend/system libraries, probes capabilities, fails closed
when the validated policy is unsupported, and constructs the graph binding. It does not configure,
load or execute, so platform owner factories and runtime composition can bind it exactly like the
plugin or reference graph.

`qnn_inference_graph` implements `inference_graph_port` over the engine: configure/load compose the
model, start validates the declared outputs against the composed graph, arm configures the bounded
submission window, `get_input_specs` reports the model input tensor and `submit_tensors` executes
and correlates the result to the source frame. `submit_frame` is rejected because pixel
preprocessing is a separate neutral stage; the engine never treats a raw NV12 frame as model input.
The graph reports the engine's probed capabilities. A pump binding that carries an
`image_processor_port` and its plan preprocesses the frame with the neutral stage and submits
tensors; a binding without one keeps raw-frame submission, so both backends share the same
scheduling, cadence and ownership.

These steps still prove neither accelerator availability nor execution correctness until run on the
board; callers must serialize engine calls on the backend worker.

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

`compute_unit_count == 0` means the backend does not advertise topology; any policy that requests
units or affinity is then rejected rather than guessed.

## Execution domain

`inference_execution_domain` is the neutral identity and capacity of one shared accelerator resource
domain (one backend/device/context). Several model graphs may bind to one domain so one HTP context
is reused instead of one context per model. The adapter owns the real domain object; the runtime
admits the aggregate with `vqec_vision_ai_core_inexe_domain_admits`, which rejects over-capacity
graph counts, aggregate inflight beyond the backend bound, and multi-graph admission when
shared-context support is absent. A domain is not an authorization decision and does not change
buffer ownership or completion semantics.

## Shared/registered buffers

`inference_shared_buffer` names one imported/registered allocation by `allocation_id`/`generation`,
not by a numeric FD, so a stale FD is never treated as identity. `memory_mode::registered_shared`
selects this path; the runtime checks the descriptor and remaining capacity with
`vqec_vision_ai_core_inexe_shared_buffer_supported` and fails closed when the backend does not
advertise shared memory. A valid descriptor proves none of import, zero-copy, cache coherency or
completion; those require the BSP contract and board trace.

## Adapter / LoRA updates

`inference_model_update` binds an immutable update artifact to an exact base model identity and
revision. The runtime validates it and checks `vqec_vision_ai_core_inexe_model_update_supported`
against the backend capability before use. An update is applied only while the graph is idle and
drained; the artifact is authenticated by the trusted resolver. An update never changes buffer
ownership, completion semantics or the effective output policy.

## Explicit ownership and bounds (unchanged)

Async or shared memory never changes completion semantics: a submitted job owns its shared
frame/tensor owner until the backend reports real completion. FD close, cache sync, `appsrc`
acceptance, `gst_buffer` finalization and timeout are not completion. Input stays read-only and
AI-owned surfaces stay separate. Drains are bounded and a non-quiescent resource is quarantined and
reported, not reused.

## Model and version inputs

- Artifact paths come from the trusted resolver; the adapter only loads resolved paths.
- Output tensor identity/dtype/quantization comes from the model output manifest and the typed
  tensor contract.
- QAIRT runtime/libs, hexagon-v68 skel and the model generation version must be pinned. Models
  observed built with QAIRT 2.35 and 2.43; cross-version loading is not assumed.

## Evidence gates before enabling a path

1. `qnn-platform-validator` and a runtime probe record backend/device/core/version on the target
   image.
2. Golden input/output tests including dtype, layout, quantization and stride.
3. Ownership/cache/fence/completion traces under repeated load, stream, drain and unload.
4. Copies, pool occupancy, p50/p95 latency, FPS, RSS and thermal on the agreed workload. A vendor
   sample or a source build is not performance evidence.
5. Per-file license/provenance review; no copied vendor implementation into neutral layers.

## Live HTP performance votes (2026-09-16)

The production launcher accepts `--inference-perf-profile balanced|low_latency`. The default remains
balanced (no added clock vote). The explicit low_latency intent requests an HTP DCVS performance-mode
TURBO bus/core vote through the pinned `QnnHtpDevice_PerfInfrastructure_t`. Device/core IDs are
discovered with version-checked platform metadata; no board-specific index or model name chooses the
vote. Each engine owns a distinct nonzero power client ID until close after graph drain.
Creation/application failures roll back the client and fail activation; other backends and
unsupported profiles fail closed. No RPC polling, global client-zero override, thermal governor
change or cancellation behavior is introduced. Live and isolated image enrollment models use the
same configured intent. This is latency policy, not a thermal acceptance claim; it needs
lead/platform review and workload/soak evidence before release.

On `.98`, applying this policy and parallelizing the two independent root-model owners raised a
short combined person+FD/FR sample from roughly 9.2 FPS to 24--28 FPS depending on board
temperature. A later hot 45-second sample fell to 22.6 encoded FPS with thermal zones at roughly
66--70 C. These samples prove the mechanism executes and also show why `low_latency` cannot be
treated as a sustained-performance or temperature guarantee.

## Limits and next work

- Async, LoRA/update, file domains and topology still require a qualified lifecycle and board
  evidence.
- Released-FW acceptance and sustained thermal qualification remain open.
- The `low_latency` vote needs lead/platform review and workload/soak evidence before release.

## See also

- [Qualcomm adapter — implementation blueprint](qualcomm_adapter.md)
- [Qualcomm QNN ION-registered output memory](qualcomm_qnn_ion_memory.md)
- [Inference graph port](inference_graph_port.md)
- [ADR 0003 owned QNN engine](../adr/0003_owned_qnn_engine.md)
