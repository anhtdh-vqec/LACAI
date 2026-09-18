# Contract và phạm vi ba team

Plan này thiết lập contract authority v1, stable product IDs và quy trình bàn giao bắt buộc
giữa BSP+FW, AI APP và AI Model. Mục tiêu là hai team producer có thể bắt đầu cung cấp đúng
artifact/evidence ngay, không chờ AI APP đoán ABI hoặc semantics.

- **Status:** accepted — AI APP lead baseline đã được mã hóa thành registry/schema/checker;
  eSDK và QCS6490 `.98` conformance smoke pass ngày 2026-09-18.
- **Layer:** docs
- **Source:** [three-team integration registry](../../contracts/integration_contract_registry.md),
  `config/contracts`, `tools/contracts/vqec_vision_check_integration_contract.py`.

## Trách nhiệm

- AI APP lead là authority của integration boundary, stable IDs, feature/runtime semantics,
  authorization, admission và quyết định accept/reject end-to-end.
- BSP+FW là authority duy nhất của device/media facts, allocator/cache/fence/reset, released-FW
  transport, deployment và operations evidence.
- AI Model là authority duy nhất của artifact, exact preprocess/decode semantics, ontology,
  golden và quality evidence.
- Một schema có đúng một owner. Consumer không sửa nghĩa field trong adapter; owner không tự ký
  acceptance thay consumer.

## 1. Quyết định contract authority

Thứ tự authority bắt buộc:

1. requirement sản phẩm/security đã duyệt và canonical version registry;
2. `integration_contract_registry.json` cho ID/version/owner/mandatory fields;
3. contract chuyên biệt mà registry trỏ tới;
4. conformance receipt gắn exact artifact/report digest;
5. implementation note/example.

Tất cả LACAI-owned schema/ABI ở baseline đầu tiên là version `1`. Thay đổi stable ID, required
field, ownership/completion, authorization hoặc semantics phải có migration ADR. External FW ABI
và vendor ABI giữ version của owner bên ngoài; không được đổi số để khớp LACAI.

Một receipt bắt buộc chứa contract/version/registry revision, producer, artifact/config digest,
lệnh và môi trường test, valid/error report, deviation/expiry và consumer disposition. Schema
receipt kiểm tra producer authority, exact case coverage, digest, expiry và chỉ cho AI APP chốt
disposition; producer phải gửi ở trạng thái `pending`. Receipt không thay signature verification,
entitlement hoặc runtime admission.

## 2. Ma trận C01–C10 đã chốt

| ID | Schema owner | Producer bắt buộc | Consumer | Ranh giới không được đổi |
|---|---|---|---|---|
| C01 RAW source | BSP+FW | BSP+FW | AI APP | source/epoch/planes/clock, lease, final-reader ACK, quiesce/reset |
| C02 accelerator platform | BSP+FW | BSP+FW | AI APP, AI Model | SDK/ABI/toolchain, allocator/cache/fence/completion, signing, resource/thermal |
| C03 model integration kit | AI Model | AI Model | AI APP, BSP+FW | artifact, IO, preprocess/decode/quantization, ontology, M0–M5, limits/rollback |
| C04 scene/calibration/time | AI APP | BSP+FW cung cấp facts theo schema AI APP | BSP+FW, AI Model | coordinate/revision/validity/uncertainty, clock mapping, signal/access facts |
| C05 control/auth/admission | AI APP | AI APP; BSP+FW provision signed facts | BSP+FW, AI Model | installed/entitled/desired/supported/compatible/admitted/running tách biệt |
| C06 metadata/query | AI APP | AI APP | BSP+FW query clients | snapshot/paging/coverage/quality/retention/field authorization |
| C07 event/evidence | AI APP | AI APP event; BSP+FW media receipt | BSP+FW | phase/revision/idempotency; event ACK tách media receipt |
| C08 annotation/video | AI APP | AI APP compatibility writer | BSP+FW | source frame/transform/TTL/authorized fields, single writer/output generation |
| C09 cloud metadata | AI APP | AI APP | BSP+FW/cloud bridge | partition/order/outbox/dedup; broker ACK tách lake receipt |
| C10 deployment/operations | BSP+FW | BSP+FW | AI APP, AI Model | coherent manifest/SBOM/UID/path/volume/health/OTA/rollback |

