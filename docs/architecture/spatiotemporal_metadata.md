# Spatiotemporal metadata architecture

This document defines the target AI APP metadata subsystem for live and historical object
footprints, cross-camera paths, event analytics and long-retention security/traffic queries.

**Status:** board-smoke — version 1 contracts, packed SQLite shards, correction-aware rollups
and the bounded service passed the 2026-09-19 eSDK and QCS6490 workload; production composition
and retention remain open. **Layer:** app. **Source:**
`include/vqec/vision/ai/contracts/vqec_vision_spatiotemporal_metadata.hpp`,
`src/adapters/storage/`, `src/app/service/vqec_vision_metadata_service.cpp`.

## Responsibility

- Preserve enough frame-correlated, spatial and temporal information for declared future
  queries without blocking inference or promising unlimited exact history.
- Provide one AI-owned service/API while using different physical stores for different access
  patterns.
- Keep local track evidence immutable and represent cross-camera identity as revisioned
  hypotheses.
- Support live subscriptions, bounded historical queries, aggregates and Kafka export from the
  same canonical identities.
- Treat identity, embedding and plate as ordinary metadata domains by current product policy.
  Domain scopes still control feature/output entitlement; they do not imply encrypted storage.

## Architectural constraints

Four requirements cannot all be maximized simultaneously: every processed detection, arbitrary
future exact geometry, month-scale retention and bounded camera storage. The service therefore
exposes data resolution and error explicitly:

| Resolution | Meaning | Valid use |
|---|---|---|
| `observation_exact` | Every retained processed observation has its original frame locator and geometry | Exact replay/retroactive rules inside the configured exact horizon |
| `trajectory_bounded` | Track path is simplified or sampled with declared time/spatial error and mandatory boundary points | Footprint rendering, approximate historical geometry and path summaries |
| `episode_fact` | Accepted interval/event/passage/relationship without the full path | Search, audit, evidence and drill-down entry point |
| `aggregate` | Versioned bucket, grid or sketch with denominator and coverage | Month/year dashboards and trend comparisons |

If exact observations and media have expired, the API must not claim that an arbitrary new line
crossing can be reconstructed exactly. It returns the retained resolution, maximum error, gaps
and `partial` or `approximate` completeness.

## Logical data model

### Frame locator

Every persisted observation or mandatory trajectory point binds:

- device, camera/channel, boot/session and source epoch;
- source frame ID and source PTS;
- capture UTC when available, clock-mapping revision and uncertainty;
- inference model/decoder/tracker revisions and recorded time;
- source geometry and coordinate-transform revision;
- optional FW media segment/reference plus media PTS/availability.

The frame locator maps metadata to the originating video frame. It does not claim that video is
still retained. ONVIF-compatible export can derive a frame UTC/coordinate representation, but
ONVIF XML/JSON is not the internal storage schema.

### Local tracklet and trajectory chunk

`track_key` remains local to device, source, boot/session, epoch and tracker ID. A tracklet has
ordered chunks. Each chunk header contains:

- track key, chunk sequence, class candidates and lifecycle state;
- begin/end frame locator and point count;
- coordinate space, anchor meaning, scene/calibration revision;
- model/tracker/config revisions;
- chunk spatial bounds and time bounds;
- sample policy, exact/approximate mode, maximum spatial/time error;
- gap, predicted/observed, split/merge and close-reason flags;
- codec identity, uncompressed size and checksum.

Point data uses fixed-width or varint delta fields for frame/time and quantized coordinates.
The minimal footprint point is anchor X/Y plus frame/time delta and flags. Optional box, score,
class/attribute change and observation reference are separate streams so common identifiers and
unchanged values are not repeated for every frame.

### Entity and cross-camera association

A global person/vehicle/object path is a read model over immutable local tracklets:

```text
local tracklet A -- association hypothesis h1 --> entity revision R
local tracklet B -- association hypothesis h2 --> entity revision R
local tracklet C -- rejected hypothesis h3
```

Each link carries method/model revision, descriptor reference, score, topology path, travel-time
window, clock uncertainty, review state and valid revision. Accepting or rejecting a link creates
a new association revision; it never rewrites original track IDs. A footprint response returns
confidence and gaps per camera transition. Edge-only association is limited to sources on that
device; cross-device identity belongs to the center/federated tier.

### Scene, episode and aggregate

- Scene revisions define source geometry, normalized image coordinates, 2D ground plane when
  calibrated, zones/lines/lanes, topology and validity interval.
