# Kế hoạch metadata và truy vấn security/traffic

Plan này triển khai data catalog D01–D18 và query catalog Q01–Q30 trong master plan cho
18 usecase security và phần traffic mở rộng. Mục tiêu là một canonical schema có thể query
đúng, offline-safe và có provenance; engine SQLite/Parquet/DuckDB chỉ được chọn sau benchmark.

- **Status:** planned — schema/query chưa được triển khai.
- **Layer:** docs
- **Source:** master plan [mục 6](README.md),
  [observation contract](../../architecture/observation_contract.md), `n/a` cho source mới.

## Trách nhiệm

- AI APP là owner metadata semantics, local data service, query API, retention projection,
  authorization và Kafka export của edge.
- BSP+FW cung cấp source/profile/time/calibration/signal/media facts, volume/quota/security
  context và query client/UI; không ghi trực tiếp database AI.
- AI Model cung cấp ontology, output quality, sampling constraints và golden records;
  không tự chọn schema query hay retention.

## 1. Phạm vi triển khai theo lớp

| Lớp | Dữ liệu | Mục đích | Quyền lưu |
|---|---|---|---|
| Live | latest track/annotation, temporal state | overlay/runtime | RAM bounded, mất khi restart theo contract |
| Detail | sampled observation/trajectory/relations | timeline, geometry, replay | retention ngắn, budget theo source/usecase |
| Search facts | passage, attribute interval, event, recognition, plate | Q01–Q13/Q18–Q24 | bền vững theo search retention |
| Aggregates | count/dwell/density/flow/speed buckets | Q14/Q15/Q21/Q22 | revision + coverage + exact/approximate |
| Evidence refs | request/media/actual interval/gaps | drill-down FW media | media bytes không vào metadata DB |
| Audit/outbox | receipt, manifest, correction/purge/access | durability/replay/privacy | bền vững theo compliance policy |

Không lưu toàn bộ tensor/frame/crop vì model cần chúng lúc inference. Chỉ giữ supporting
data đủ cho query/explain/recompute theo policy. Dữ liệu thiếu phải có reason/coverage;
không trả empty list làm giả “không xảy ra”.

## 2. Canonical schema tối thiểu

### 2.1. Envelope bắt buộc

Mọi record có `record_id`, `revision`, `supersedes` hoặc tombstone; device/source,
boot/session/epoch; producer/model/ontology/config/rule revisions; event time và recorded
time; clock domain/mapping/uncertainty; quality/provenance; sensitivity/classification.
Timestamp integer kèm unit; enum/value có schema ID/version. Không dùng chuỗi rỗng cho
unknown/not_observable/not_supported/expired.

`track_key = device_id + source_id + boot_id + source_epoch + local_track_id`.
Face identity, plate text, cross-camera candidate và local track có khóa riêng. Update
là append revision; query snapshot chọn revision hợp lệ thay vì sửa mất lịch sử.

### 2.2. Record families

| Family | Required fields | Producer |
|---|---|---|
| `scene_revision` | profile/FOV/pose, coordinate spaces, zone/line/lane/grid/topology, calibration validity/error | BSP+FW facts + AI APP scene registry |
| `observation_sample` | entity/class candidates, bbox/keypoint/mask refs, score/quality, frame/ticket, observed/predicted | AI APP decoder |
| `track_segment` | track key, start/end, close reason, gap/ID switch, spatial bounds, representative refs | AI APP tracker |
| `trajectory_chunk` | ordered points, sample mode, uncertainty, coordinate revision, gap markers | AI APP trajectory writer |
| `attribute_assertion` | subject, typed schema/value/candidates, confidence, valid interval, expiry, model provenance | AI APP; ontology AI Model |
| `relation_interval` | subject/object, carries/near/plate/group/member, valid interval, confidence/method | AI APP fusion |
| `passage_presence` | line/polygon/lane ref, enter/cross/exit, direction, begin/end, open/uncertain | AI APP geometry/rules |
| `measurement_sample` | quantity/unit/method, window, calibration ref, uncertainty/validity | AI APP from platform/model |
| `external_state_interval` | signal/access/roster/schedule value/revision, effective/receive/expiry time | BSP+FW → AI APP |
| `event_episode` | type/rule, lifecycle, participants/region, onset/end, severity, claims, evidence refs | AI APP feature |
| `recognition_attendance` | identity candidate, gallery/roster/session revision, score/threshold, decision/correction | AI APP |
| `plate_read_consensus` | raw/normalized text, alternatives, alphabet/normalization revision, quality, vehicle/passage ref | AI APP ANPR |
| `context_assessment` | VLM model/prompt/rule revision, bounded structured claims, support refs, review state | AI APP + AI Model |
| `aggregate_bucket` | dimensions/revisions, interval, count/sum/histogram/sketch, denominator/coverage | AI APP |
| `evidence_reference` | FW request/media, state, actual interval/gaps, availability/digest | BSP+FW media + AI APP link |
| `association_hypothesis` | track/entity links, score/method, validity, accepted/rejected/review revision | AI APP |
| `coverage_health` | enabled/running, source/model/storage/export gaps, dropped/sample rates, clock health | AI APP + BSP+FW |
| `delivery_archive_audit` | sink, attempt/receipt, file manifest/checksum/coverage, correction/purge/access audit | AI APP |

