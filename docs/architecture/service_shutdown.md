# Service shutdown owner

This document defines the single owner that drains one service generation and maps its
complete lifecycle outcome to a process result.

**Status:** logic-tested — direct exit-policy and service characterization tests pass under
the approved eSDK/QEMU profile. **Layer:** app.
**Source:** `src/app/service/generation/vqec_vision_service_shutdown.{hpp,cpp}`,
`tests/unit/application/vqec_vision_service_shutdown_test.cpp`.

## Responsibility

- Request executor stop, drain retained results and wait for the composition to report stopped.
- Drain cascade workers before stopping cascade graphs.
- Stop optional enrollment, metadata and evidence owners after the executor can no longer publish.
- Emit final metrics and map stopped/error/reconcile/source-count state to a stable exit result.
- Treat incomplete executor, enrollment or cascade shutdown as recovery-required.

The owner does not load configuration, construct a graph, poll a camera, process a result or
decide whether a replacement generation may start. All objects are borrowed from the generation
controller and must outlive the shutdown call.

## Exit policy

The pure exit decision has four outcomes:

| Condition | Result |
|---|---:|
| any required owner did not stop cleanly | recovery-required (`5`) |
| generation unpublished or first error present | failure (`1`) |
| clean drain requested by a newer control revision | reconcile (`4`) |
| clean drain and required source count reached | success (`0`) |

Recovery-required takes precedence over reconcile so the controller cannot replace a generation
while hardware or retained owners may still be live. The direct unit test covers every precedence
branch; executable characterization tests cover the real drain path.

## Limits and next work

- The current stop deadline remains the existing bounded service policy; released-BSP recovery
  after a real hardware timeout is outside this owner.
- Electrical power removal and released-FW media shutdown require their respective owner tests.

## See also

- [Runtime executor](runtime_executor.md)
- [Service enrollment runtime](service_enrollment_runtime.md)
- [Application composition](application_composition.md)
