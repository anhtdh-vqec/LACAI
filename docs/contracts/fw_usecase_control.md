# FW usecase activation contract

This document records the currently delivered FW/AI APP compatibility boundary through which FW
controls commercial AI usecases rather than individual models. It defines the effective-state
rule, the D-Bus v1 methods, the entitlement boundary and the reconciliation/hardware-lifetime
ordering. It is not the target product App Manager authority.

**Status:** source-delivered — the bounded D-Bus v1 adapter, the neutral desired-plan
manager and service-owned stop/drain/recomposition are delivered; live hardware acceptance
and FW product integration require the evidence below. **Layer:** contracts.
**Source:** `n/a`.

The product target moves commercial app management to the AI-owned
`com.vqec.AiVision.AppManager1` facade with an authenticated backend peer. This document records
the currently delivered `UsecaseControl1` compatibility seam; it must become an internal control
boundary during that migration. Backend product code must not drive both interfaces. FW/BSP has
no ownership of the target usecase package, entitlement, inventory or install lifecycle. See
[usecase app distribution](../planning/architecture_improvement/usecase_app_distribution_plan.md).

FW controls commercial AI **usecases**, not individual models. A usecase is the stable
product identity shown to licensing and UI. Its implementation may require one or more
primary models, secondary cascade models, feature processors and output permissions.
Those dependencies belong to the signed AI catalogs and are never supplied as paths or
plugin names by a D-Bus caller.

## Responsibility

- Defines the usecase activation boundary and the separate state gates per
  `(source_id, usecase_id)` association.
- Requires `desired=true` to express operator intent only; entitlement, compatibility and
  resource admission cannot be bypassed by a D-Bus caller.
- Must not load or schedule a model that no effective usecase references.

## Catalog entries

The first catalog entries are:

| `usecase_id` | Root workload | Dependent workload | Effective output |
|---|---|---|---|
| `person_detection` | `yolov8n_person` | none | person observations/overlay/events |
| `face_recognition` | `scrfd_500m_bnkps` | `edgeface_s_gamma_05` per eligible face | face observations, identity label/overlay and authorized events |

These identifiers are contract values, not executable conditionals. Future usecases are
added through a versioned usecase catalog and compiled capability registration.

## Effective-state rule

For every `(source_id, usecase_id)` association:

```text
effective = installed AND entitled AND desired AND supported AND compatible AND admitted
```

The service reports each gate separately. `desired=true` only records operator intent;
it never bypasses entitlement, compatibility or resource admission. Command acceptance
is distinct from `running` readiness.

When no effective usecase references a model, the next runtime generation must not open
its artifact, create its graph/context, allocate its tensor pools or schedule inference.
Shared model dependencies use reference counts and remain loaded while at least one
effective consumer exists. Hiding overlay or dropping events alone is not a valid disable.

## D-Bus v1

The deployed bus name and object root are installation configuration. The versioned
interface is `com.vqec.AiVision.UsecaseControl1`; examples use object
`/com/vqec/AiVision/UsecaseControl`.

AI APP resolves the configured FW well-known name to its unique sender when binding the
adapter and rejects calls from any other sender. Service bus name, object path, RPC timeout,
callback budget and system/session bus selection are deployment inputs; none are selected
from model identity.

### `ApplyDesiredPlan`

```text
ApplyDesiredPlan(
    s request_id,
    t expected_control_revision,
    a(ssb) entries
) -> (
    b accepted,
    t control_revision,
    s apply_state,
    u reason_code
)
```

Each entry is `(source_id, usecase_id, desired_enabled)`. The array is a complete desired
snapshot, bounded to 16 sources × 64 usecases. Omitted associations become disabled.
Duplicate associations, unknown identifiers, oversized input and stale revision are
rejected transactionally. `request_id` is a bounded idempotency key; retrying the same
payload returns the original result, while reusing it with another payload is rejected.

`accepted=true` means the desired snapshot was validated and queued. `apply_state` is one
of `unchanged`, `reconciling`, `running`, `degraded` or `failed`. FW must query status until
the published generation either runs or fails; it must not infer readiness from method
return alone.

### `GetUsecaseStatus`

```text
GetUsecaseStatus() -> (
    t control_revision,
    t entitlement_revision,
    t runtime_generation,
    a(ssbbbbbbbbss) entries
)
```

Each status entry contains, in order:

```text
source_id, usecase_id,
installed, entitled, desired, supported, compatible, admitted, loaded, running,
effective_state, reason
```

`effective_state` is one of `disabled`, `denied`, `unsupported`, `incompatible`,
`resource_limited`, `loading`, `running`, `draining` or `faulted`. `reason` is a bounded
stable machine string; human diagnostics remain separate and must not contain biometric
data, credentials or private paths.

### `GetCapabilities`

```text
GetCapabilities() -> (t catalog_revision, a(ss) usecases)
```

Each entry is `(usecase_id, usecase_version)`. Capability presence does not imply
installation, entitlement or admission.

