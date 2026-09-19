# Metadata query foundation

This document defines the AI APP-owned version 1 metadata envelope, transactional local
store and authorized query boundary shared by security and future traffic applications.

**Status:** board-smoke — eSDK tests and QCS6490 native test passed on 2026-09-19. **Layer:** adapters. **Source:** `include/vqec/vision/ai/contracts/vqec_vision_metadata_query.hpp`, `src/core/output/vqec_vision_metadata_query.cpp`, `src/adapters/storage/vqec_vision_sqlite_metadata_store.cpp`.

## Responsibility

- AI APP owns D01–D18 semantics, Q01–Q30 capability identity, local persistence, query
  authorization, paging and the record-plus-outbox commit.
- Model and FW producers submit facts through reviewed contracts; neither writes the SQLite
  file or exposes SQL to clients.
- The store is a blocking cold/output-path component. It must never execute on a frame,
  inference, DSP completion or renderer worker.
- Evidence media bytes, tensors, raw frames, face embeddings and model binaries do not enter
  this database. Records carry bounded facts and references only.

## Contract model

The machine catalog in `config/contracts/metadata_query_catalog.json` is the reviewable
authority for all 18 record families, 30 query identities, 18 security usecases and nine
traffic extension profiles. Every project-owned schema value is baseline version 1.

Each record has an immutable `record_id` and append-only `revision`, source epoch, valid and
recorded time, provenance revision, explicit value state, one sensitivity scope and a bounded
payload. A later revision names the immediately preceding revision. A tombstone is a later
revision; an initial tombstone is rejected. Unknown, not observable, unsupported and expired
are distinct states and must not be converted to false or an empty result.

The common envelope deliberately keeps indexed identity, time, family, subject, object,
scene, semantic type and typed value outside the payload. The payload is bounded cold detail;
it is not a JSON/EAV query escape hatch.

## Transaction and recovery

`sqlite_metadata_store` is a single-connection, single-writer adapter configured with an
explicit path, payload/page bounds, busy timeout, WAL checkpoint policy and `FULL` or `NORMAL`
synchronous mode. Production profiles must choose these values; source defaults are not
embedded.

One `BEGIN IMMEDIATE` transaction performs all of the following:

1. validate the canonical record and revision chain;
2. reject a stale revision or accept an exact idempotent retry;
3. insert the fact revision;
4. insert one pending row for each bounded outbox sink;
5. commit one SQLite receipt.

A successful commit is only a local durable receipt. It is not a Kafka acknowledgement,
archive publication or evidence-media receipt. Conflicting retries fail closed. Reopening the
database preserves the committed fact and outbox state.

## Query boundary

Clients submit a typed query kind and matching `Qxx` identity, source set, half-open time
interval, typed filters, explicit projection, authorization revision/scope, page bound and an
optional snapshot/cursor. SQL text is never accepted from a client.

The baseline has local prepared-statement access paths for Q01–Q06, Q08, Q12, Q14, Q15,
Q18 and Q26–Q29. The remaining query IDs fail with `unsupported`; they are still defined so
future model or traffic producers cannot invent incompatible semantics. Q29 reads
correction/purge audit state; it does not authorize or execute a purge operation.

Authorization is checked twice: the query kind requires its minimum scope, and every returned
row must intersect the caller's granted scope. Q02 requires both visual-attribute and
trajectory scopes because it joins an attribute interval to a passage. Projection clears
non-requested fields before records leave the adapter.

Paging uses a fixed maximum sequence snapshot and a monotonically increasing keyset cursor.
Records committed after the first page do not appear in later pages of the same snapshot.
Coverage records are evaluated for all requested sources; missing coverage returns `partial`,
not a misleading complete empty result.

## Target evidence and storage decision

The 2026-09-19 QCS6490 run used the user-authorized `lacai-home` target and the standard
`/opt/lacai` workspace. The native contract test passed, including temporal Q02, combined
scope denial, transactional outbox, idempotency conflict, unsupported capability, projection,
stable paging and reopen recovery.

The non-CTest benchmark uses `synchronous=FULL`, one transaction per record and Q08 page size
128. It is a storage fixture, not a product workload SLO.

| Records / queries | Insert records/s | Q08 p50 | Q08 p95 | Q08 p99 | Max RSS | DB bytes |
|---:|---:|---:|---:|---:|---:|---:|
| 2,000 / 200 | 15,121 | 1.625 ms | 2.018 ms | 2.093 ms | 5,248 KiB | 663,552 |
| 20,000 / 500 | 15,048 | 14.855 ms | 16.303 ms | 16.731 ms | 7,040 KiB | 6,651,904 |

SQLite remains the version 1 hot transactional baseline. The approved eSDK contains SQLite
but no reviewed DuckDB, Arrow or Parquet library, so a hybrid backend does not pass the
dependency/package gate and was not compared as if it existed. Columnar history is added only
after a representative workload exceeds an approved SQLite capacity/SLO and an equivalent
eSDK/board benchmark proves the benefit.

## Limits and next work

- The adapter is not yet composed into the service event/output path; the current board run
  qualifies the isolated native boundary, not live inference FPS impact.
- Query cases define all five required outcome classes for Q01–Q30, but model-quality golden
  data and producer-specific calibration remain activation gates for each usecase.
- Retention quotas, physical purge orchestration, Kafka delivery, archive manifests and
  asynchronous long-query jobs are later operational components. Q28/Q29 only expose the
  stable read/audit foundation delivered here.
- The SQLite adapter does not yet enforce a private parent directory or encrypt database,
  WAL and shared-memory files. Plate/identity metadata must remain non-admitted until the
  deployment supplies and verifies protected storage plus the approved at-rest key policy.
- The benchmark is not a sizing profile for 18 simultaneous usecases. A deployment must
  provide measured cardinality, retention and latency budgets before admission.

## See also

- [Transactional metadata store ADR](../adr/0008_transactional_metadata_store.md)
- [Metadata and query plan](../planning/architecture_improvement/metadata_query_plan.md)
- [Observation contract](observation_contract.md)
- [Three-team integration registry](../contracts/integration_contract_registry.md)
