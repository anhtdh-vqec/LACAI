# Feature activation manager

`feature_activation_manager` is the cold-path owner that turns validated deployment,
model and feature catalogs into effective per-source feature state. It keeps catalog
revisions in the activation snapshot so downstream output and audit paths can reject
stale state.

For each `(source_id, feature_id)` request the manager evaluates desired enablement,
entitlement authorization, resource admission, model dependency assignment, then
compiled-in processor factory resolution and stage activation.

The resulting state is explicit: `disabled`, `denied`, `unsupported`,
`resource_limited`, `ready` or `faulted`. A missing processor contract is an
`unsupported` capability. Factory, configuration or stage errors are `faulted` and the
first such error is returned while other associations are still reconciled.

Catalogs and factories are borrowed immutable inputs. Every `ready` association owns a
distinct processor and `feature_stage`, which prevents stateful feature packages from
being shared accidentally across sources. Request syntax and duplicate associations
are checked before candidate state is built; malformed input leaves the previous
records and snapshot untouched. Reconciliation is bounded to 16 sources × 64 feature
entries and does not allocate an unbounded registry or queue.

This manager does not authenticate catalogs, resolve FW RAW sources, load model
artifacts, or claim board/hardware support. Those responsibilities remain with the
authenticated composition and platform adapters described by the deployment and
Qualcomm adapter contracts.
