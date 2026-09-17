# Tracker registry

`tracker_registry` is the activation-time bridge from a product-neutral tracker
contract to a compiled-in implementation factory. The registry stores borrowed factory
references and is bounded to 64 contracts. It never includes association state or
vendor types.

**Status:** source-delivered — activation-time tracker factory registry source exists.
**Layer:** perception.
**Source:** `src/perception/tracking/vqec_vision_tracker_registry.{hpp,cpp}`.

## Responsibility

- Resolve a product-neutral tracker contract to a compiled-in factory and return a
  distinct `tracker_port` owner.
- Preserve the caller's previous owner when contract resolution, binding validation,
  factory validation or creation fails.
- Must not select an algorithm, authorize a feature, resolve model artifacts or claim
  hardware support; those decisions stay in authenticated activation and platform
  composition.

## Contract resolution

Composition supplies the source and model identities for each binding. A factory must
validate that binding and return a distinct `tracker_port` owner. Null owners are
rejected. Allocation and factory exceptions map to bounded status codes.

The resulting tracker is passed to `tracking_stage`, which remains the serialized
epoch/clock/validation coordinator.

## Limits and next work

- The registry is bounded to 64 contracts and stores borrowed factory references only.
- Algorithm selection, feature authorization, model artifact resolution and hardware
  support claims stay outside this class.

## See also

- [tracking stage](tracking_stage.md)
- [perception stage factory](perception_stage_factory.md)
