# Application composition

The serialized composition binds 1..16 already admitted, idle source sessions and
activates the multi-source supervisor only after every slot is bound. Authentication,
resource admission and construction of the platform owners are caller prerequisites;
a binding is not an authorization decision. Session owners outlive composition and
must reach stopped before destruction; no destructor performs hardware cancellation.

`step` advances one supervisor action. A completed tensor and its complete source/model
progress report occupy one pending result slot. `take_result` transfers both together,
once; an empty take preserves caller outputs. While occupied, further steps return
pending without advancing sessions. The executor must consume this slot promptly,
including during shutdown, to allow drain to progress. Stop requests still latch while
a result awaits delivery. Taking a tensor is not permission to publish it: downstream
feature/output policy checks remain mandatory.

Stop is allowed immediately after activation and is repeatable while draining. Later
steps reconcile all sessions. Recovery reflects actual session recovery flags, not
ordinary drain or pending output. The first session fault remains visible in the
composition snapshot even when supervisor progress isolates it as pending. Invalid
revalidation does not overwrite active lifecycle state; sentinel/backward times are
rejected before changing the clock.

This is source-delivered orchestration, not a runnable service or board qualification.
Contract tests cover pending result correlation, delivery backpressure, early/repeated
stop, drain completion and invalid-time preservation. Target execution and lead plus
runtime-owner review remain required before live integration acceptance.

`runtime_composition_factory` now supplies the cold-path neutral construction layer that
creates admitted multi-model sessions and perception bundles, binds every source, and
returns this composition after validation. Platform RAW/graph owner creation and
authenticated artifact/evidence resolution remain outside this class.
