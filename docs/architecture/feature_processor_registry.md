# Feature processor factory registry

`feature_processor_registry` is the cold-path composition boundary for compiled-in
usecase packages. A package registers one borrowed `feature_processor_factory_port` for
each supported `processor_contract`; the generic runtime never switches on commercial
feature names.

Creation cross-checks the feature catalog entry, the configuration schema identity and a
configuration revision. The generic payload is bounded to 256 KiB and borrowed only for
the factory call. Its syntax and semantic validation belong to the factory named by the
catalog contract. A successful factory call returns a distinct
`feature_processor_port` owner, which must copy every configuration value it retains and
must not depend on factory lifetime.

Registry capacity is 64 contracts and registration is activation-time only. Contract
lookup and creation are serialized with package lifecycle. Duplicate/unknown contracts,
schema mismatch, oversized configuration, exceptions and a successful factory call that
returns no owner all fail closed. The caller's existing processor owner is preserved on
every failure.

The registry does not load shared libraries, authenticate configuration, enforce
entitlement, create `feature_stage`, or decide source/model routing. The composition root
must authenticate catalogs and configuration, build/admit the activation snapshot,
create one processor per stateful source/feature association and keep it alive through
stage drain.

See [feature catalog](feature_catalog.md), [feature stage](feature_stage.md) and
[multi-model feature pipeline](multi_model_feature_pipeline.md).
