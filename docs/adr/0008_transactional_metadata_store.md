# ADR 0008 — Transactional metadata store baseline

Status: accepted — transactional prototype primitive; P2 target architecture is reopened by ADR 0009.
Date: 2026-09-19.
Owner: AI APP lead.

## Context

Security and traffic queries need interval-correct attributes, trajectories, episodes,
aggregates, provenance, coverage and durable delivery state. A plain event log cannot answer
point and interval lookups efficiently, while a columnar archive alone is a poor authority for
small transactional revisions, idempotent retries and an outbox. The target must remain
offline-capable and must not couple frame processing to storage latency.

The approved eSDK and QCS6490 image provide SQLite. They do not currently provide a reviewed
DuckDB, Arrow or Parquet production dependency. The original acceptance did not exercise
representative high-rate trajectory retention, cross-camera footprint, concurrent compaction or
month/year analytical workloads.

## Decision

- Use SQLite WAL as the version 1 transactional catalog/outbox prototype behind a neutral
  metadata query contract. The adapter is blocking and runs outside all frame/inference/DSP
  workers.
- Store typed indexed envelope fields plus a bounded cold payload. Do not accept client SQL or
  use payload text as the only search representation.
- Commit a fact revision and all requested outbox rows in one transaction. Treat local commit,
  broker acknowledgement, archive publication and evidence-media completion as different
  receipts.
- Use append-only revisions, exact idempotency comparison, prepared statements, a fixed
  snapshot sequence, keyset paging, field projection, scope checks and explicit coverage.
- Keep Q01–Q30 identities stable. Return `unsupported` for a capability whose producer or
  access path is absent; do not synthesize an empty successful result.
- Defer immutable Parquet history and DuckDB/Arrow readers until a reviewed dependency passes
  eSDK packaging/license/SBOM gates and the same representative dataset shows an approved
  latency, RSS, CPU or flash benefit on QCS6490. ADR 0009 separately evaluates detail shards,
  packed trajectories and the cold tier.

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

- The prototype has a recoverable transaction/outbox primitive and deterministic export seam.
  QCS6490 fixture measurements are recorded in the architecture doc.
- The generic table and single database file are not accepted as the complete metadata
  architecture. Large detail and historical data require the tiering decision in ADR 0009.
- Per-usecase producer quality, product retention, purge execution, Kafka export and service
  composition remain separately admitted capabilities. Accepting this ADR does not claim all
  18 usecases are implemented or sized.
- Identity, embedding and plate fields use the ordinary configured metadata storage directory
  under current product policy. Their access domains still enforce entitlement and output
  isolation. A deployment may add stricter at-rest controls as policy, but encryption is not a
  baseline activation prerequisite for these metadata families.
