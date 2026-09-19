# Kế hoạch metadata và truy vấn security/traffic

Plan 2 thiết lập authority dữ liệu D01–D18, capability Q01–Q30 và local transactional
baseline dùng chung cho 18 usecase security, đồng thời không khóa đường mở rộng traffic.

**Status:** accepted — AI APP baseline đã qua eSDK/QEMU và QCS6490 native test ngày 2026-09-19. **Layer:** docs. **Source:** `config/contracts/metadata_query_catalog.json`, `include/vqec/vision/ai/contracts/vqec_vision_metadata_query.hpp`, `src/adapters/storage/vqec_vision_sqlite_metadata_store.cpp`.

## 1. Phạm vi đã đóng

P2 đóng phần nền tảng thuộc AI APP:

- một catalog v1 duy nhất cho D01–D18, Q01–Q30, S01–S18 và traffic extension;
- neutral C++ contract không lộ SQLite/vendor type;
- SQLite WAL writer một owner, append revision, fact + outbox cùng transaction;
- typed prepared query, quyền theo scope, projection, coverage, snapshot/keyset paging;
- quyết định SQLite là hot transactional baseline và rule mở lại hybrid;
- contract fixtures cho năm outcome của mọi query và native fixtures person, temporal
  attribute/passage, ANPR, vehicle aggregate, gap, retry, recovery.

“Đóng P2” không có nghĩa 18 thuật toán AI đã được triển khai, mọi query đã active, retention
đã được sizing hay Kafka/purge worker đã có. Capability thiếu producer/access path phải trả
`unsupported`; từng app chỉ được quảng bá query sau receipt chất lượng và capacity riêng.

## 2. Authority và ranh giới team

| Hạng mục | Owner | Contract bắt buộc |
|---|---|---|
| Schema, query semantics, DB, index, authorization, retention projection | AI APP | D01–D18/Q01–Q30 v1; team khác không ghi trực tiếp DB |
| Fact từ model, ontology, threshold và quality envelope | AI Model | C03/C05 receipt; không tự đổi schema/query |
| Source/time/calibration/signal/media fact | BSP+FW | C01/C02/C04/C07 receipt; không cung cấp SQL/file DB |
| Query/API/App Manager và backend contract | AI APP | backend gọi API/D-Bus do AI APP định nghĩa |
| Video/ảnh evidence | BSP+FW media service | DB chỉ giữ D15 reference và trạng thái, không giữ media bytes |
| Kafka schema/outbox/replay ở edge và center ingest thuộc sản phẩm | AI APP | local commit, broker ACK và lake commit là receipt khác nhau |

Store chạy ngoài frame/inference/DSP/overlay thread. Tensor, raw frame, crop mặc định, face
embedding, key/token và model binary không được đưa vào metadata DB.

## 3. Coverage 18 usecase security

Machine authority nằm trong `metadata_query_catalog.json`; bảng dưới là bản review cho lead.

| ID | Usecase | Record chính tạo ra | Query dự kiến |
|---|---|---|---|
| S01 | Hút thuốc vùng cấm | D05, D06, D10, D15 | Q02, Q05, Q08 |
| S02 | Vũ khí/vật thể nghi ngờ | D02, D05, D06, D10, D15 | Q01, Q02, Q06, Q08 |
| S03 | PPE | D05, D06, D10, D14 | Q01, Q05, Q08, Q15 |
| S04 | Cháy/khói | D02, D10, D14, D15 | Q08, Q15 |
| S05 | Người blacklist | D06, D10, D11, D15 | Q08, Q09, Q26 |
| S06 | Điểm danh | D11, D14, D18 | Q09, Q10 |
| S07 | Tuổi/giới tính | D05, D14 | Q01, Q14, Q15 |
| S08 | Mật độ/heatmap | D04, D08, D14 | Q14, Q15 |
| S09 | Xâm nhập | D07, D10, D15 | Q04, Q05, Q08 |
| S10 | Đếm người ra/vào | D07, D14 | Q04, Q14, Q15 |
| S11 | Theo dõi người | D02, D03, D04, D05, D16 | Q01, Q03–Q05, Q07 |
| S12 | Cảnh báo VLM | D10, D13, D15 | Q08, Q17, Q26 |
| S13 | Bỏ quên/biến mất | D03, D05–D07, D10, D15 | Q06, Q08, Q11 |
| S14 | Truy vết đồ thất lạc | D03–D06, D16 | Q01, Q03, Q06, Q07, Q16 |
| S15 | Theo dõi hành lý/xe đẩy | D03, D04, D06, D07, D14 | Q03–Q06, Q14 |
| S16 | Biển số xe | D03, D05–D07, D12, D15 | Q12, Q13, Q18 |
| S17 | Tụ tập đông người | D08, D10, D14, D15 | Q08, Q14, Q15 |
| S18 | Ẩu đả/xung đột | D05, D06, D10, D15 | Q06, Q08, Q15, Q26 |

