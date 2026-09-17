# Contract và phạm vi ba team

Plan này biến phân tích ownership thành các contract có thể ký, test và bàn giao giữa
BSP+FW, AI APP và AI Model. Không tạo ABI mới trong plan; mọi thay đổi external boundary
phải được cập nhật contract/ADR trước source.

- **Status:** planned — chỉ có thiết kế và checklist, chưa có implementation của plan này.
- **Layer:** docs
- **Source:** [FW–AI APP contract](../../contracts/fw_ai_app_contract.md),
  [model integration contract](../../contracts/model_integration.md), `n/a` cho đề xuất mới.

## Trách nhiệm

- AI APP lead sở hữu kiến trúc tích hợp, data/query catalog, runtime semantics và acceptance
  sản phẩm AI; điều phối nhưng không tự phê duyệt primitive BSP hoặc model quality.
- BSP+FW sở hữu device/media, RAW lease, allocator/cache/fence/reset, signal/time facts,
  evidence media, deployment, storage volume và target compatibility.
- AI Model sở hữu artifact/model kit, ontology, preprocess/decode semantics, golden fixtures,
  quality report và giới hạn không quan sát được.
- Mỗi contract có đúng một owner schema; bên nhận phải ký conformance. `Edge data service`
  và `Kafka exporter` là module thuộc AI APP, không phải team thứ tư.

## 1. Ma trận bàn giao

| ID | Bàn giao | Owner | Consumer | Nội dung bắt buộc |
|---|---|---|---|---|
| C01 | RAW source | BSP+FW | AI APP | profile/source ID, planes/stride/modifier, epoch/clock, lease, cache/fence, completion, reset |
| C02 | Accelerator platform | BSP+FW | AI APP | SDK/ABI/toolchain, allocator/import, HTP/cDSP capability, signing, thermal/resource, quiesce |
| C03 | Model integration kit | AI Model | AI APP | artifact provenance, tensor/preprocess/decode/quantization, ontology, cadence, quality, golden |
| C04 | Scene/calibration/time | BSP+FW cung cấp facts; AI APP quản lý schema | AI APP/AI Model | FOV/pose, zones/lines/lanes, CRS, calibration error/validity, signal/access/clock revision |
| C05 | Control/auth/admission | AI APP + BSP+FW | cả ba | desired/entitlement/revision, source/field/export scopes, readiness, quota, reason codes |
| C06 | Metadata/query | AI APP | BSP+FW query clients | D01–D18, Q01–Q30, snapshot/paging, completeness/quality/retention, field authorization |
| C07 | Event/evidence | AI APP ↔ BSP+FW | cả hai | event/request IDs, phase/revision, ACK levels, pre/post-roll, media receipt, dedup/reconcile |
| C08 | Annotation/video | AI APP | BSP+FW | frame/epoch/time/transform/TTL, field scope, legacy/new video-owner mode |
| C09 | Cloud metadata | AI APP | center/platform | schema/partition/order, outbox, dedup, offline quota, broker ACK và lake receipt |
| C10 | Deployment/operations | BSP+FW | cả ba | manifest/SBOM, paths/UID/volume, health, OTA/rollback, logs/metrics, compatibility matrix |

Mỗi contract phải có: schema/IDL, version policy, field units, max message/bytes/rate,
ownership và completion, retry/idempotency, security principal, error taxonomy, valid/error
fixtures, compatibility matrix, test command và owner sign-off. Không chấp nhận chỉ một
header C++ hoặc một D-Bus method có tên đúng.

## 2. Phạm vi team chi tiết

### BSP+FW

1. Công bố RAW source và media/evidence capability; chứng minh stride, offset, modifier,
   cache/fence, completion thật và quiesce sau disconnect/reset.
2. Cung cấp timestamp/profile/PTZ/signal/access/roster facts theo clock/revision; không để
   AI suy ra signal hay calibration từ pixels khi đã có authoritative input.
3. Sở hữu encoded video, prebuffer, clip/snapshot, RTSP/UI, evidence media và storage
   durability. Giai đoạn hiện tại vẫn giữ AI preview encoder theo compatibility contract.
4. Cung cấp target SDK/Hexagon toolchain/signing/power/thermal traces và package install,
   supervisor, quota/credentials. Không đưa private SDK headers vào neutral LACAI.
5. Cấp mock/board harness cho C01/C02/C07/C08/C10 và ký released-FW conformance.

### AI APP

1. Sở hữu usecase catalog, dependency/admission, perception orchestration, tracker,
   attribute freshness, relations, scene rules, event semantics và output authorization.
2. Sở hữu neutral ports, Qualcomm/reference adapters, DSP integration, buffer lifetime,
   feature processors, metadata canonical schema/read models/query API và Kafka exporter.
