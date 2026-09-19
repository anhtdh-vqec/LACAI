# ADR 0008 — Transactional metadata store baseline

Status: accepted — AI APP lead-directed P2 baseline with eSDK and QCS6490 evidence.
Date: 2026-09-19.
Owner: AI APP lead.

## Context

Security and traffic queries need interval-correct attributes, trajectories, episodes,
aggregates, provenance, coverage and durable delivery state. A plain event log cannot answer
point and interval lookups efficiently, while a columnar archive alone is a poor authority for
small transactional revisions, idempotent retries and an outbox. The target must remain
offline-capable and must not couple frame processing to storage latency.

The approved eSDK and QCS6490 image provide SQLite. They do not currently provide a reviewed
DuckDB, Arrow or Parquet production dependency. There is no accepted 18-usecase retention and
cardinality profile that demonstrates a columnar tier is required.

## Decision

- Use SQLite WAL as the version 1 hot transactional authority behind a neutral metadata query
  contract. The adapter is blocking and runs outside all frame/inference/DSP workers.
- Store typed indexed envelope fields plus a bounded cold payload. Do not accept client SQL or
  use payload text as the only search representation.
- Commit a fact revision and all requested outbox rows in one transaction. Treat local commit,
  broker acknowledgement, archive publication and evidence-media completion as different
  receipts.
- Use append-only revisions, exact idempotency comparison, prepared statements, a fixed
  snapshot sequence, keyset paging, field projection, scope checks and explicit coverage.
- Keep Q01–Q30 identities stable. Return `unsupported` for a capability whose producer or
  access path is absent; do not synthesize an empty successful result.
- Defer immutable Parquet history and DuckDB/Arrow readers. Reopen the decision only when a
  reviewed dependency passes eSDK packaging/license/SBOM gates and the same representative
  dataset shows an approved latency, RSS, CPU or flash benefit on QCS6490.

## Alternatives

- SQL tables per usecase: rejected because shared tracks, passages, attributes and coverage
  would be duplicated 18 times and authorization/correction semantics would diverge.
- Parquet-only storage: rejected as the local transaction/outbox/revision authority; it remains
  a possible immutable cold tier.
- DuckDB-only storage: deferred because the target dependency and operational concurrency
  profile are not qualified.
- A custom binary trajectory store: rejected as the version 1 authority because it would
  recreate transactions, recovery, indexing, paging and query planning without evidence that
  SQLite misses the product budget.
- Kafka as local storage: rejected because the camera must query while offline and a broker
  acknowledgement is not a local durable read receipt.

## Consequences

- Version 1 has one recoverable source of truth and a deterministic export seam with low
  integration complexity. QCS6490 fixture measurements are recorded in the architecture doc.
- Large historical scans and long retention may eventually require a cold columnar tier. That
  tier must use committed manifests, revision deduplication and reader-generation drain; it
  cannot bypass the SQLite authority silently.
- Per-usecase producer quality, product retention, purge execution, Kafka export and service
  composition remain separately admitted capabilities. Accepting this ADR does not claim all
  18 usecases are implemented or sized.
- Query authorization does not protect files at rest. Sensitive D11/D12 activation requires
  enforced private database/WAL/shared-memory storage and an approved encryption/key policy;
  those controls are not delivered by the baseline adapter.