Registry v1 bắt buộc cho mỗi contract: max bytes/rate, clock + unit, ownership + completion,
retry/idempotency/order, authenticated principal, authorization scopes, sensitive fields,
deny-by-default, error taxonomy, required fields và valid/rejected case.

## 3. Scope team không chồng lấn

### BSP+FW

Phải cung cấp C01/C02/C10 receipts và authoritative C04 facts. BSP+FW sở hữu camera/ISP, RAW
producer, external signal/time source, allocator/cache/fence/reset, SDK/image/signing, launcher,
storage volume, RTSP/UI/recording và evidence media. Team này không được:

- đặt model/preprocess/decode semantics trong FW branch;
- dùng `consumer_id`, tenant/customer field tự khai báo làm authentication;
- recycle buffer vì timeout/disconnect/FD close;
- biến D-Bus enable thành entitlement hoặc running readiness;
- fork usecase/event/query schema để tiện UI/backend.

### AI APP

Sở hữu C04–C09 schema, neutral ports, feature/usecase catalog, runtime dependency DAG, scheduling,
tracking/relations, event semantics, local metadata/query, cloud projection, authorization,
admission, Qualcomm/reference adapters, integration tests và acceptance report. AI APP không được:

- tự tuyên bố BSP completion/cache/reset hoặc model quality;
- tự điền golden/ontology/threshold thiếu từ model binary;
- expose vendor/FW types qua neutral contracts;
- giữ raw frame không bounded hoặc viết đè FW input;
- gọi empty result thay cho disabled/denied/unsupported/gap/expired/not-observable.

### AI Model

Phải giao C03 package hoàn chỉnh: identity/provenance, target compatibility, artifact + exact IO,
preprocess/quantization/decode/ontology, cadence/state reset, resource envelope, M0–M4 golden,
M5 quality/hard-negative/unknown rules, known limits và rollback. Team này không được:

- giao binary-only hoặc đổi tensor/ontology dưới cùng version;
- định nghĩa FW ABI, entitlement, retention, query implementation hay event delivery;
- coi score là calibrated probability nếu report không chứng minh;
- coi ReID/plate/face candidate là verified physical identity mặc định.

## 4. Stable security catalog S01–S18

Registry khóa 18 ID version 1, không gộp usecase để khớp số lượng:

| Code | Stable `usecase_id` | Current truth |
|---|---|---|
| S01 | `security.restricted_area_smoking` | unsupported |
| S02 | `security.suspicious_weapon` | unsupported |
| S03 | `security.ppe_compliance` | unsupported |
| S04 | `security.fire_smoke_detection` | partial |
| S05 | `security.blacklist_person_alert` | partial |
| S06 | `security.attendance_recognition` | partial |
| S07 | `security.demographic_estimation` | unsupported |
| S08 | `security.people_density_heatmap` | partial |
| S09 | `security.unauthorized_intrusion` | partial |
| S10 | `security.people_entry_exit_count` | partial |
| S11 | `security.person_tracking` | partial |
| S12 | `security.vlm_context_alert` | unsupported |
| S13 | `security.abandoned_or_removed_object` | unsupported |
| S14 | `security.lost_item_trace` | unsupported |
| S15 | `security.luggage_cart_tracking` | unsupported |
| S16 | `security.vehicle_plate_recognition` | unsupported |
| S17 | `security.crowd_gathering` | partial |
| S18 | `security.abnormal_fight_conflict` | unsupported |

Mỗi entry machine-readable đã có model-role, data, Q01–Q30 và output dependencies cùng reason.
`partial` chỉ nói dependency tái sử dụng đã tồn tại; không có nghĩa feature/quality đã accepted.
Traffic camera sau này thêm stable IDs mới trên common entity/track/attribute/relation/event core,
không đổi nghĩa S01–S18 và không giả định person-only.

## 5. Workflow bắt buộc cho hai producer team

