# ADR 0006 — Unwired execution infrastructure in the clean base

Status: **accepted** (clean-base CB-D, 2026-09-16). Owner: AI APP lead.
Date: 2026-09-16.

## Context

Three modules existed with source and passing tests but no production wiring. That left two
designs for the same concern and made capability claims ambiguous:

- `src/runtime/scheduler/vqec_vision_secondary_inference_scheduler.{hpp,cpp}` plus the
  neutral `contracts/vqec_vision_secondary_inference.hpp` and `src/core/...secondary_inference.cpp`;
- `src/runtime/scheduler/vqec_vision_inference_worker.{hpp,cpp}`;
- `src/runtime/lifecycle/vqec_vision_recovery_controller.{hpp,cpp}`.

The delivered cascade path is `cascade_coordinator` over the retained `cascade_frame_store`
(ADR 0005). It binds each dependent task to exact retained pixels and a frame-store
completion ticket, which the legacy scheduler does not do (it only bounds opaque queued
requests). The runtime already achieves asynchronous model progress through
`source_session_worker` and the pump's optional per-model workers, and source restart/backoff
is still an open roadmap item (`implementation_status` "Not delivered" #5).

## Decision

1. **Remove the legacy secondary scheduler and its contract.** Two designs for cascade
   scheduling must not coexist. `cascade_coordinator` is the owner; the legacy modules,
   their CTest and CMake targets are deleted. This is a source removal, not a deprecation
   shim.
2. **Keep `inference_worker` as a reserved offload path.** It is the documented future
   owner for moving a blocking backend `submit` off the serialized source executor without
   requiring the QNN async API. It is not production-wired. Its doc already states this.
3. **Keep `recovery_controller` as a reserved recovery path.** It owns bounded per-source
   exponential backoff for the roadmap's automatic source/BSP recovery. It is not
   production-wired.
4. **No module may be built, tested and left unwired without an explicit ADR status.** Every
   reserved module must name its future wiring point in this ADR; anything else is removed.
5. **Capability/status docs must keep reserved modules out of delivered-capability claims.**
   The structural presence of a target is not an integrated feature.

## Reserved-module wiring points

| Module | Future wiring point | Gate before wiring |
|---|---|---|
| `inference_worker` | `multi_model_pump` / source-session executor for blocking-backend offload | measured need; epoch/supersede/stop semantics preserved; board workload evidence |
| `recovery_controller` | source lifecycle restart/backoff after a source fault | BSP reset/quiesce handshake; no fake ACK; release-FW fault test |

## Alternatives

- Wire the legacy scheduler now instead of removing it: rejects the delivered retained-frame
  cascade and duplicates `cascade_coordinator`. Rejected.
- Keep all three with no status: leaves ambiguous dead infrastructure that the clean-base
  goal forbids. Rejected.
- Remove `inference_worker`/`recovery_controller` too: they cover roadmap capabilities that
  are not yet delivered; removing them would discard planned, tested building blocks. Only
  the superseded duplicate was removed.

## Consequences

- `src/core`, `src/runtime/scheduler` and the test suite lose one target and its test; the
  naming registry drops `secin`, `secsd`, `sitst`.
- `cascade_inference.md` and `face_recognition_completion_plan.md` no longer reference the
  removed header.
- `inference_worker` and `recovery_controller` remain in the default build but must not be
  described as integrated until their wiring gate is met.