- Event episodes merge repeated frame detections into one business occurrence with onset/end,
  participants, region, severity, review and evidence state.
- Aggregate contributions have stable IDs so correction/retraction is possible without double
  counting. Rollups preserve definition, scene and coverage revisions.
- Spatial heatmaps store a versioned grid or tile key, duration/count basis and denominator.
  Pixel heatmaps from different scene revisions are never combined silently.

## Physical data plane

One service owns the API, but all data must not live in one database file.

```text
perception / tracker / features
            |
            v
bounded ingest queue ---> live state + subscription fan-out
            |
            v
single metadata writer
   |        |                 |
   |        |                 +--> durable Kafka outbox
   |        +--> active detail shard(s): observations + trajectory chunks
   +--> catalog.db: scenes, tracks, episodes, associations, manifests, rollups
                         |
                         v
              seal / verify / publish generation
                         |
          +--------------+----------------+
          |                               |
 sealed read-only detail shards     optional Parquet cold tier
          |                               |
          +---------- query planner ------+
                         |
              page / job / live stream API
```

### Live state

A bounded in-memory table owns latest tracks, active episodes, current association candidates and
short temporal windows. UI footprint uses snapshot-plus-delta: fetch a stable initial snapshot,
then subscribe to ordered deltas with sequence/gap recovery. D-Bus remains control/status; bulk
trajectory streaming uses a bounded data API such as UDS or the backend's WebSocket bridge.

### Catalog and hot facts

`catalog.db` uses SQLite WAL and typed tables for scene revisions, track/episode headers,
attribute/relation intervals, passages, association revisions, aggregate definitions, shard
manifests, coverage, jobs and outbox. Transactions remain short. UI/backend never opens this file.

The current generic `metadata_records` table is a prototype receipt store, not the target schema.
Hot access paths need typed projections and contribution tables; generic payload is retained only
for bounded extension data.

### Detail shards

High-rate observations and trajectory chunks are partitioned by bounded time window and source
group. The active shard is writable; sealed shards are immutable and opened read-only. Partition
duration, maximum bytes and source grouping come from a measured storage profile.

Version 1 uses SQLite shards containing packed trajectory BLOBs and typed chunk indexes. This
reuses transactions/recovery without paying one SQL row per point. Current indexes cover
source/time, subject/time, frame range and spatial bounds; exact polyline verification follows
candidate filtering. SQLite R-tree remains optional and cannot replace exact verification.

### Cold columnar history

Parquet remains the preferred center interchange candidate because immutable row groups, column
statistics and projection/filter pushdown match long-range scan/aggregate workloads. It is not a
version 1 edge runtime dependency: the approved eSDK has no reviewed DuckDB/Arrow/Parquet package,
and the measured packed-shard path meets the current five-minute gate without one.

The manifest, not filesystem globbing, publishes a cold generation. A file manifest records
schema, checksum, source/time/sequence bounds, row-group stats and predecessor generation.
Compaction publishes a new generation then retires old files after reader/cursor drain.

## Capture and sampling strategy

The tracker produces at its own cadence; storage policy is independent of preview FPS.

Mandatory points are retained at:

- track start/end and every gap/reconnect;
- scene/calibration/coordinate revision change;
- split, merge, ID switch or cross-camera association boundary;
- zone enter/exit, line crossing, direction change and stop/start transition;
- attribute/class/relationship state change;
- event onset, peak, evidence frame and end;
- maximum configured time gap even when motion is linear.

Between mandatory points, a policy selects one of:

1. exact processed observations for the exact horizon;
2. fixed maximum time/distance sampling;
3. online error-bounded polyline simplification for the footprint horizon;
4. representative/keyframe-only detail for event-centric usecases.

The chosen policy and achieved error are stored per chunk. Simplification works on the anchor
path, while box/attribute streams use their own change/error policy. Events and traffic facts are
computed before lossy downsampling when their correctness needs higher cadence.

For every retained point the exact originating frame locator is stored. An interpolated point has
no fabricated frame ID; the response identifies its two supporting samples. Exact mapping for
every processed frame requires exact observation retention or later inference from retained video.

## Query model

Q01–Q30 remain stable product capability groups, but they are not 30 hardcoded SQL statements.
The query service composes a bounded typed plan from these primitives:

