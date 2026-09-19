# Usecase application manager

This document defines the AI-owned application lifecycle boundary used to distribute, configure
and activate the stable S01–S18 usecases without putting package I/O in the inference hot path.

**Status:** board-smoke — signed first install, entitlement, configuration CAS, desired-state
reconcile, persistent inventory, D-Bus facade and startup-order independence passed on the recorded
QCS6490 candidate; an immutable content-store adapter is logic-tested but package ingest,
operation journaling, update/rollback and backend conformance remain open.
**Layer:** app. **Source:** `config/schemas/usecase_app_manifest.schema.json`,
`config/schemas/runtime_control_snapshot.schema.json`,
`config/schemas/fire_smoke_configuration.schema.json`.

## Responsibility

- Own verified package staging, immutable content, transactional inventory and operation journal.
- Verify entitlement and derive independent lifecycle gates before publishing a runtime snapshot.
- Expose the bounded `AppManager1` version 1 control facade to separately configured backend
  mutation and runtime read peers.
- Never acquire frames, own tensors, run models or perform evidence media encoding.
- Never infer installation from a directory scan or accept authority fields from a package.

## Authority and process boundary

App Manager is an AI APP control-plane process. Backend is a producer of requests and signed
grants, not an installation or entitlement authority inside LACAI. BSP/FW is outside this
lifecycle. The shared inference service consumes only an immutable snapshot through a neutral
port; D-Bus, SQLite and filesystem types do not enter core contracts.

| Gate | Authority |
|---|---|
| catalog available | verified AI-owned catalog |
| entitled | verified scoped grant and local expiry/revocation policy |
| installed | committed inventory receipt with complete dependency closure |
| desired | authenticated backend request with expected revision |
| supported | compiled processor/capability registry |
| compatible | target/runtime/schema/dependency resolver |
| admitted | hardware/resource admission for the complete candidate |
| loaded/running | published runtime generation and component readiness |

Effective activation requires every gate through admitted. Install commits desired false. Disable
blocks scheduling and output before draining; uninstall never implies business-data purge.

## Package and configuration

`.vqapp` version 1 is declarative. It contains one strict manifest and immutable payload entries;
it cannot contain symlinks, devices, hard links, absolute paths, traversal paths, native plug-ins
or install scripts. The manifest requests scopes but cannot grant them. Exact dependency identity
is component ID, component version, target ID, artifact SHA-256 and semantic-contract SHA-256.

Configuration is a separate revisioned transaction. A package declares one schema and bounded
defaults. App Manager validates the requested document, entitlement limits and model operating
envelope, then publishes a typed immutable payload in a new runtime snapshot. Runtime never parses
JSON per frame.

## Operation and recovery contract

The current baseline executes bounded manifest/config/grant validation and inventory transactions
synchronously with CAS revisions. The target operation journal adds an idempotency key, payload
digest and explicit asynchronous operation state. Until that journal ships, callers must not infer
queued/cancel/update/rollback semantics from the current methods.

Package ingest copies exactly the declared bytes from a read-only FD into a private staging file
while calculating the digest. Commit order is payload fsync, candidate receipt fsync and atomic
inventory publication. Recovery selects a complete old or new revision. Orphan staging is not an
installed application and may be garbage-collected after journal reconciliation.

## Runtime snapshot

The runtime snapshot is complete, bounded and revisioned. Each association carries app/source
identity, the independent effective gates, exact configuration revision/digest and authorized
output scopes. Runtime rejects stale revisions and unsupported schema versions. A D-Bus signal is
only a wake-up; after reconnect or revision gap the consumer fetches the complete snapshot.

Entitlement expiry and revocation are enforced locally at scheduling and output boundaries. An App
Manager outage may keep a previously committed non-expired snapshot running, but cannot extend a
grant or authorize new output.

The inference service does not require App Manager or backend startup ordering. When App Manager is
unavailable at service startup, the runtime publishes no active source/model generation and polls
for the complete snapshot at the configured bounded interval. A higher snapshot revision requests
a serialized generation drain and reconciliation. A missing or stale reply never enables an
association. Locally observed entitlement expiry also requests reconciliation even while App
Manager is offline.

