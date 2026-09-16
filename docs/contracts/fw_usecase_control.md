# FW usecase activation contract

Status: normative AI APP boundary; transport implementation and FW integration are pending.

## Purpose

FW controls commercial AI **usecases**, not individual models. A usecase is the stable
product identity shown to licensing and UI. Its implementation may require one or more
primary models, secondary cascade models, feature processors and output permissions.
Those dependencies belong to the signed AI catalogs and are never supplied as paths or
plugin names by a D-Bus caller.

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

Desired-state methods are not entitlement provisioning. FW installs a signed entitlement
snapshot through the separately authenticated provisioning path described in
[FW control](fw_control.md). AI APP verifies device/customer scope, validity, revision and
usecase/source limits before publishing it. A normal D-Bus caller cannot set
`entitled=true`.

Revocation blocks newly emitted sensitive attributes/events immediately at the output
gate using the new entitlement revision, then triggers runtime reconciliation. Queued
output is rechecked before dispatch.

## Reconciliation and hardware lifetime

An accepted change constructs a complete candidate runtime generation:

1. Resolve effective usecases from trusted catalog, entitlement and desired plan.
2. Expand each effective usecase to primary model roots and compute the dependency closure.
3. Perform compatibility, memory, accelerator, encoder and thermal admission.
4. Build all required model graphs, pools, processors and output routes without exposing
   a partial generation.
5. Atomically publish the candidate, then start its source sessions.
6. Block removed outputs, stop scheduling removed work, drain submitted hardware work and
   unload only dependencies whose reference count reached zero.

The initial implementation may stop and rebuild the complete runtime generation. It must
still keep command/state reporting live and must not release buffers until actual backend
completion. A failed candidate leaves the last valid generation running when policy still
authorizes it; revocation always blocks unauthorized output even if teardown fails.

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