Record dùng chung không bị nhân bản theo app. Quyền của record được tính từ field thực tế,
không kế thừa quyền của app đầu tiên tạo ra nó.

## 4. Coverage traffic mở rộng

| Profile | Dữ liệu bắt buộc | Query | Gate trước khi active |
|---|---|---|---|
| Vehicle flow | D01, D03, D05, D07, D14, D17 | Q14, Q18 | line/lane/direction và dedup basis |
| Lane movement | D01, D04, D07, D10, D17 | Q04, Q19, Q25 | topology revision, unknown lane |
| ANPR/parking/access | D05, D07, D09, D12, D14, D17 | Q12, Q13, Q20 | normalization revision, session ambiguity |
| Speed/acceleration | D01, D04, D08, D17 | Q21, Q25 | calibration, unit, uncertainty |
| Queue/congestion | D07, D08, D14, D17 | Q20, Q22 | physical unit, denominator, coverage |
| Movement violation | D01, D07, D08, D10, D15, D17 | Q19, Q23 | rule/geometry/clock validity |
| Red-light | D01, D07, D09, D10, D15, D17 | Q23, Q26 | authoritative signal interval |
| OD/travel time | D01, D07, D16, D17 | Q07, Q24 | matching scope, censoring, clock uncertainty |
| Near miss/incident | D04, D06, D08, D10, D15, D17 | Q06, Q23, Q26 | trajectory quality và method revision |

Traffic không được suy speed/violation từ pixel khi thiếu calibration/signal/time. Kết quả
phải là `unknown` hoặc `unsupported`, không phải số 0 hay “không vi phạm”.

## 5. Capability query v1

| Trạng thái baseline | Query IDs | Ý nghĩa |
|---|---|---|
| Local access path đã có | Q01–Q06, Q08, Q12, Q14, Q15, Q18, Q26–Q29 | Prepared query và paging có trong SQLite adapter |
| Khai báo nhưng fail closed | Q07, Q09–Q11, Q13, Q16, Q17, Q19–Q25, Q30 | Chờ producer, fusion/index hoặc workflow phù hợp |

Q02 là case chuẩn: attribute phải valid tại thời điểm passage, không lấy latest value; caller
phải có cả `visual_attribute` và `trajectory`. Q28 là stable authorized read/export foundation.
Q29 đọc audit correction/purge; không tự thực thi xóa. Query không nhận SQL tự do.

Response luôn mang snapshot, cursor, authorization revision, coverage và
`complete|partial|approximate|unsupported|budget_exceeded`. `complete` chỉ nghĩa đã đọc đủ
dữ liệu hợp lệ trong snapshot, không khẳng định detector nhìn thấy mọi đối tượng.

## 6. Kiến trúc lưu trữ đã chọn

```text
feature/perception facts
        |
        v
bounded metadata output worker (chưa compose vào service)
        |
        +-- SQLite WAL: typed envelope + revision + indexes
        |       +-- authorized snapshot/keyset query
        |       `-- fact + pending outbox in one transaction
        |
        `-- future exporter --> Kafka --> center data lake
```

SQLite được chọn cho hot transactional authority, không phải vì SQL giải mọi bài toán. Query
trajectory dựa trên D03/D04/D07 đã chuẩn hóa, index source/subject/scene/time và exact semantic
check; không scan raw frame. Parquet/DuckDB chỉ mở khi retention/analytics workload thực tế vượt
budget và dependency đã qua eSDK/license/SBOM. Không viết custom binary store trước khi có bằng
chứng SQLite là bottleneck.

## 7. Kết quả task M01–M08

