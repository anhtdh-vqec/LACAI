# Vendor-neutral inference-graph port

Status: interface, Qualcomm forwarding adapter and single-model app migration delivered;
multi-model fan-out/session integration is also source-delivered. Neutral execution
capability/policy contracts and the port capability/policy query are source-delivered
(A1/A3); the owned QNN engine and device verification remain pending.

## Execution capability and policy

The port exposes a probed `inference_capabilities` via
`vqec_vision_ai_ports_infgr_get_capabilities` and validates an
`inference_execution_policy` via `vqec_vision_ai_ports_infgr_validate_policy` before any
resource is acquired, so accelerator optimizations (async, registered/shared memory,
native output, compute-unit affinity, perf profile, adapter updates) are requested as
validated neutral data rather than a vendor branch in orchestration. The default is
deliberately conservative (one synchronous graph, copy memory, float32 only) and fails
closed, so an adapter must override capabilities to advertise what it can satisfy. The
reference backend advertises every reviewed dtype and native output; the plugin-backed
Qualcomm adapter keeps the conservative default until the owned QNN engine exists. The
contract and validators are in `vqec_vision_inference_execution.hpp`; fail-closed
semantics and Qualcomm mapping are in
[qualcomm execution policy](qualcomm_execution_policy.md) and
[ADR 0003](../adr/0003_owned_qnn_engine.md).

`inference_graph_port` is the only model-execution dependency allowed in application
orchestration. It exposes explicit configure/load/bind/start/arm/submit/poll/drain/unload
operations and a neutral state enum. GStreamer, Qualcomm/QNN, FastCV and recovery-domain
types remain private to the platform adapter.

Submission accepts `const raw_frame&`. A backend that accepts the job must retain a copy of
the frame's shared owner until its real input-read completion. Because the caller keeps the
original frame envelope, the same pixels can be submitted to several due model graphs
without pixel memcpy; each accepted graph contributes one owner reference. This is an
ownership design, not proof that the vendor imports DMA-BUF without an internal copy.

`qualcomm_inference_graph` borrows the existing `plugin_graph` and owns a shared reference
to its `graph_retention` domain. It is the only new layer that converts the native handle
to a Linux FD and forwards Qualcomm lifecycle calls. Invalid/non-FD handles fail before
vendor submission. Retention capacity and armed-graph safety rules are unchanged.

The current `camera_session` and `camera_graph_pump` now depend on RAW-source and inference
ports only and therefore build as portable orchestration source. Platform composition owns
concrete adapters and must keep them alive until explicit stop/unload. The implemented
multi_model_session binds 1..16 graph ports to one RAW-source port; multi_model_pump uses
numeric cadence slots and shares each received owner across accepted graphs. Executable
owner construction and live integration remain pending. See [multi-model session](multi_model_session.md).
