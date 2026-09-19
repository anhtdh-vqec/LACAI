# Kế hoạch metadata không-thời gian cho security và traffic

Plan 2 xây metadata service đủ cho footprint realtime/lịch sử, truy vết liên camera, tìm kiếm
sự kiện và thống kê dài hạn của 18 usecase security cùng traffic mở rộng.

**Status:** accepted — P2 đóng ngày 2026-09-19 sau 142/142 eSDK/QEMU tests, fault/recovery
gates và exact composed QCS6490 candidate chạy 5 phút ở 30,124 FPS, 13,16% CPU.
**Layer:** docs. **Source:** `docs/architecture/spatiotemporal_metadata.md`,
`docs/adr/0009_spatiotemporal_metadata_tiering.md`.

## 1. Kết luận thiết kế

AI APP sở hữu một metadata service và một typed API. “Một service” không có nghĩa tất cả dữ liệu
nằm trong một file DB:

```text
perception/features
       |
bounded ingest queue
       +--> live RAM snapshot + delta
       +--> catalog.db: scene/track/episode/association/rollup/manifest/outbox
       +--> active detail shards: observation + packed trajectory chunks
                    |
             seal/read-only
                    +--> optional Parquet cold tier
```

- SQLite WAL hiện tại được giữ làm transaction/outbox prototype và ứng viên catalog/hot shard.
- Không lưu một SQL row cho mọi detection trong một DB tăng vô hạn.
- Mỗi điểm được giữ phải có frame locator thật. Điểm nội suy chỉ tham chiếu hai điểm gốc, không
  được bịa frame ID.
- Local tracklet là fact bất biến; đường đi xuyên camera là association hypothesis có revision,
  score, topology, clock uncertainty và review state.
- Identity, embedding và biển số là metadata thông thường, lưu trong thư mục metadata được cấu
  hình. Access domain vẫn bắt buộc để kiểm entitlement/output, không phải nhãn dữ liệu nhạy cảm.
- API công bố resolution, error bound, gap và coverage; storage hữu hạn không thể hứa mọi truy
  vấn hình học hồi tố chính xác sau khi exact observations đã hết retention.

Chi tiết normative nằm trong [kiến trúc metadata không-thời gian](../../architecture/spatiotemporal_metadata.md).

## 2. Bốn mức dữ liệu bắt buộc

| Mức | Dữ liệu | Mục đích | Quy tắc trả kết quả |
|---|---|---|---|
| Exact observation | Geometry/attribute đã xử lý + frame locator | Replay, rule hồi tố, drill-down trong exact horizon | `complete` chỉ khi coverage đủ và không gap |
| Bounded trajectory | Điểm mandatory + sample/simplify có error bound | Footprint dài ngày, route, line/zone candidate | `approximate` nếu không còn exact points |
| Episode fact | Event/passage/relation đã dedup theo interval | Search, evidence, workflow, audit | Không đếm từng frame detection thành event |
| Aggregate | Bucket/grid/sketch có definition và coverage revision | Tháng/năm, heatmap, trend | Drill-down về episode/contribution còn retention |

Retention được cấu hình độc lập cho exact observation, footprint, episode, aggregate, media
reference và export spool. Admission phải tính cả WAL, active shard, compaction space và reserve.

## 3. Query coverage phải nghiệm thu

Q01–Q30 là capability group ổn định, không phải 30 câu SQL cố định. Query v1 đích phải compose
được collection, entity filter, temporal relation, spatial relation, attribute/relation,
similarity, aggregation, revision, resolution và output projection/job/subscription.

Mỗi S01–S18 phải có fixture cho các họ truy vấn trong bảng kiến trúc, tối thiểu:

- S01–S03: episode/zone/dwell/relation/compliance denominator và trend;
- S04: số vụ cháy theo ngày/tháng/năm, duration/severity, hotspot theo scene revision và drill-down;
- S05–S07: encounter/attendance/demographic distribution, correction, unknown/coverage;
- S08–S11: live/historical footprint, density/dwell grid, ordered passages, gap và liên camera;
- S12–S15: structured claim, object lifecycle, last seen, owner candidate, luggage/cart relation;
- S16: exact/normalized/fuzzy plate, passage/parking/route/OD/travel time;
- S17–S18: crowd/conflict episode, peak/participants/hotspot/pre-post trajectory/review;
- traffic: lane movement, speed, queue, signal-phase violation, OD/headway và near miss với
  calibration/topology/clock validity.