1. Producer lấy registry version/revision hiện hành và contract chuyên biệt.
2. Producer nộp complete immutable handoff + receipt; không gửi link mutable hoặc binary-only.
3. AI APP chạy checker, schema/negative fixtures và scoped consumer conformance.
4. C01/C02 phải chạy fault/completion trên exact released target; C03 phải chạy M0–M6 theo stage.
5. AI APP trả `accepted`, `rejected` hoặc `accepted_with_deviation` cùng stable reason/deadline.
6. Chỉ artifact digest đã accept được vào deployment/admission. Receipt stale/mismatch trả
   `incompatible`, không tự fallback CPU hoặc bỏ validation.
7. Breaking change phải có ADR/migration/rollback và chạy lại mọi downstream receipt.

## 6. Kết quả thực hiện T01–T08

| Task | Kết quả |
|---|---|
| T01 | S01–S18 stable IDs/version/status/dependencies nằm trong registry v1 |
| T02 | C01/C02 bắt buộc capability/completion/security/resource receipt; BSP điền target facts |
| T03 | C03 required model-kit fields + binary-only rejection đã machine-check |
| T04 | C04/C05 clock/calibration/validity và independent state gates đã chốt |
| T05 | C07/C08 ACK/media/single-writer/generation/migration boundary đã chốt |
| T06 | C06/C09 snapshot/coverage/field auth/outbox/broker-vs-lake boundary đã chốt |
| T07 | 20 baseline cases, receipt validator và 6 negative mutation self-tests chạy không cần dependency ngoài |
| T08 | Authority, scope, unresolved external receipts và release gates được ghi rõ trong contract |

## 7. Gate đóng plan

- [x] Có đúng 18 stable usecase IDs; mỗi ID có model/data/query/output dependency và truthful status.
- [x] C01–C10 có một schema owner, consumers, version 1, bounds/rate, clock/units, ownership,
  retry/idempotency/order, security, errors, required fields và valid/rejected case.
- [x] C01 cấm recycle do timeout/disconnect/FD close; final-reader completion/quiesce là bắt buộc.
- [x] C03 cấm binary-only; M0–M5, unknown/limits/rollback là mandatory handoff.
- [x] C04 giữ revision/validity/uncertainty; speed/signal/access không hợp lệ fail closed.
- [x] C06 phân biệt empty với disabled/denied/unsupported/coverage gap/expired/not-observable.
- [x] C07 tách event ACK và media receipt; duplicate/retry/outbox-full có reason.
- [x] C08 khóa single writer/output generation; legacy migration không bị gỡ trước FW acceptance.
- [x] C09 tách broker ACK/lake receipt; C10 yêu cầu atomic coherent set và rollback.
- [x] Receipt schema/CLI khóa authoritative producer, revision, SHA-256, exact case coverage,
  expiry, deviation và AI APP consumer disposition.
- [x] Checker pass eSDK CTest và chạy trực tiếp trên QCS6490 `.98`.
- [x] BSP+FW/AI Model receipts còn thiếu được giữ là external release gate đúng owner, không đẩy
  ngược ambiguity vào AI APP implementation.

Plan này được đóng ở mức **accepted contract baseline** theo quyết định AI APP lead. “Đóng plan”
không có nghĩa BSP+FW đã chứng minh released-FW completion hoặc AI Model đã ký quality; hai team
phải nộp receipts theo contract này trước khi capability tương ứng được promote.

## Bàn giao sang plan khác

- Plan metadata/query nhận stable S01–S18 dependencies và C04/C06/C09 semantics.
- Plan event/evidence nhận C01/C04/C07/C08 boundaries.
- Plan DSP đã nhận C02/C03 owner split; BSP signing và Model quality vẫn giữ external gate.
- Integration/rollout chỉ promote target/model có receipt đúng registry revision và digest.

## Giới hạn và công việc tiếp theo

- Chưa có released-FW C01/C02/C10 receipt hoặc AI Model C03 quality receipt; AI APP không giả lập
  chữ ký của hai owner này.
- Registry checker chứng minh completeness/invariants của contract pack, không chứng minh hardware,
  model accuracy, entitlement signature hoặc cloud/data-lake delivery.

## See also

- [Three-team integration registry](../../contracts/integration_contract_registry.md)
- [FW–AI APP contract](../../contracts/fw_ai_app_contract.md)
- [Usecase control](../../contracts/fw_usecase_control.md)
- [Model integration](../../contracts/model_integration.md)
- [Architecture improvement master plan](README.md)