3. Sở hữu event intent, outbox/retry/reconcile; không tự ghi video hoặc nhận quyền media.
4. Sở hữu local retention/privacy projection, field authorization, archive manifest,
   correction/tombstone và audit. Dữ liệu nhạy cảm phải có scope riêng.
5. Sở hữu integration test, workload manifest, eSDK build, QEMU logic evidence, board
   acceptance orchestration và status truthful; không tuyên bố model quality thay AI Model.

### AI Model

1. Bàn giao model kit hoàn chỉnh, không chỉ `.so`: tensor identity, transform,
   quantization, decoder, label/attribute ontology, cadence, ROI limits, temporal reset.
2. Cung cấp golden input/tensor/output/observations, calibration/quality report,
   hard negatives, unknown/not-observable rules và model-version migration.
3. Xác nhận semantics cho human/vehicle/plate/scene/VLM; không tự định nghĩa retention,
   entitlement, UI, FW ABI hoặc local query implementation.
4. Ký M0–M4 completeness/load/golden/decode; phối hợp replay và mixed-load accuracy.

## 3. Quy trình làm việc bắt buộc

1. AI APP mở issue/contract draft với owner, consumer, version, scope và acceptance.
2. BSP+FW chốt thiết bị/ABI/lifetime; AI Model chốt semantics/golden; AI APP chốt neutral
   representation và error/quality behavior.
3. Hai phía tạo fixture độc lập và test negative trước khi triển khai producer/consumer.
4. Thay đổi field/ownership/entitlement/clock/quantization là breaking integration change:
   tăng version, migration note, rollback và chạy lại downstream tests.
5. Consumer không được dùng field ngoài contract; producer không được tự mở rộng quyền.
6. Chỉ khi cả owner và consumer ký report thì plan downstream được phép bắt đầu.

## 4. Task có thể giao ngay

| Task | Người thực hiện | Đầu ra | Phụ thuộc |
|---|---|---|---|
| T01 | AI APP lead | Chuyển 18 usecase thành stable IDs/version và owner matrix | lead quyết định |
| T02 | BSP+FW | C01/C02 capability sheet: profile, memory, clock, completion, reset, toolchain | target profile |
| T03 | AI Model | C03 kit template + một detector/attribute/ANPR golden package | model artifact |
| T04 | AI APP | C04/C05 schema: scene, calibration, clock, grants, admission/reason | T02/T03 |
| T05 | AI APP + BSP+FW | C07/C08 envelope/ACK/media migration draft | C01/C04 |
| T06 | AI APP | C06/C09 schema/query/outbox draft và privacy classification | T01/T03 |
| T07 | Cả ba | valid/error fixtures và independent contract test harness | T02–T06 |
| T08 | AI APP lead | boundary review record, unresolved decision list và release gate | T07 |

## 5. Tiêu chí nghiệm thu

- [ ] Có 18 stable usecase IDs, không gộp nghiệp vụ chỉ để đạt con số 16; mỗi ID có
  model/data/query/output dependency và trạng thái supported/partial/unsupported.
- [ ] C01–C10 có owner, consumer, version, max size/rate, clock/units, ownership,
  retry/idempotency, security, error codes và fixture lỗi.
- [ ] C01 chứng minh producer chỉ recycle buffer sau hardware completion; timeout/FD close
  không được dùng làm ACK.
- [ ] C03 không phải binary-only; golden preprocess/decode và quality/unknown có report.
- [ ] C04 thể hiện scene/calibration/signal revision và invalidity; không bịa speed/red-light.
- [ ] C06 field authorization không chỉ ở UI; query “không có data” phân biệt với disabled,
  expired, unsupported và coverage gap.
- [ ] C07 duplicate/retry/lost ACK/restart/disk full có disposition; media receipt tách event ACK.
- [ ] C08 có một writer và legacy migration/rollback; chưa gỡ AI encoder nếu FW chưa ký.
- [ ] C09 phân biệt broker delivery với lake commit; C10 có install/reboot/rollback evidence.
- [ ] Cả ba team ký boundary review; mọi mục chưa có owner là blocker, không đẩy sang plan khác.

## Bàn giao sang plan khác

Plan 2 chỉ bắt đầu khi C03–C06 có schema và fixtures; plan 3 cần C01/C04/C07; plan 4 cần
C02/C03; plan 5 cần tất cả contract cùng workload manifest. Các mục chưa ký giữ trạng thái
`planned`, không tạo source production để “giữ tiến độ”.

## Giới hạn và công việc tiếp theo

- Đây là plan contract, không supersede các contract hiện hành; thay đổi boundary phải tạo
  ADR/contract change riêng.
- Đầu tiên thực hiện T01–T04, sau đó review với hai lead còn lại trước khi viết adapter.

## See also

- [Architecture improvement master plan](README.md)
- [FW–AI APP contract](../../contracts/fw_ai_app_contract.md)
- [Usecase control](../../contracts/fw_usecase_control.md)
- [Model integration](../../contracts/model_integration.md)