| Primitive | Examples |
|---|---|
| Collection | observations, tracklets, entities, episodes, passages, aggregates, health |
| Entity filter | class, local track, entity revision, identity, plate, embedding candidate |
| Temporal | instant, overlap, during, before/after, ordered sequence, dwell, recurrence |
| Spatial | source, scene revision, zone/line/lane, bbox, polygon, path intersection, nearest |
| Attribute/relation | typed value valid at time, carries, near, member-of, plate-of |
| Similarity | top-k with model/vector-space revision and structured filters |
| Aggregation | count basis, unique scope, duration, histogram, percentile, grid, trend |
| Revision | as-observed, as-known-at, latest-corrected, scene/association/model revision |
| Resolution | exact observations, bounded trajectory, episode, aggregate |
| Output | projection, sort, page/job, live subscription, coverage and evidence refs |

The planner resolves hot facts, detail shards, cold files and rollups behind one snapshot token.
Small selective queries return pages. Multi-day geometry, similarity and recomputation become
bounded asynchronous jobs with cancellation, scan-byte, CPU, RSS, output and deadline budgets.

## Usecase query coverage

This matrix defines query families that the data model must express; it is not a claim that the
corresponding model producer is available.

| ID | Required query templates beyond basic event search |
|---|---|
| S01 smoking | episode count/rate by zone/time; smoker dwell/path; repeat locations; PPE/context joins; evidence and review trend |
| S02 suspicious weapon | count by type/severity/zone; object/person relation; pre/post path; recurring location/time; review outcome |
| S03 PPE | compliance denominator/rate by item, zone and shift; missing-item duration; repeat track/entity; trend and coverage |
| S04 fire/smoke | distinct episode count by day/month/year; duration/onset/severity; frame/ground-plane hotspot; recurring source/zone/time; evidence/review/model revision |
| S05 blacklist | encounter timeline, first/last seen, per-camera and cross-camera path, watchlist revision, reviewed disposition and recurrence |
| S06 attendance | present/absent/unknown, late/early, first/last and duration by roster/shift; corrections, duplicate encounter policy and coverage |
| S07 age/gender | distribution and trend by zone/time/flow; unknown/quality denominator; joint attributes; drill-down to eligible tracks |
| S08 heatmap | live and historical density, dwell-weighted grid, peak/percentile, zone flow, path overlay, scene revision and coverage |
| S09 intrusion | episode count/duration; entry boundary and route; dwell/last seen; associated objects; repeated zone/time and evidence |
| S10 entry/exit count | in/out/occupancy by line/direction/class/bucket; unique basis; recross; correction, gap and comparison trend |
| S11 person tracking | current/last seen; exact or bounded path; frame drill-down; ordered zones; cross-camera candidates; time/distance and gaps |
| S12 VLM alert | structured-claim search; counts by rule/prompt/model; evidence frames; review result; claim co-occurrence and revision comparison |
| S13 abandoned/removed | object lifecycle, stationary duration, owner/nearby relations, last-seen path, removal route, resolution and false-alarm review |
| S14 lost item | attribute/similarity candidate search; last seen; item/possible-owner path; cross-camera hypotheses; ranked evidence and outcome |
| S15 luggage/cart | route/dwell/zone transitions; person relation and separation episodes; count/utilization; lost/abandoned joins and evidence |
| S16 plate | exact/normalized/fuzzy history; vehicle relation; passage/parking session; route/OD/travel time; frequency, class/color and alternatives |
| S17 crowd | episode count/duration/peak size; formation/dissolution path; hotspot and recurring time; flow/density trend; participants and coverage |
| S18 fight/conflict | count/duration/severity; participants/relations; pre/post trajectories; hotspot/time trend; evidence, review and recurrence |

Traffic adds lane movement, speed/acceleration, stop/queue, signal-phase violation, OD/headway
and near-miss queries using the same primitives with calibration, topology and clock validity.

## Fire and smoke example

Frame detections are not counted as fires. The pipeline writes:

1. sampled observations or masks tied to frame locators;
2. one spatially bounded episode after temporal deduplication;
3. episode contributions to hour/day/month rollups;
4. duration/severity and heatmap-grid contributions pinned to scene revision;
5. evidence references and review corrections.

A yearly count reads monthly/day rollups and can drill down to episode IDs. “Where does fire
usually occur in the image?” reads a duration- or episode-weighted grid for one scene revision.
Changing camera pose starts a new scene revision; merging the two needs an explicit valid
transform or a separate result.

## Sizing and retention

Profiles are computed, not hardcoded:

```text
point_bytes_per_day = sources * mean_active_tracks * stored_points_per_second
                      * encoded_bytes_per_point * 86400
detail_peak = exact_horizon + footprint_horizon + active_shards + compaction_space
total_peak = catalog + detail_peak + aggregates + outbox + cold + reserve
```

