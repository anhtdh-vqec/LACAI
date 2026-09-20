# Feature activation manager

`feature_activation_manager` is the cold-path owner that turns validated deployment, model
and feature catalogs into effective per-source feature state. This document defines its
reconciliation order, its explicit state model and its bounded storage.

**Status:** source-delivered — the manager exists with contract tests. **Layer:** runtime.
**Source:** `src/runtime/feature_manager/vqec_vision_feature_activation_manager.cpp`,
`tests/contract/runtime/vqec_vision_feature_activation_manager_test.cpp`.

## Responsibility

- Turns validated deployment, model and feature catalogs into effective per-source feature
  state.
- Records the feature catalog, model catalog and deployment revisions in the activation
  snapshot so downstream output and audit paths can reject stale state.
- Borrows catalogs and factories as immutable inputs.
- Does not authenticate catalogs, resolve FW RAW sources, load model artifacts, or claim
  board/hardware support.
- Does not create an unbounded registry or queue.

## Reconciliation

For each `(source_id, feature_id)` request the manager evaluates desired enablement,
entitlement authorization, resource admission, model dependency assignment, then
compiled-in processor factory resolution and stage activation.

The resulting state is explicit: `disabled`, `denied`, `unsupported`,
`resource_limited`, `ready` or `faulted`. A missing processor contract is an
`unsupported` capability. Factory, configuration or stage errors are `faulted` and the
first such error is returned while other associations are still reconciled.

Request syntax and duplicate associations are checked before candidate state is built;
malformed input leaves the previous records and snapshot untouched. Reconciliation is
bounded to 16 sources × 64 feature entries. Every `ready` association owns a distinct
processor and `feature_stage`, which prevents stateful feature packages from being shared
accidentally across sources.

Production requests carry the immutable scoped usecase projection. Before construction,
the manager validates source/feature, exact assigned model slot, deployment/model revisions,
configuration/policy revisions, authority booleans and attribute dependency schema IDs.
A mismatch rejects the candidate transaction; records retain the validated association.
The unscoped request path remains for explicit development fixtures.

Once borrowed, a manager is frozen. A separately reconciled candidate may adopt processor
and stage owners from that frozen manager only when catalog/deployment object identity and
all association identity, configuration and attribute-scope fields match exactly. Adoption
moves ownership rather than copying state; changed or disabled associations never inherit
temporal state. Contract tests assert both retained pointer identity and replacement on a
configuration revision change.

## Limits and next work

- Authentication, FW RAW source resolution, model artifact loading and board/hardware
  support claims remain with the authenticated composition and platform adapters.
- Authenticated admission/entitlement activation remains external integration work; the
  manager does not grant authorization by itself.

## See also

- [Feature catalog](feature_catalog.md)
- [Feature processor registry](feature_processor_registry.md)
- [Feature stage](feature_stage.md)
- [Runtime feature activation](runtime_feature_activation.md)
