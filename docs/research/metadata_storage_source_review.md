# Metadata storage source review

This dated review records upstream storage, trajectory and video-metadata facts used to
redesign the LACAI metadata subsystem. It is research evidence, not a target capability claim.

**Status:** source-delivered — read-only review completed on 2026-09-19. **Layer:** reference. **Source:** `n/a`.

Review date: 2026-09-19.

## Responsibility

- Separate upstream guarantees from LACAI design inferences.
- Compare transaction, trajectory, columnar and concurrency properties relevant to QCS6490.
- Prevent a format or engine name from being treated as a complete query architecture.

## Upstream evidence

| Source | Direct fact | LACAI implication |
|---|---|---|
| [SQLite WAL](https://www.sqlite.org/wal.html) | Readers and one writer can proceed concurrently; only one WAL writer exists; a long reader can stop checkpoint progress | Keep transactions short, own writes in one service and prevent long history scans from holding the active WAL |
| [SQLite R-tree](https://www.sqlite.org/rtree.html) | R-tree accelerates rectangle/time-range candidates, but exact geometry needs a second check; modifying a scanned R-tree can lock | Index immutable chunk bounds, then decode exact polylines; do not update the same active spatial index from a query callback |
| [Apache Parquet concepts](https://parquet.apache.org/docs/concepts/) | Files contain row groups and per-column chunks | Batch high-rate history into bounded immutable segments, not one file per track or frame |
| [Parquet page index](https://parquet.apache.org/docs/file-format/pageindex/) | Column and offset indexes permit page skipping and row navigation | Sort/partition choices and statistics determine selective-query cost; the `.parquet` suffix alone gives no trajectory index |
| [DuckDB concurrency](https://duckdb.org/docs/current/connect/concurrency) | In-process read/write supports multiple threads, but stable multi-process write is not the normal embedded model | If selected, one AI metadata service owns DuckDB; UI/backend processes must use its API rather than open the file |
| [DuckDB Parquet](https://duckdb.org/docs/lts/data/parquet/overview) | Projection/filter pushdown and row-group statistics reduce scans | A read-only analytical worker is a valid cold-tier candidate after eSDK packaging and board qualification |
| [Arrow C++ Parquet](https://arrow.apache.org/docs/cpp/parquet.html) | Parquet C++ belongs to Arrow and supports record-batch readers/writers | Arrow is a sizeable dependency boundary, not a header-only codec; cross-build/package/RSS must be measured |
| [OGC Moving Features](https://docs.ogc.org/is/22-003r3/22-003r3.html) | A moving feature has temporal geometry and temporal properties; APIs support bbox, time and subtrajectory access | Treat trajectories as first-class versioned resources, not as opaque event payloads |
| [OGC Moving Features Access](https://docs.ogc.org/is/16-120r3/16-120r3.html) | Queries include feature attributes, trajectory-to-geometry and trajectory-to-trajectory relations | Query design needs temporal attribute joins, spatial predicates and pairwise relations |
| [ONVIF Analytics](https://www.onvif.org/specs/2106/ONVIF-Analytics-Service-Spec-v2106.pdf) | Metadata frames carry UTC relation, source, coordinate transforms, object IDs, boxes and object split/merge/delete relations; metadata need not exist for every video frame | Persist a stable frame locator, coordinate revision and track lineage; lower-rate metadata must declare gaps and sampling |
| [GeoParquet project](https://github.com/opengeospatial/geoparquet/blob/main/format-specs/geoparquet.md) | Bounding boxes and row-group statistics can prune spatial data; exact processing remains reader work | GeoParquet is useful for center/cold exchange, but does not replace temporal/entity indexes; current 2.0 work is still pending final OGC approval |
| [Kafka producer](https://kafka.apache.org/33/configuration/producer-configs/) | Producer idempotence constrains retries, ordering and in-flight requests | Stable record identity and a durable local outbox remain necessary across application restart and data-lake commit |
| [Timescale hypertables](https://docs.timescale.com/use-timescale/latest/hypertables/) | Server time-series systems commonly separate recent row storage from compressed columnar chunks | The lifecycle pattern is relevant, but PostgreSQL/Timescale is not an embedded QCS6490 recommendation |
| [ClickHouse inserts](https://clickhouse.com/blog/asynchronous-data-inserts-in-clickhouse) | Column stores require batching; many small parts cause merge and write-amplification pressure | Do not deploy a server column store on the camera or flush one immutable file per small batch |

## Target facts observed

The user-authorized `lacai-home` target was inspected read-only on 2026-09-19:

- QCS6490 userspace is `aarch64`;
- the root volume had about 80 GiB available and the mounted SD card about 58.6 GiB total;
- total RAM was about 5.24 GiB, with about 3.72 GiB available during inspection;
- SQLite runtime library exists, but the CLI is absent and target R-tree compile support has
  not yet been proven;
- the approved eSDK contains SQLite headers/libraries but no reviewed DuckDB, Arrow or Parquet
  target package.

These are point-in-time facts, not allocation budgets. Storage available to AI APP must be a
validated deployment quota and must reserve WAL, compaction, export spool and system headroom.

## Design conclusions

1. One row per detection in one ever-growing SQLite file is not the target architecture.
2. SQLite remains useful for catalog, episodes, manifests, outbox and hot indexes, but history
   must be time-sharded and high-rate points must be chunked/batched.
3. A track-centric binary chunk is a domain encoding inside a transactional store, not a new
   database engine. It can delta-encode frame/time/coordinates and avoid repeating strings.
4. Parquet is appropriate for immutable cold history and the center lake after its writer and
   reader pass the eSDK/package/board gates. DuckDB is a candidate query adapter, not the owner
   of high-frequency per-frame transactions.
5. Full arbitrary retrospective geometry and aggressive lossy sampling are incompatible. The
   product must choose exact-observation and error-bounded horizons explicitly.
6. Cross-camera footprint cannot be represented by a global track ID alone. Local tracklets,
   topology, time uncertainty and revisioned association hypotheses must remain distinct.
7. Long-range statistics should read versioned rollups and drill down to episodes/chunks; a
   yearly dashboard must not scan every detection point.

## Limits and next work

- No DuckDB/Arrow/Parquet code was downloaded, built or run.
- No R-tree target capability, compression ratio, flash endurance or concurrent workload was
  measured.
- GeoParquet is considered an interoperability direction, not a committed edge dependency.
- Sampling error and retention profiles still require representative security and traffic
  datasets plus product SLOs.

## See also

- [Spatiotemporal metadata architecture](../architecture/spatiotemporal_metadata.md)
- [Metadata query prototype](../architecture/metadata_query.md)
- [Metadata improvement plan](../planning/architecture_improvement/metadata_query_plan.md)