Illustrative arithmetic shows why this matters: 16 sources, ten active tracks/source, five
stored points/s and 64 bytes/point is about 133 GB for 30 days before indexes and reserve. At
30 points/s it is about 796 GB. Packed delta chunks may reduce bytes/point substantially, but
compression ratio is an acceptance measurement, not an architecture promise. The observed board
free space cannot hold an arbitrary full-rate month without an SD/retention/cold-tier policy.

Retention is therefore independently configured for exact observations, bounded trajectories,
episodes/search facts, aggregates, media references and export spool. Admission rejects a profile
whose worst-case active data plus compaction and reserve exceeds the assigned quota.

## Concurrency and failure model

- One service is the only writer. Producers submit bounded batches; overflow policy is per data
  class and always increments coverage/drop counters.
- Writer batches by record/byte/age while keeping event/outbox durability boundaries explicit.
- Live subscriptions read RAM snapshots; they do not hold SQLite read transactions.
- Historical readers use short catalog snapshots plus immutable shard/file generations. Cursors
  have leases so abandoned queries cannot block retirement indefinitely.
- A full disk stops durable acceptance before corrupting existing history. Live processing may
  continue only under an explicit degraded policy.
- Crash recovery replays committed manifests/outbox, discards or imports verified orphan files,
  and marks the unflushed active-chunk interval as a coverage gap.
- Kafka export reads committed sequence ranges; broker acknowledgement and center-lake receipt
  remain separate.

## Technology decision gates

| Candidate | Role | Gate |
|---|---|---|
| SQLite WAL | Selected v1 catalog, hot facts, manifests, outbox and detail shards | Production retention, power-loss and disk-full qualification remain |
| Packed trajectory chunk | High-rate edge point encoding inside a transactional shard | Golden codec, corruption/overflow tests, compression and exact/error-bound replay |
| Parquet | Center interchange; optional future immutable cold history | Add only after a pinned eSDK package/license/SBOM and measured edge benefit |
| DuckDB C API | Optional future cold query worker | Add only with bounded RSS/threads/temp disk/cancellation evidence |
| Arrow C++ | Alternative Parquet batch adapter | Only if its package/RSS benefit beats DuckDB or a smaller writer |
| RocksDB/LMDB | Not selected | Reconsider only if measured key-write pressure exceeds SQLite and secondary-query cost is funded |
| ClickHouse/Timescale | Center/server reference only | Never deploy on the camera without a separate product/operations decision |

## Measured implementation evidence

The 2026-09-19 `FULL`-sync QCS6490 run used 27 deterministic security/traffic scenario profiles,
four sources, 50 record sets/s and an offline outbox while the full AI preview workload ran. In
300 seconds it committed 45,081 records with no rejection, write/query failure or oracle failure.
There were 9,952 concurrent aggregate queries: p50 2.688 ms, p95 12.454 ms and p99 18.546 ms.
The metadata process used 30.110% of one core and 37,120 KiB maximum RSS; its store reached
32,567,296 bytes. The AI process averaged 12.88% of one core. RTSP remained H.264 1920x1080 at
30.000 packet-PTS FPS, and the reviewed contact sheet showed current person overlays.

The materialized corrected rollup replaced a first implementation that scanned all contribution
revisions. That earlier run used 46.56% metadata CPU and its query cost grew with history. Keeping
the failed comparison prevents the optimized result from hiding the rejected design.

## Limits and next work

- The service library is not composed into `vqec_ai_vision_applications`; usecase producers do
  not yet feed production observation/event batches into it.
- Retention/purge cannot retire data until the Plan 3 outbox exposes durable sink receipts.
- Board power-cut, disk-full, restart and long-query cancellation evidence is still missing.
- Q01–Q30 remain capability groups; the new typed query implements tracklet, association,
  episode and aggregate slices and rejects unsupported collections explicitly.
- The current eSDK lacks reviewed DuckDB/Arrow/Parquet packages; no edge cold-tier claim is made.
- Cross-device footprint and year-scale analytics normally belong to the center tier even when
  one device keeps a bounded local history.

## See also

- [Metadata storage source review](../research/metadata_storage_source_review.md)
- [Metadata prototype](metadata_query.md)
- [Metadata redesign plan](../planning/architecture_improvement/metadata_query_plan.md)
- [Observation contract](observation_contract.md)
- [Tiered storage proposal](../adr/0009_spatiotemporal_metadata_tiering.md)