Tất cả phải có positive, negative, unknown, unsupported, expired; bổ sung partial, approximate,
budget-exceeded, corrected và association-ambiguous cho contract mới.

## 4. Kế hoạch thực hiện

Trạng thái được chốt theo bằng chứng hiện có, không suy rộng từ việc build pass:

| Mốc | Trạng thái | Bằng chứng / khoảng trống |
|---|---|---|
| M01 | accepted | Contract v1, codec và validation lấy version duy nhất từ registry |
| M02 | accepted baseline | Catalog bao phủ S01–S18 + 9 traffic profile; benchmark 27 scenario có oracle; query chưa có producer trả `unsupported` |
| M03 | accepted | Catalog, packed shard, manifest/seal/recovery/quota và atomic outbox pass fault gates |
| M04 | accepted baseline | Bounded query, exact spatial, snapshot/delta, deadline và cancellation đã có; history job lớn là extension sau P2 |
| M05 | accepted baseline | Episode revision, correction/retract và rollup giao dịch đã có; concrete usecase producer thuộc P5 |
| M06 | accepted | Chọn SQLite packed shards cho edge v1; Parquet là center/future cold tier |
| M07 | accepted | Concurrent benchmark, restart, SIGKILL, disk-full, corruption và cancellation pass trên board |
| M08 | accepted | Validated composition, authorized producers, receipt-safe retention, health và clean drain đã chạy production candidate |

### M01 — Chốt logical contracts v1

Deliverables: frame locator; clock/scene/coordinate revision; local tracklet; trajectory chunk;
entity/association revision; episode; aggregate contribution; coverage; shard manifest; typed
query algebra; result resolution/error/coverage; live snapshot/delta sequence. Contract đích đổi
khái niệm `sensitivity_scope` prototype thành `access_domain`.

Nghiệm thu:

- schema version lấy duy nhất từ version registry và vẫn là v1;
- validation có bound cho mọi list/string/payload/chunk; không lộ SQLite/vendor type;
- golden encode/decode bắt overflow, corrupt checksum, clock/scene mismatch, gap và point giả;
- lead AI APP approve ADR 0009 trước khi freeze public ABI.

### M02 — Dataset và query oracle đại diện

Deliverables: deterministic catalog/generator cho S01–S18/traffic; scenario nhiều camera với
topology, clock uncertainty, ID switch, split/merge và late correction; oracle cho các primitive
đã hỗ trợ; mỗi query phụ thuộc producer/center capability chưa có phải trả `unsupported`.

Nghiệm thu:

- mỗi capability group có oracle mô tả expected result/coverage và trạng thái support rõ;
- cùng một seed chạy được eSDK/QEMU và QCS6490;
- oracle phân biệt detection, episode, unique entity, passage và aggregate contribution.

### M03 — Ingest, catalog và packed detail shards

Deliverables: bounded ingest worker một writer; typed `catalog.db`; active SQLite detail shards
theo time/source; packed trajectory codec; typed indexes; seal/verify/publish/retire generation;
cursor lease, recovery; mandatory sampling và exact/fixed-gap/error-bounded policies.

Nghiệm thu:

- fact/outbox vẫn atomic; detail loss không làm mất receipt im lặng;
- power interruption, orphan shard, checksum error, disk-full và stale cursor fail closed;
- replay giữ đúng mandatory points/frame locators và đạt declared error bound;
- full quota dừng durable acceptance có trạng thái rõ, không corrupt lịch sử.

### M04 — Query service và live footprint

Deliverables: planner chọn live/catalog/detail/sealed/rollup; RAM snapshot-plus-delta; exact
spatial verification sau candidate index; paged selective query có deadline/scan/output budget,
cancellation; association revision read model cho đường đi qua camera. History vượt budget trả
bounded failure; async job/cold planner chỉ thêm sau khi có API/center requirement được duyệt.

Nghiệm thu:

- UI/backend không mở DB/file trực tiếp và không gửi SQL tự do;
- không nối trajectory qua gap/scene revision khi thiếu transform;
- query có deadline, scan-byte, output bounds, cancellation và không giữ active WAL reader;
- correction/tombstone/association revision deterministic theo snapshot token.

### M05 — Rollup và 18-usecase projections

Deliverables: episode dedup/state machines; stable aggregate contribution IDs; heatmap grids;
count/duration/denominator/coverage semantics; typed passage/event/plate/relation/traffic
projections; fire/smoke pipeline observation → episode → rollup → hotspot → evidence/review.