## D-Bus facade

Production uses the system bus and binds separately configured backend and runtime well-known names
to their unique owners on every request. Only the backend peer may mutate lifecycle state. The
runtime peer may only call `GetSnapshot`; backend may also read it for status/revision handling.
The daemon can start while either peer is offline, and a restart does not retain authority from a
previous unique owner. The two names must be different. The source-delivered facade currently
provides FD-based signed install, FD-based signed entitlement, configuration apply, desired state,
uninstall and complete snapshot reads. Large package/grant data never travels as a byte array.
Wrong sender, stale revision, invalid/expired/device-mismatched grant or not-installed enable fails
closed even if the UI hides an action. Asynchronous operation status/cancel, update/rollback and
catalog-list methods remain product work and must not be inferred from the current interface.

Per-app CPU/RAM is attributed work and shared-cost metadata, not a fabricated `/proc` process
value, because applications share the runtime process and components.

The runtime consumer owns its configured well-known client name and App Manager resolves it to the
current unique owner for every `GetSnapshot` call. The service name, both peer names, object
path, RPC timeout, poll interval and system/session bus selection are deployment configuration;
they are not model or usecase constants. Snapshot polling is level-triggered: reconnect never
depends on receiving an earlier D-Bus signal.

## Package signature baseline

The baseline package signature is a raw 64-byte Ed25519 signature. The signed byte sequence is
the ASCII domain `VQEC-LACAI-VQAPP-1`, the manifest length as an unsigned 64-bit big-endian
integer, the exact manifest bytes, the configuration length in the same encoding and the exact
configuration bytes. Length framing prevents concatenation ambiguity; artifact SHA-256 values
are checked independently and are not treated as authentication.

The verifier opens one configured absolute PEM public-key path with `O_NOFOLLOW`, accepts only a
regular file owned by root or the service UID, and rejects group/other-writable trust material.
It verifies the signature before parsing or accepting manifest authority. A production image must
provision the public key outside the app content store and bind a reviewed key ID into service
configuration. Private keys and test-generated keys never ship on the device.

## Entitlement signature baseline

Entitlement v1 is a strict signed JSON document binding grant/revision, issuer/key/customer,
machine ID, target, app, source, validity interval and output scopes. The signed byte sequence is
the ASCII domain `VQEC-LACAI-ENTITLEMENT-1`, the exact document length as unsigned 64-bit
big-endian and the exact document bytes. App Manager checks the configured key ID, machine/target,
UTC validity, installed association, requested-scope subset and signed expected entitlement
revision before publishing it. Replay with a stale revision and package presence without a signed
grant both fail closed.

Backend supplies only the signed entitlement. Compiled processor support is derived from the AI
registry, compatibility from verified target/runtime/package checks, and admission from the
configured App Manager resource capacity. These booleans are never accepted from the D-Bus caller.

## Limits and next work

- Rotation, revocation and multi-key trust-store policy still need supply-chain owner approval;
  this baseline intentionally accepts one configured Ed25519 public key.
- Content-addressed artifact staging is source-delivered and logic-tested independently. Binding
  package component FDs, inventory generation references and safe garbage collection to that
  adapter remains open together with operation journaling, update/rollback, D-Bus/backend
  conformance, fault injection and release acceptance.
- Released FW evidence service is a separate contract and does not affect install authority.
- The first-install board path is accepted only at board-smoke level. It does not make the
  synchronous mutation API an asynchronous operation journal and does not qualify component
  update/rollback.

## See also

- [AI-owned lifecycle ADR](../adr/0010_ai_app_manager_lifecycle.md)
- [System architecture](system_architecture.md)
- [Usecase activation](usecase_activation.md)
- [Application distribution plan](../planning/architecture_improvement/usecase_app_distribution_plan.md)
- [Fire/smoke product slice](../planning/architecture_improvement/fire_smoke_product_slice_plan.md)