## Entitlement boundary

Desired-state methods are not entitlement provisioning. The delivered compatibility seam
historically expects a separately authenticated provisioning path described in
[FW control](fw_control.md). In the target product, the authenticated backend supplies the signed
snapshot only through the AI-owned App Manager. AI APP verifies device/customer scope, validity,
revision and usecase/source limits before publishing it. No normal D-Bus caller can set
`entitled=true`; FW/BSP is not the target entitlement authority.

Revocation blocks newly emitted sensitive attributes/events immediately at the output
gate using the new entitlement revision, then triggers runtime reconciliation. Queued
output is rechecked before dispatch.

## Reconciliation and hardware lifetime

An accepted change resolves a complete candidate desired plan. The delivered serialized
replacement follows this order:

1. Resolve trusted gates, root models, dependency closure and configured resource admission.
2. Stop new scheduling/rendering for the obsolete generation.
3. Drain submitted work, reconcile source leases and unload obsolete graphs.
4. Destroy runtime owners only after successful drain; recovery-required blocks replacement.
5. Prepare and construct the complete effective candidate; inactive catalog models do not
   require decoder owners or model preparation.
6. Start source sessions and publish only after every session reports running. All-off
   publishes an empty generation and retains only the control loop.

Incremental replacement follows ADR 0012: compatible desired/configuration/entitlement changes
retain unrelated sources and exact shared dependencies, while incompatible identity/capacity
changes use an explicit replacement boundary. Both paths preserve those ownership/output/readiness
gates. Session readiness does not prove numerical warmup or continued source health.

The target App Manager runtime consumer is startup-order independent. If App Manager or backend is
absent, the service starts with a valid empty effective generation and periodically retries a full
snapshot read through its configured trusted D-Bus client name. Only a strictly newer complete
snapshot can trigger replacement; transport failure and stale data fail closed. Entitlement expiry
is evaluated from the local UTC clock and triggers drain even when the control plane is absent.

The compatibility implementation may stop and rebuild a complete runtime generation. The product
App Manager path must apply compatible toggles incrementally. Neither path may release buffers
until actual backend completion. A failed candidate leaves the last valid generation running when
policy still authorizes it; revocation always blocks unauthorized output even if teardown fails.

The current delivered service implements serialized full-generation replacement in the same
process; ADR 0012 records the required product replacement. The usecase D-Bus object outlives the
runtime owners. Scheduling/rendering stops
before draining the old generation; after successful drain its owners are destroyed and the effective
model deployment is reconstructed. All-off keeps only the control loop. FR enrollment
is available only while its runtime is enabled; its authenticated gallery survives disable.
The configured FW peer must retain its unique connection across transitions. D-Bus work
is polled between runtime steps; synchronous model preparation and image enrollment can
delay replies. Bounded asynchronous preparation/enrollment remains a production gate.
Desired-plan receipts/revisions are currently process-local; restart restores the trusted
startup snapshot rather than persisting live commands. Signed entitlement provisioning
and hardware state observation remain separate unfinished boundaries.

If every usecase for a source is disabled and preview is not independently contracted,
AI APP releases the camera lease. If all usecases are disabled, no model graph is loaded.

## Failure and restart semantics

- Desired plan and entitlement snapshots have independent monotonic revisions.
- Runtime generation is monotonic and changes only after a successful publish.
- Service restart restores only authenticated snapshots; it never defaults all installed
  usecases to enabled.
- Crash during candidate construction cannot publish a partial plan.
- Unknown usecases fail closed and consume no model resources.
- Admission failure reports the affected association and keeps it non-running.
- Health reports desired/effective divergence and the last transition reason.

## Acceptance criteria

FW and AI APP acceptance must demonstrate on QCS6490 that:

1. Enabling only `person_detection` loads/schedules YOLO and no face models.
2. Enabling only `face_recognition` loads SCRFD and EdgeFace and no person model.
3. Enabling both preserves person plus face/name overlays from one acquired frame stream.
4. Disabling one usecase stops its submissions and releases its unshared graph/pools after
   drain while the other continues.
5. Disabling all releases every graph/context and the camera lease.
6. An unentitled request is accepted only as desired state and remains `denied`; it loads
   no model and emits no protected output.
7. Revoke during in-flight inference blocks output immediately and safely drains hardware.
8. Repeated/stale/concurrent requests obey idempotency and revision CAS.

Measured load/unload, RSS, accelerator utilization, CPU, temperature and clock-frequency
evidence is required; source inspection alone is not acceptance.

## Limits and next work

- Live hardware acceptance and FW product integration require the test evidence above.
- Signed entitlement provisioning, durable desired-plan receipts and hardware state
  observation remain separate unfinished boundaries.
- Current measured cases and release limits:
  [FR validation](../testing/face_recognition_production_validation.md).

## See also

- [FW control, outputs, entitlement and BSP handoff](fw_control.md)
- [FR production validation](../testing/face_recognition_production_validation.md)
