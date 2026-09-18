# Feature processor factory registry

`feature_processor_registry` is the cold-path composition boundary for compiled-in usecase
packages. This document defines how processors are registered, created and bounded without
switching on commercial feature names.

**Status:** source-delivered — the registry exists with contract tests. **Layer:** runtime.
**Source:** `src/runtime/feature_manager/vqec_vision_feature_processor_registry.cpp`,
`tests/contract/runtime/vqec_vision_feature_processor_registry_test.cpp`.

## Responsibility

- Lets a package register one borrowed `feature_processor_factory_port` for each supported
  `processor_contract`.
- Ensures the generic runtime never switches on commercial feature names.
- Cross-checks the feature catalog entry, the configuration schema identity and a
  configuration revision at creation time.
- Does not load shared libraries, authenticate configuration, enforce entitlement, create
  `feature_stage` or decide source/model routing.

## Creation contract

The generic payload is bounded to 256 KiB and borrowed only for the factory call. Its
syntax and semantic validation belong to the factory named by the catalog contract. A
successful factory call returns a distinct `feature_processor_port` owner, which must copy
every configuration value it retains and must not depend on factory lifetime.

Registry capacity is 64 contracts and registration is activation-time only. Contract
lookup and creation are serialized with package lifecycle. Duplicate/unknown contracts,
schema mismatch, oversized configuration, exceptions and a successful factory call that
returns no owner all fail closed. The caller's existing processor owner is preserved on
every failure.

## Composition root duties

The composition root must authenticate catalogs and configuration, build/admit the
activation snapshot, create one processor per stateful source/feature association and keep
it alive through stage drain.

## Limits and next work

- The registry does not authenticate configuration or enforce entitlement.
- It does not create `feature_stage` or decide source/model routing.
- Registration is activation-time only and capacity is fixed at 64 contracts.

## See also

- [Feature catalog](feature_catalog.md)
- [Feature stage](feature_stage.md)
- [Multi-model feature pipeline](multi_model_feature_pipeline.md)
