# Service execution loop

This document defines the bounded hot-loop owner for one already-composed service generation.
It keeps per-generation progress state out of bootstrap, composition and shutdown code.

**Status:** logic-tested — direct cadence/context tests and service characterization tests pass
under the approved eSDK/QEMU profile. **Layer:** app.
**Source:** `src/app/service/generation/vqec_vision_service_execution_loop.{hpp,cpp}`,
`tests/unit/application/vqec_vision_service_execution_loop_test.cpp`.

## Responsibility

- Poll the selected control peer with a bounded callback budget and stop on a newer revision.
- Start optional cascade graphs only after the associated source session crosses its first-frame
  gate.
- Advance the executor, route correlated feature results and project authorized metadata.
- Poll optional recognition/enrollment work without changing its graph ownership.
- Take retained preview frames, apply generation-local output cadence, prepare authorized overlay
  state and hand the frame to the production renderer.
- Return a compact execution result to the shutdown owner; never destroy or unload borrowed owners.

The loop receives only an already validated immutable context. It does not parse JSON, resolve a
package, open QNN/HTP during construction, create a camera source or publish a new generation.

## Generation-local state

Observation caches, identity labels, preview cadence phases and diagnostic rate-limit timestamps
are allocated per invocation. A disable/enable or configuration rollover therefore cannot inherit
phase or stale overlay/log state from the previous generation. Every source has an independent
fixed-capacity slot; no map grows with frame count.

Preview selection uses rational phase arithmetic. Full-rate or unspecified output passes every
retained frame; a lower configured rate advances only that source's bounded phase. Direct tests
cover full rate, 15/30 and 25/30 selection plus incomplete-context rejection.

## First-frame and startup-order rule

The generation controller may exist while camera, App Manager or backend is unavailable. The loop
can poll control state, but a cascade graph remains stopped until its primary source session enters
one of the post-frame startup phases. Primary Qualcomm graph creation follows the source-session
first-frame gate. Consequently process start, D-Bus availability and desired state alone cannot
load DSP/HTP.

## Limits and next work

- Model-quality and released-FW timing are separate acceptance gates.
- Shared-work attribution across multiple installed applications requires the future capacity
  profile; this loop exposes execution facts but does not fabricate per-app process CPU values.

## See also

- [Runtime executor](runtime_executor.md)
- [Service shutdown owner](service_shutdown.md)
- [Runtime composition factory](runtime_composition_factory.md)
- [Qualcomm submission lifecycle](qualcomm_submission_lifecycle.md)