Mỗi usecase đăng ký `produces`, `requires`, `retains`, `query_capabilities`, schema version,
max rate/cardinality và sensitivity. Một canonical fact dùng chung cho nhiều feature; không
nhân bản 18 lần hoặc kế thừa quyền của feature đầu tiên.

## 3. Query API và usecase mapping

### 3.1. Query families phải có

| Nhóm API | Query IDs | Contract result |
|---|---|---|
| Search object/attribute/passage | Q01–Q06, Q11–Q13, Q18–Q20 | typed filters, interval semantics, candidate/confirmed distinction |
| Timeline/trajectory/geometry | Q03–Q05, Q07, Q24–Q25 | points/segments/gaps, coordinate revision, observed coverage |
| Identity/attendance/similarity | Q09–Q10, Q16 | scope-sensitive candidate, gallery/roster/index revision |
| Event/explain/evidence | Q08, Q17, Q23, Q26 | lifecycle, provenance, media status, no hallucinated fact |
| Aggregate/traffic analytics | Q14–Q15, Q18–Q24 | count basis, denominator, calibration/method, exact/approximate |
| Health/admin/export/recompute | Q27–Q30 | snapshot/job/retention/purge/receipt and bounded cancellation |

### 3.2. Request/response contract

Request có query kind/version, source set, `[begin,end)` và time basis, typed filters,
historical/current revision, quality threshold, field projection, sort, page cursor,
deadline và principal scope. Không cho client gửi SQL tùy ý.

Response có query/snapshot ID, stable cursor, authorized fields, coverage/watermark,
gaps/retention boundary, quality/unknown, `complete|partial|approximate|unsupported|
budget_exceeded`, provenance revision, scope `local|federated` và evidence availability.
`complete` chỉ nghĩa dữ liệu hợp lệ đã đọc đủ snapshot, không nghĩa detector thấy mọi vật.
Query lớn chuyển async job với scan/RSS/temp-disk budget; cursor lease hữu hạn.

Các semantics bắt buộc trước khi mở query:

- “áo đỏ tại cổng” là attribute valid tại crossing hay từng xuất hiện trong track;
- passage là line crossing hay polygon presence, anchor là footpoint hay center;
- unique là passage, local track hay person/vehicle entity;
- absence/zero là có coverage đủ hay camera/feature đang gap;
- speed/violation chỉ là candidate nếu calibration/signal/time chưa đạt gate.

## 4. Storage baseline và quyết định

### 4.1. Baseline M1

Tạo data service một-writer với SQLite typed tables cho search facts, aggregate buckets,
archive manifest và delivery outbox; query facade dùng prepared statements, bind parameters,
keyset pagination và field-level authorization. WAL/durability policy là config đã validate.
M1 chỉ mở Q01–Q08, Q14 và Q27 trên person/vehicle/scene fixtures, không quảng cáo Q09/Q16
hoặc speed nếu producer chưa có.

Index thử nghiệm: `(source_id, scene_revision, zone/lane, type/value, event_time, record_id)`
cho passage; track/source/time và bounds cho trajectory; event/source/time/state; plate
normalized/time; relation subject/object/time. Không tạo index mọi cột trước `EXPLAIN`.
R-tree chỉ lọc bounding candidates; exact geometry phải kiểm tra trajectory và revision.

### 4.2. Spike M2 hybrid

