# ADR 0003 — LACAI-owned QNN engine behind a neutral execution policy

Date: 2026-09-12. Status: accepted implementation direction. Owner/reviewer: AI APP lead.
Board qualification remains pending.

## Context

ADR 0002 selected a private GStreamer plugin graph (`qtimlvconverter` + `qtimlqnn`) as the
first Qualcomm backend. That path is available on the image and cross-builds, but it is
constrained: the wrapper negotiates FLOAT32 outputs, executes one graph per model, and
does not expose HTP core affinity, asynchronous execution, shared/registered memory or
adapter (LoRA) updates. The AI team now delivers W8A16 graph models (SCRFD, EdgeFace) and
the product needs the fastest defensible Qualcomm execution.

The legacy `application/ai_app` already ran these model families on QCS6490 through QAI
AppBuilder. QAI proves the mechanism (float tensor I/O over quantized graphs, multi-model
in one process, HTP perf profile, core affinity, async, shared memory, LoRA) but it is a
C++20 sample lineage with per-call output allocation, coarse `bool` status and limited
completion semantics. LACAI must keep explicit ownership, typed tensors, bounded work,
fail-closed policy and portability to other vendors.

## Decision

Implement a **LACAI-owned QNN engine adapter** on the approved QAIRT SDK, behind the
existing `inference_graph_port`:

- add vendor-neutral `inference_capabilities` and `inference_execution_policy` contracts;
  the runtime validates a policy against a probed capability and fails closed;
- implement the adapter with the reserved owners `vqec_vision_qnn_engine.cpp` (`qneng`),
  `vqec_vision_buffer_manager.cpp` (`bufmg`), `vqec_vision_backend_factory.cpp` (`bfact`)
  and `vqec_vision_sdk_loader.cpp` (`sdkld`) in `src/adapters/qualcomm/`;
- translate the neutral policy to QNN/QAI mechanisms: HTP compute-unit affinity as
  `compute_unit_affinity`, async as `execution_mode`, registered/shared memory as
  `memory_mode`, LoRA as a model-update descriptor, and a neutral `perf_profile`;
- do **not** link QAI AppBuilder or copy its source without per-file license/provenance
  review; reuse only the documented mechanism, written as C++17 under LACAI contracts;
- keep the plugin graph as one implementation of the same port. Backend selection is by
  probed capability and validated policy, never an implicit fallback.

## Consequences

- LACAI keeps one neutral execution contract for every vendor; Qualcomm optimization
  becomes policy (data) rather than a vendor branch in orchestration.
- Zero-copy input (registered DMA-BUF), native output dtype, async overlap, one shared
  HTP context and LoRA become possible, but each requires board evidence; no claim is
  made from source alone.
- More adapter work than wrapping QAI; the QAIRT version, hexagon-v68 skel and model
  generation version must be pinned and reconciled (models were observed built with
  QAIRT 2.35 and 2.43).
- The plugin path remains supported for models and images it satisfies.

## Phases

A0 contract-first documents; A1 neutral capability/policy contracts and validators;
A2 execution-domain descriptor and admission; A3 port capability/policy wiring and
reference adapter; A4 Qualcomm engine and backend factory; A5 shared/registered buffers;
A6 model-update lifecycle; A7 board validation and measurement.