Nghiệm thu:

- tháng/năm đọc rollup, không scan mọi point; sampled drill-down khớp episode source;
- scene pose/calibration change tách heatmap revision, không cộng pixel grid im lặng;
- recross, late correction và retry không double count;
- từng S01–S18 pass oracle hoặc trả `unsupported` đúng capability receipt.

### M06 — Cold tier spike và quyết định công nghệ

Deliverables: pin-version Parquet writer/reader và DuckDB C API spike qua eSDK nếu khả thi;
manifest-published immutable files; row-group/source/time sort; so cùng dataset/query với SQLite
packed shards; không tải extension lúc runtime.

Nghiệm thu:

- license/SBOM/package size/cross-build/runtime dependencies được review;
- report bytes/point, compression, rows/bytes scanned, p50/p95/p99, CPU, RSS và flash bytes;
- chỉ chọn hybrid khi lợi ích được duyệt bù complexity; nếu không, ghi bằng chứng giữ sealed
  SQLite shards. Không claim production nếu gate chưa qua.

### M07 — Concurrent QCS6490 benchmark và capacity admission

Chạy đồng thời full AI workload; metadata ingest đại diện; live footprint; selective search;
month aggregate; một spatial/history job; compaction/seal; Kafka-offline outbox pressure.

Nghiệm thu:

- báo FPS, AI/metadata CPU, RSS, p50/p95/p99, lag/drop, DB/WAL/shard size, flash bytes,
  write amplification, checkpoint/compaction pause và thermal;
- không vi phạm target full-workload; lead chốt budget metadata riêng;
- profile chứng minh các horizon vừa quota với reserve;
- crash/restart, disk-full và query cancellation pass trên board.

### M08 — Compose, export và đóng P2

Deliverables: production service lifecycle ngoài frame/DSP threads; durable outbox/receipt API;
operational metrics; retention/purge; recovery và capability advertisement. Kafka transport và
center receipt consumer sử dụng API này trong P3, không mở file store trực tiếp.

Nghiệm thu đóng P2:

- M01–M07 pass và ADR 0009 được AI APP lead chuyển `accepted`;
- source/docs/catalog/cases đồng bộ; eSDK tests và board evidence có artifact digest;
- 18 security usecases và traffic matrix có fixture coverage; capability thiếu producer fail closed;
- 5 phút concurrent device test pass functional/performance gate; soak dài vẫn là release gate;
- không claim cross-device path, year-scale local retention hoặc exact replay ngoài profile đã đo.

Phân ranh kế hoạch: Kafka/ACK/export transport thuộc P3; concrete usecase producer và release
rollout thuộc P5. P2 vẫn phải cung cấp lifecycle/config hook và retention primitive để hai plan đó
không mở file DB trực tiếp. Không chuyển đầu việc sang P3/P5 để hợp thức hóa việc đóng P2 sớm.

## 5. Bằng chứng prototype được giữ lại

Ngày 2026-09-19, SQLite prototype `synchronous=FULL`, một transaction/record, Q08 page 128 đã
chạy native trên QCS6490:

| Dataset | Write | Q08 p50/p95/p99 | Max RSS | DB size |
|---|---:|---:|---:|---:|
| 2k record / 200 query | 15,121 record/s | 1.625 / 2.018 / 2.093 ms | 5,248 KiB | 663,552 B |
| 20k record / 500 query | 15,048 record/s | 14.855 / 16.303 / 16.731 ms | 7,040 KiB | 6,651,904 B |

Đây chỉ là bằng chứng primitive transaction/outbox/query chọn lọc. Nó không có live inference,
trajectory points, cross-camera footprint, concurrent compaction, month query hoặc cardinality
18-usecase, nên không còn là bằng chứng đóng P2.

## 6. Bằng chứng implementation workload

Candidate packed-shard/materialized-rollup có SHA-256
`c9945697b847ae64c8bb664011579c38ef3bdf03ebede76f2418907456a22190`, chạy trên board có
machine ID `09c89b1858f54955a3d13f2767622448` qua alias `lacai-home`.

