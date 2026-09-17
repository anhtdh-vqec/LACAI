# Application composition

The serialized composition binds 1..16 already admitted, idle source sessions and
activates the multi-source supervisor only after every slot is bound. This document
defines that orchestration boundary, its single pending-result handoff and its stop
semantics.

**Status:** source-delivered — source-delivered orchestration; contract tests cover
pending result correlation, delivery backpressure, early/repeated stop, drain completion
and invalid-time preservation. Target execution and lead plus runtime-owner review remain
required before live integration acceptance. **Layer:** app. **Source:**
`src/app/vqec_vision_application_composition.cpp`,
`tests/contract/vqec_vision_application_composition_test.cpp`.

## Responsibility

- Binds already admitted, idle source sessions and activates the multi-source supervisor
  only after every slot is bound.
- Treats authentication, resource admission and construction of the platform owners as
  caller prerequisites; a binding is not an authorization decision.
- Requires session owners to outlive composition and reach stopped before destruction.
- Must not perform hardware cancellation in a destructor.
- Transfers a completed tensor and its complete source/model progress report together.
- Must not treat taking a tensor as permission to publish it; downstream feature/output
  policy checks remain mandatory.

## Pending result handoff

`step` advances one supervisor action. A completed tensor and its complete source/model
progress report occupy one pending result slot. `take_result` transfers both together,
once; an empty take preserves caller outputs. While occupied, further steps return
pending without advancing sessions. The executor must consume this slot promptly,
including during shutdown, to allow drain to progress. Stop requests still latch while
a result awaits delivery.

## Stop and recovery

Stop is allowed immediately after activation and is repeatable while draining. Later
steps reconcile all sessions. Recovery reflects actual session recovery flags, not
ordinary drain or pending output. The first session fault remains visible in the
composition snapshot even when supervisor progress isolates it as pending. Invalid
revalidation does not overwrite active lifecycle state; sentinel/backward times are
rejected before changing the clock.

## Runtime construction and production service

`runtime_composition_factory` now supplies the cold-path neutral construction layer that
creates admitted multi-model sessions and perception bundles, binds every source, and
returns this composition after validation. Platform RAW/graph owner creation and
authenticated artifact/evidence resolution remain outside this class.

The concrete production service takes the source session's bounded preview mailbox after
each executor step. It caches completed observations by source/model and merges them only
when a new model result arrives, then renders every camera frame with that snapshot. Tensor
result backpressure and preview cadence remain separate; neither path creates an unbounded
frame queue.

## Limits and next work

- This is source-delivered orchestration, not a runnable service or board qualification.
- Target execution and lead plus runtime-owner review remain required before live
  integration acceptance.
- Platform RAW/graph owner creation and authenticated artifact/evidence resolution remain
  outside `runtime_composition_factory`.

## See also

- [Multi-source supervisor](multi_source_supervisor.md)
- [Runtime composition factory](runtime_composition_factory.md)
- [Runtime executor](runtime_executor.md)
