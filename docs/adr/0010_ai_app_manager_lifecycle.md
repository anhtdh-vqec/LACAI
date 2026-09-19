# ADR 0010 — AI-owned usecase application lifecycle

Status: proposed — AI APP lead approval and backend conformance remain required.
Date: 2026-09-19.
Owner: AI APP lead.

## Context

LACAI needs to sell and operate each of the stable S01–S18 usecases as an independently
entitled application. A customer may download only granted applications, and only a verified,
installed application may be enabled. The current startup snapshot keeps the individual gates
separate in memory, but one input document can still self-assert installation, entitlement,
compatibility and admission. It is therefore a development seam, not a production authority.

Running one inference process per application would reacquire camera streams, duplicate model
contexts and memory pools, and prevent sharing exact model dependencies. Installation also needs
blocking package I/O, digest/signature verification, fsync and recovery, which do not belong in
the real-time frame path.

## Decision

- AI APP owns a separate App Manager control-plane process and its private inventory/content
  store. BSP/FW does not own usecase package lifecycle.
- Backend integrates only through `com.vqec.AiVision.AppManager1` version 1 on system D-Bus.
  Its configured well-known name has mutation authority. The separately named AI runtime has
  read-only `GetSnapshot` authority. Package bytes travel through a read-only Unix FD; D-Bus
  carries bounded control metadata.
- A usecase application is a declarative SKU and activation unit, not an inference process.
  LACAI keeps one shared runtime and shares dependencies only by exact immutable identity.
- Package version 1 contains no native plug-in and no install script. Feature processors remain
  compiled, reviewed LACAI capabilities; a package requests a processor contract but cannot add
  executable code.
- App Manager verifies entitlement, package integrity, target/runtime compatibility and complete
  dependency closure before atomically publishing an inventory revision. Install always ends in
  `installed_disabled`.
- Entitlement v1 is an Ed25519-signed strict document scoped to key/customer/device/target/app/
  source/time/output and a signed expected entitlement revision. Backend cannot submit
  `supported`, `compatible` or `admitted`; App Manager derives those gates from its compiled
  registry, verified target/package and configured resource capacity.
- `installed`, `entitled`, `desired`, `supported`, `compatible`, `admitted`, `loaded` and
  `running` remain independent states with different authorities. Output authorization is
  enforced from actual payload scope after activation, not only at the UI switch.
- App Manager publishes a complete immutable runtime-control snapshot by monotonically increasing
  revision. Signals are wake-ups only; runtime never reconstructs authority from deltas.
- Inventory and operation journals are separate from business metadata. Content blobs are
  immutable and digest-addressed; a database never stores model binary payloads.
- The source baseline uses synchronous revisioned transactions. Asynchronous idempotent operation
  journaling remains mandatory before remote rollout: accepted will mean queued, never installed
  or running. Recovery must resolve a transaction to the old or new committed inventory, never a
  half-installed directory scan.
- Every LACAI-owned schema and D-Bus contract starts at version 1 and uses the canonical version
  registry. A version increment needs a migration ADR.

## Alternatives

- One process per usecase: rejected because camera/model/accelerator ownership and memory would be
  duplicated while exact dependencies could not be shared safely.
- Let backend write the application directory: rejected because filesystem presence is not a
  verified install receipt and bypasses entitlement, compatibility and recovery.
- Reuse the operational metadata database: rejected because inventory, package transactions and
  business observations have different owners, retention and failure domains.
- Load native shared objects from `.vqapp`: rejected for the baseline because an entitled data
  package is not a reviewed code execution boundary.
- Keep startup booleans as production authority: rejected because one caller could self-grant
  unrelated gates and a restart could not prove committed inventory.

## Consequences

- AI APP must deliver strict manifest/config/snapshot parsers, transactional inventory recovery,
  an entitlement verifier port, a D-Bus facade and runtime reconciliation.
- Backend must follow AI-owned schema, CAS revision, idempotency and sender-binding rules.
- Compatible model updates can replace an immutable component reference without changing the
  processor source. Semantic or ABI changes require explicit compatibility work.
- Fire/smoke is the first complete application slice, but resolver, inventory, operations and
  runtime snapshot must not contain S04-specific branches.
- Released FW remains involved only in the evidence-media receiver contract, not application
  installation or control.

## Approve after (gates)

1. AI APP lead approves authority, state and recovery semantics.
2. Backend owner accepts the D-Bus method/state/revision contract and negative conformance cases.
3. Supply-chain owner selects the product trust store and reviewed signature primitive.
4. Power-loss, disk-full, duplicate request, stale revision and entitlement-revoke tests pass.
5. S04 install/config/enable/disable/update/rollback/uninstall passes on the recorded QCS6490.