| Task | Kết quả phiên đóng P2 | Disposition |
|---|---|---|
| M01 catalog | D01–D18, unit/value-state/provenance/privacy trong machine catalog + C++ contract | Done |
| M02 fixtures | 150 outcome rows; native person/scene/ANPR/vehicle/gap/recovery fixtures | Done cho contract; model golden là gate từng app |
| M03 query contract | Q01–Q30, scope/projection/snapshot/coverage/unsupported | Done |
| M04 SQLite | WAL, config bounds, prepared query, revision, outbox, recovery | Done |
| M05 platform facts | C01/C02/C04/C07 authority đã khóa; thiếu receipt làm capability unsupported | Contract done; producer receipt external |
| M06 benchmark | Cùng binary fixture chạy eSDK/QEMU và QCS6490; số đo bên dưới | Done |
| M07 hybrid spike | Không kích hoạt: eSDK chưa có dependency và SQLite chưa fail approved SLO | Correctly deferred |
| M08 retention/audit | State/audit schema và tombstone revision có; physical purge/export worker chưa thuộc baseline | Contract done; execution ở Plan 3/5 |

## 8. Bằng chứng nghiệm thu

Ngày 2026-09-19, target `lacai-home` do user authorize trỏ tới QCS6490 `aarch64`; workspace
`/opt/lacai` sẵn sàng. Native test pass các case transaction/outbox, exact retry và conflict,
Q02 temporal join + combined authorization, Q12 plate, Q18 vehicle aggregate, coverage gap,
unsupported query, projection, stable paging và reopen recovery.

Benchmark dùng SQLite `synchronous=FULL`, một transaction/record, Q08 page 128:

| Dataset | Write | Q08 p50/p95/p99 | Max RSS | DB size |
|---|---:|---:|---:|---:|
| 2k record / 200 query | 15,121 record/s | 1.625 / 2.018 / 2.093 ms | 5,248 KiB | 663,552 B |
| 20k record / 500 query | 15,048 record/s | 14.855 / 16.303 / 16.731 ms | 7,040 KiB | 6,651,904 B |

Đây là fixture quyết định kiến trúc, không phải SLO/capacity cho 18 app. Metadata store chưa
wired vào live inference nên không được suy ra ảnh hưởng FPS/CPU của full service.

Checklist đóng P2:

- [x] S01–S18 map đủ produces/requires/queries; scene-only, plate, identity và traffic không bị ép vào person track.
- [x] D01–D18 và Q01–Q30 có canonical ID v1; năm outcome được kiểm machine-readable.
- [x] Event/recorded time, epoch, revision, value state, coverage và provenance có contract.
- [x] Q02 dùng valid-time join và quyền kép; empty/zero không che coverage gap.
- [x] SQLite giữ writer bounded theo config, prepared statement, WAL/recovery, outbox atomic và keyset paging.
- [x] Storage decision có ADR, số đo target và rule rõ để mở lại hybrid.
- [x] Tài liệu phân biệt source, board-smoke, product sizing và per-usecase activation.

## 9. Giới hạn và công việc tiếp theo

- Plan 3 nhận outbox để làm Kafka/UDS delivery, retry và receipt; không sửa transaction authority.
- Plan 5 compose bounded metadata worker vào service, đo live CPU/FPS/I/O và chạy retention,
  disk-full, power-loss, purge/revoke, long-query cancellation.
- Trước khi lưu D11/D12, Plan 5 phải enforce private directory cho DB/WAL/SHM và chốt
  encryption/hardware-key policy; query authorization không thay thế bảo vệ dữ liệu at-rest.
- Mỗi app nộp producer golden, quality/calibration và cardinality/retention profile trước khi
  chuyển các query liên quan từ `unsupported` sang advertised capability.
- Chỉ mở cold Parquet/DuckDB spike khi workload đã duyệt làm SQLite vi phạm budget; benchmark
  phải dùng cùng dataset/query/projection và báo p50/p95/p99, RSS, CPU, flash, bytes scanned.

## Tài liệu liên quan

- [Metadata query architecture](../../architecture/metadata_query.md)
- [Transactional metadata store ADR](../../adr/0008_transactional_metadata_store.md)
- [Architecture improvement master plan](README.md)
- [Observation contract](../../architecture/observation_contract.md)
- [Integration contract registry](../../contracts/integration_contract_registry.md)
