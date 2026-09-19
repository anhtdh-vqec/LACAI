# ADR 0009 — Spatiotemporal metadata tiering

Status: proposed — physical v1 choice has board evidence, but retention and failure gates remain.
Date: 2026-09-19.
Owner: AI APP lead.

## Context

The transactional baseline in ADR 0008 proves append-only revisions, an atomic outbox and
bounded selective queries. It does not prove that one generic table in one ever-growing SQLite
database can retain high-rate object observations, reconstruct a month-old multi-camera
footprint, answer spatial relations and serve yearly aggregates while live ingest continues.

The product needs both security and future traffic workloads. These include exact frame
drill-down, error-bounded trajectories, cross-camera association hypotheses, event episodes,
heatmaps and long-range statistics. Keeping every processed observation indefinitely conflicts
with the bounded storage and flash budget of an edge device. The retained resolution and its
error therefore have to be part of the public result semantics.

Identity, embeddings and plate values are ordinary metadata under the current product policy.
They use the normal configured metadata directory and retention system. Access domains still
enforce entitlement and output isolation; they are not an at-rest sensitivity classification.

## Proposed decision

- Expose one AI APP-owned metadata service and typed query API. No other process opens its
  database, shard or columnar files directly.
- Separate four explicit resolutions: exact retained observation, error-bounded trajectory,
  episode fact and aggregate. Responses report resolution, coverage, gaps and error bounds.
- Make frame locator, scene/coordinate revision, local tracklet, trajectory chunk, association
  revision, episode and aggregate contribution first-class version 1 contracts.
- Use a single-writer SQLite WAL catalog for scenes, track/episode headers, associations,
  manifests, rollups and the durable outbox.
- Partition high-rate detail by bounded time/source groups. First benchmark typed SQLite detail
  shards with packed trajectory chunks so point fields are delta encoded rather than stored as
  one SQL row per sample. Seal old shards for read-only access.
- Preserve immutable local tracklets. Cross-camera identity is a revisioned, scored association
  read model with topology, clock uncertainty and review state; it never rewrites source track
  IDs.
- Retain mandatory points at lifecycle, gap, scene, zone/line, attribute and episode boundaries.
  Between them, use a declared exact, fixed-gap or error-bounded sampling policy. Never invent a
  frame ID for an interpolated point.
- Serve live footprint through bounded RAM snapshot-plus-delta. Serve selective history from the
  catalog and shards. Route large historical scans/recomputation through cancellable bounded
  jobs rather than long transactions on the active database.
- Select packed SQLite detail shards for the version 1 edge tier. Keep Parquet as center
  interchange and an optional future cold tier; do not add DuckDB/Arrow/Parquet to the edge build
  without a pinned eSDK package and a measured benefit. A committed manifest publishes every
  future cold generation.
- Treat Q01–Q30 as stable capability groups implemented by a bounded typed query algebra, not as
  thirty fixed SQL statements or a natural-language-to-SQL boundary.
- Configure and admit separate exact-observation, footprint, episode, aggregate, evidence and
  export-spool horizons from measured cardinality, storage quota and compaction reserve.

ADR 0008 remains valid evidence for the transactional catalog/outbox primitive. This proposal
supersedes its assumption that the generic single-file adapter is a complete P2 architecture.

## Alternatives

- One generic SQLite table and one database file: rejected as the target because point-level
  volume, unrelated indexes, WAL checkpoint pressure and long scans share one failure domain.
- One SQL row per detection: rejected for retained trajectories because it repeats identifiers
  and attributes, amplifies indexes and does not express sampling/error semantics.
- Parquet-only local authority: rejected because immutable files do not replace transactional
  revision, outbox, active episode and manifest authority.
- DuckDB-only authority: not selected because it is optimized for analytical batches and has not
  passed the target packaging or mixed live-ingest workload gate.
- A custom binary database: rejected. A project-owned trajectory chunk codec is acceptable
  inside a transactional shard, but rebuilding transactions, secondary indexes, recovery and
  query planning is not justified.
- PostgreSQL/TimescaleDB or ClickHouse on the camera: rejected for the edge baseline because of
  server operations and resource cost. They remain center-tier options outside this ADR.
- Store only event episodes: rejected because it cannot reconstruct footprints, arbitrary
  geometry inside the retained horizon or frame-correlated evidence.
- Store every observation forever: rejected because capacity is unbounded and cannot be admitted
  against a finite device quota.

## Consequences

- The query contract must expose resolution and coverage instead of promising silent exactness.
- Storage, compaction and query planning become a subsystem rather than a single adapter.
- The design adds manifests, shard lifecycle, cursor leases and correction-aware aggregates, but
  isolates high-rate writes from historical scans and supports explicit capacity admission.
- Cross-device footprint and year-scale analytics normally execute at the center tier; the edge
  still provides its configured local history and export receipts.
- P2 stays reopened until the version 1 contracts, representative generator, concurrent target
  benchmark and one physical tier choice are accepted.

## Acceptance required

1. AI APP lead approves the version 1 logical contracts and resolution semantics.
2. S01–S18 plus traffic fixtures exercise live, selective, spatial, sequence, month aggregate,
   correction, gap and cross-camera queries.
3. QCS6490 measures simultaneous ingest, footprint reads, long queries, compaction and
   Kafka-offline outbox pressure, including CPU, RSS, latency, flash bytes and FPS impact.
4. Retention profiles fit the assigned quota with WAL, active shards, compaction, spool and
   reserve included.
5. The selected detail implementation passes crash, corruption, disk-full and reader-drain tests
   before this ADR changes to accepted.

## Current evidence

On 2026-09-19 the packed-shard implementation passed 106/106 eSDK tests. A five-minute native
QCS6490 run committed 45,081 records for 27 security/traffic scenario profiles with no rejected
record, failed query or oracle failure while the full AI workload ran. Aggregate query p50/p95/p99
was 2.688/12.454/18.546 ms; metadata used 30.110% of one core, 37,120 KiB maximum RSS and
32,567,296 store bytes. The exact benchmark binary SHA-256 was
`c9945697b847ae64c8bb664011579c38ef3bdf03ebede76f2418907456a22190`.

This satisfies the representative physical-tier comparison needed to reject an additional edge
columnar dependency for v1. It does not satisfy retention sizing, power interruption, disk-full,
reader-drain or production composition gates, so this ADR remains `proposed`.

## See also

- [Spatiotemporal metadata architecture](../architecture/spatiotemporal_metadata.md)
- [Metadata storage source review](../research/metadata_storage_source_review.md)
- [Transactional metadata baseline](0008_transactional_metadata_store.md)
- [Metadata redesign plan](../planning/architecture_improvement/metadata_query_plan.md)