Khi SQLite detail vượt disk/RSS/write budget hoặc query analytics cần scan lớn, archive
immutable chunks sang Parquet theo time + bounded source groups, row groups có sort/stats.
Manifest có file ID/checksum/schema/sequence/time coverage/generation. DuckDB đọc/ghi cold
path qua adapter C API; projection/filter pushdown là tối ưu scan, không phải trajectory index.
Không tạo file mỗi track/plate, không glob mọi file coi là committed. Hot/cold overlap dedup
theo record ID/revision; compaction publish generation mới rồi retire sau reader drain.

Arrow C++ chỉ là ứng viên nếu toolchain phù hợp; LACAI C++17 không được nâng standard toàn
dự án vì dependency. Engine chỉ được chốt sau eSDK cross-build, package/license/SBOM,
RSS/CPU/flash/latency benchmark.

### 4.3. Kafka không nằm trong query writer

Transaction ghi canonical fact + outbox record; exporter độc lập batch/retry. Broker ACK,
archive publish và local durable commit là receipt khác nhau. Offline spool bounded theo
retention/quota; không để exporter/query khóa inference hoặc evidence lane.

## 5. Task thực hiện

| Task | Owner | Đầu ra | Tiêu chí kết thúc |
|---|---|---|---|
| M01 | AI APP | `data_catalog_v1` cho D01–D18, units/null/quality/privacy | Schema review không còn field mơ hồ |
| M02 | AI APP + AI Model | Fixtures person/vehicle/scene/plate/attribute/unknown/gap | Golden record có checksum/provenance |
| M03 | AI APP | Query contract Q01–Q30, capability/unsupported matrix | API có semantics, projection, auth, SLO |
| M04 | AI APP | SQLite DDL, migration, writer, outbox, prepared query facade | Crash/retry/revision tests pass |
| M05 | BSP+FW | Volume/quota/clock/media/query-client fixture | Disk full/profile/reset behavior verified |
| M06 | AI APP | Benchmark SQLite-only vs hybrid using same dataset | Decision record có p50/p95/p99, RSS, bytes scanned |
| M07 | AI APP | Parquet manifest/DuckDB adapter spike nếu M06 cần | eSDK/package/license gate pass |
| M08 | AI APP + BSP+FW | Retention/purge/access/correction/recompute report | Hot/cold and derived copies consistent |

## 6. Tiêu chí nghiệm thu

- [ ] 18 usecase rows map được tới `produces/requires/retains/query_capabilities`; cháy/khói,
  VLM, attendance, plate, lane và scene-only event không bị ép vào person track.
- [ ] Q01–Q30 có positive/negative/unknown/unsupported/expired fixtures và quyền filter/
  projection; Q02 trả đúng attribute tại event time, không dùng latest value.
- [ ] `track_key`, event time/recorded time, clock/geometry/model/rule revision và gaps được
  kiểm tra; reboot/ID switch không tạo identity liên tục giả.
- [ ] Passage/count/dwell/heatmap/speed aggregate có dedup, denominator, coverage và
  exact/approximate state; không cộng frame count như người duy nhất.
- [ ] SQLite baseline giữ writer bounded, prepared query, keyset paging, WAL/recovery;
  query không block frame/event writer quá budget.
- [ ] Nếu chọn Parquet, mọi file có manifest/checksum/coverage, crash-before-publish recovery,
  hot/cold dedup và benchmark chứng minh lợi ích; nếu không đạt thì giữ SQLite baseline.
- [ ] Purge/revoke xóa hoặc tombstone canonical, projections, archive/index, outbox/cache theo
  policy; sensitive field không lộ qua aggregate/filter.

## Handoff

M1 schema + M2 fixtures bàn giao cho event/DSP/integration plans. M04 SQLite/outbox là input
cho event plan; C02 time/calibration từ BSP+FW là blocker cho traffic Q21/Q23. Chỉ quảng bá
query capability sau khi producer và acceptance report cùng version.

## Giới hạn và công việc tiếp theo

- Chưa chốt retention/SLO/cardinality; AI APP lead cần ký profile trước M04.
- Chưa benchmark engine trên target; Parquet/DuckDB vẫn là spike có điều kiện.

## See also

- [Architecture improvement master plan](README.md)
- [Feature catalog](../../architecture/feature_catalog.md)
- [Model observation contract](../../architecture/observation_contract.md)
- [Artifact and model integration contract](../../contracts/model_integration.md)
