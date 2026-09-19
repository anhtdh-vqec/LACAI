# Service execution loop

This document defines the bounded hot-loop owner for one already-composed service generation.
It keeps per-generation progress state out of bootstrap, composition and shutdown code.

**Status:** board-smoke — direct cadence/context tests pass under the approved eSDK/QEMU profile;
the recorded QCS6490 run also passed no-frame startup, a camera outage longer than 120 seconds and
same-process recovery. **Layer:** app.
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

## Source-loss recovery

A source adapter may wait for its producer and reconnect without terminating the generation. If
the source instead reports `source_lost`, the execution loop continues bounded progress until the
composition is completely stopped and drained. Only that exact terminal combination requests a
generation replacement. QNN failures, timeouts and ambiguous faults remain fatal; they are not
converted into an unbounded retry loop.

The service shutdown owner observes the already-stopped composition without stepping it again,
releases the old generation, applies the validated `source_recovery_backoff_ms` policy and creates
fresh owners from the last committed control snapshot. Graph preparation is still behind the new
generation's first-frame gate. A restart request never reuses an owner that may retain hardware
access.

The closing board test stopped the compatibility camera cleanly for more than 120 seconds. The
ring stopped while the service PID remained alive. Once media returned, the drained source-loss
generation was replaced after a 1,000 ms backoff and the ring resumed 150 frames in five seconds
with the same process PID. This is compatibility-board evidence, not released-FW acceptance.

## Limits and next work

- Model-quality and released-FW timing are separate acceptance gates.
- Shared-work attribution across multiple installed applications requires the future capacity
  profile; this loop exposes execution facts but does not fabricate per-app process CPU values.

## See also

- [Runtime executor](runtime_executor.md)
- [Service shutdown owner](service_shutdown.md)
- [Runtime composition factory](runtime_composition_factory.md)
- [Qualcomm submission lifecycle](qualcomm_submission_lifecycle.md)
