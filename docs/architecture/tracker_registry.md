# Tracker registry

`tracker_registry` is the activation-time bridge from a product-neutral tracker
contract to a compiled-in implementation factory. The registry stores borrowed factory
references and is bounded to 64 contracts. It never includes association state or
vendor types.

Composition supplies the source and model identities for each binding. A factory must
validate that binding and return a distinct `tracker_port` owner. The registry preserves
the caller's previous owner when contract resolution, binding validation, factory
validation or creation fails; null owners are rejected. Allocation and factory
exceptions map to bounded status codes.

The resulting tracker is passed to `tracking_stage`, which remains the serialized
epoch/clock/validation coordinator. The registry does not select an algorithm,
authorize a feature, resolve model artifacts or claim hardware support. Those decisions
stay in authenticated activation and platform composition.