| Thuộc tính | Kết quả 300 giây |
|---|---:|
| Profile | 27 security/traffic scenario, 4 source, 50 set/s, `FULL` sync, Kafka offline outbox |
| Durable ingest | 15.027 set / 45.081 record; reject/fail = 0 |
| Query/oracle | 9.952 query; query fail/oracle fail = 0 |
| Query p50/p95/p99 | 2,688 / 12,454 / 18,546 ms |
| Metadata CPU/RSS | 30,110% một core / 37.120 KiB max RSS |
| Store | 32.567.296 byte (`catalog.db` 25.542.656; detail 7.024.640) |
| AI APP đồng thời | 12,88% một core, RSS trung bình 371.233 KiB |
| Preview sau workload | H.264 1920x1080, 30,000 packet-PTS FPS; overlay person review pass |

Target-native codec/store/service tests cũng pass. Store test digest `957c37ee…cc18ab` bao phủ
deadline hết hạn, missing-shard trả partial, repair index và quota rejection; không được gọi đây
là power-cut hoặc filesystem-full test vật lý.

Thiết kế cũ query-time scan cùng cardinality dùng trung bình 46,56% metadata CPU và tăng theo
lịch sử. Vì vậy source đã chuyển latest-corrected aggregate sang rollup materialized giao dịch;
as-observed/as-known-at vẫn đọc revision history có budget.

Phép đo tăng khoảng 32,57 MB/5 phút, tương đương xấp xỉ 391 MB/giờ nếu giữ nguyên workload tổng
hợp. Đây là capacity evidence để buộc cấu hình retention; không được ngoại suy thành cam kết lưu
một tháng vì chưa có receipt transport P3 và cardinality production của 18 app.

Exact composed candidate commit `7e8538b9f3e15de4fc9da102e0efbf1ed419090b` có service SHA-256
`d02770e26610e213ec68cca55543e13378ca1d3aad8511eeb74bb58ecfc07a63` và profile SHA-256
`79cfc9848f05f8918681064836f9c5b97a014b720a22978e15ad5df976f294ce`. Trong đúng 300 giây,
AI APP dùng trung bình 13,16% một core, RSS trung bình 345.497 KiB, RTSP H.264 1920x1080 đạt
30,124 FPS. Store nhận/commit 26/26 work item, không reject/fail; 18 trajectory chunk có 18 ID
khác nhau. Stop kết thúc `stopped=true`, `first_error=0`, `cascade_failed=0`.

Fault suite trên cùng board chứng minh: reopen sau SIGKILL commit thêm 306 record không oracle
failure; filesystem tmpfs đầy thật trả 563 write failure rồi recovery commit 306/306; detail DB
bị ghi đè zero fail closed `file is not a database`; query cancellation và conflicting chunk ID
được kiểm native. Retention giữ shard/fact còn outbox chưa ACK. Evidence raw nằm tại
`/opt/lacai/out/p2_candidate/fault/` và `/opt/lacai/out/p2_acceptance_final_5m/`.

## 7. Điều kiện đóng P2

| Điều kiện | Kết quả |
|---|---|
| Validated composition và start/drain | Pass; `--metadata-profile`, worker ngoài hot path và clean stop trên exact candidate |
| Authorized trajectory + episode/rollup producer | Pass; thiếu source/model/feature capability trả `unsupported` |
| Receipt-safe retention và fault gates | Pass; restart, SIGKILL, disk-full, corruption, conflict và cancellation có board evidence |
| Exact candidate regression/performance | Pass; 142/142 eSDK, native regression, 300 giây, 30,124 FPS và 13,16% CPU |

P2 được đóng ở baseline v1. `accepted` không có nghĩa mọi Q01–Q30 đều chạy local: catalog công
bố query nào `supported`/`unsupported`, và không được suy diễn cross-device association,
year-scale local retention hoặc exact replay ngoài horizon. Kafka delivery thuộc P3; concrete
model/usecase packages và golden chất lượng thuộc P5.

## 8. Handoff sau khi đóng

- P3 dùng outbox/receipt API và phải phân biệt broker ACK với center-lake commit.
- P5 đăng ký producer theo capability; không được mở SQLite hoặc tự tạo schema song song.
- Chỉ mở edge Parquet/DuckDB khi có eSDK package/SBOM và A/B chứng minh lợi ích.
- Long-soak, thermal, released-FW và multi-source capacity vẫn là release gates, không phải P2.

## Tài liệu liên quan

- [Kiến trúc metadata không-thời gian](../../architecture/spatiotemporal_metadata.md)
- [Nghiên cứu nguồn storage/trajectory](../../research/metadata_storage_source_review.md)
- [ADR 0009](../../adr/0009_spatiotemporal_metadata_tiering.md)
- [Transactional prototype](../../architecture/metadata_query.md)
- [Architecture improvement master plan](README.md)
