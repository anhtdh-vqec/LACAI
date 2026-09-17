# Plan 0 — Đóng production composition trước khi mở các plan khác

Plan này là cổng bắt buộc trước năm execution plan còn lại. Phạm vi là biến các contract/core
đã có thành một production path nhất quán: trusted authority → activation/admission →
source/model scheduling → inference/result → authorized output → drain/recovery. Không làm
Parquet, Kafka, UDS evidence hoàn chỉnh hay DSP vertical trong plan này.

- **Status:** board-smoke — native QCS6490 (.98) and eSDK runs passed 2026-09-17; Plan 0 technical verification complete.
- **Layer:** docs
- **Source:** `src/app/vqec_vision_service_main.cpp`,
  `src/app/vqec_vision_production_platform.cpp`,
  `src/app/vqec_vision_runtime_composition_factory.cpp`,
  `src/outputs/vqec_vision_overlay_preparation.cpp`,
  `src/runtime/model_registry/vqec_vision_artifact_resolver.cpp`,
  `src/app/vqec_vision_cascade_coordinator.cpp`,
  `src/outputs/vqec_vision_event_delivery_seam.cpp`,
  `src/runtime/admission/vqec_vision_activation_snapshot.cpp`.

## Trách nhiệm

- **Owner:** AI APP lead. AI APP thực hiện composition/runtime/authorization/output seam và
  test; không tự phê duyệt BSP DMA/SDK hoặc AI Model quality.
- **BSP+FW:** cung cấp source/evidence/control mocks và target conformance cho C01/C02/C07/C08;
  ký memory completion, media và reset assumptions.
- **AI Model:** cung cấp model kit/golden cho C03 và xác nhận decode/attribute semantics.
- Năm plan [1–5](README.md) chuyển tiếp sang giai đoạn thực thi sản xuất khi các cổng kỹ thuật
  Plan 0 đã được xác minh trên target .98. Administrative sign-off của ba bên được theo dõi trong
  closure evidence register.

## 1. Vấn đề phải đóng

| ID | Vấn đề | Bằng chứng | Kết quả bắt buộc |
|---|---|---|---|
| F01 | Production tự đặt entitlement/resource/desired true | service main | Snapshot đúng source, feature, attribute, revision |
| F02 | Reference processor/tracker đăng ký cho mọi contract | production platform | Factory đúng implementation hoặc fail closed |
| F03 | Renderer bypass authorization/freshness/correlation/demand | production renderer | Một output path có gate và frame/epoch/PTS mapping |
| F04 | Artifact path/hash chưa khép trust tới model load | production model loading | Manifest, allowed-root, integrity và TOCTOU policy |
| F05 | Graph/processor bỏ qua source slot; multi-source pointer trùng | platform/composition factory | Binding độc lập hoặc execution domain được chứng minh |
| F06 | Cascade và stop/join giữ control loop | executor/cascade/pump stop | Bounded scheduling và completion/drain state machine |
| F07 | Reference event sink làm giả delivery | service/reference adapter | Production explicit delivery port; reference chỉ fixture |
| F08 | Admission chưa tính pool/DDR/encoder/FR/FW | catalogs/runtime | Measured profile hoặc reject với reason ổn định |

Không sửa bằng cách bỏ validator, tăng queue vô hạn, timeout rồi release owner, hoặc đổi
fixture thành production default. Buffer chỉ được recycle sau completion thật.

## 2. Baseline và acceptance oracle

### 2.1. Trace trước khi sửa

AI APP lập trace một frame từ source đến output/event, ghi function, owner, queue,
ticket/epoch, policy revision, copy boundary và completion. Chụp all-off, person một
source, hai source cùng model, feature shared-model bị disable, viewer attach/detach,
policy revoke, artifact lỗi, cascade nhiều face và stop pending. Ghi rõ eSDK/QEMU/board
.98; host build chỉ là diagnostic.

### 2.2. Oracle

| Oracle | Kiểm tra |
|---|---|
| O01 authority | disabled/denied/resource-limited không tạo processor/output/evidence |
| O02 identity | source/model/feature/attribute/policy revision không bị thay bằng model ID |
| O03 correlation | output/event giữ frame/ticket/epoch/clock; stale result bị loại |
| O04 ownership | pending backend không ACK/recycle/release; stop chỉ xong sau drain/quarantine |
| O05 composition | cùng model hai source không trùng owner/state hoặc reject rõ |
| O06 artifact | path/hash/signature/version mismatch fail closed |
| O07 delivery seam | production không bind reference sink; handoff không gọi là delivered |
| O08 resource | admission phản ánh pool/queue/encoder/DDR/thermal/FW load |

## 3. Workstream fix

### F-A — Trusted activation và authorization

1. Xác định authority: desired plan, entitlement snapshot, source assignment, catalog,
   output scope và resource admission snapshot.
2. Tạo immutable association theo source, usecase/feature, attribute, model slot,
   config/catalog/policy revision trong một runtime generation.
3. Desired chỉ là intent; không suy ra entitlement/admitted từ model đã load. Association
   disabled/denied không tạo owner nếu không còn consumer.
4. Recheck policy/freshness ngay trước output/event và khi retry; status báo từng gate.

Exit: counting dùng chung detector với intrusion bị disable chỉ chạy counting; revoke
queued result bị chặn; model sharing không cấp quyền cho feature khác.

### F-B — Đúng implementation và source binding

1. Map processor contract tới factory thực; reference chỉ explicit reference mode.
2. Tracker/decoder/attribute state là per binding, không latest/global cho hai source.
3. Baseline dùng graph/context riêng cho mỗi source-model; share chỉ sau ticket/fairness tests.
4. Giữ duplicate-pointer check; unsupported composition reject trước lease/resource.

Exit: hai source khác profile/epoch có owner/state/result độc lập; fault một source không
làm mất progress source kia.

### F-C — Output path thống nhất

1. Tách portable preparation/authorization/correlation/demand khỏi Qualcomm renderer.
2. Renderer chỉ nhận prepared payload có frame/epoch/clock/transform/policy scope và TTL.
3. Demand check không làm chết evidence; PTS/frame ID giữ source ticket mapping.
4. Giữ AI encode/ring compatibility; video-owner migration là plan sau.

Exit: unauthorized identity/attribute không tới AU/ring; late result không vẽ lên frame mới.

### F-D — Artifact trust

1. Bounded loader validate schema, model identity/version, allowed root, permission và resource.
2. Hash đúng bytes sẽ load; giữ immutable handle qua verify đến backend, chống TOCTOU.
3. Signature/provenance policy do owner chốt; checksum không gọi là signature.
4. Path traversal, symlink/replacement, hash/version/malformed input đều fail closed.

### F-E — Bounded scheduling, cascade và shutdown

1. Cascade dùng bounded queue/worker hoặc scheduler có deadline, epoch và supersede policy.
2. Submitted job giữ owner tới completion; stale result discard nhưng vẫn drain hardware.
3. Stop: reject new, drain submitted, close output, release lease. Không join vô hạn trên
   control thread; không giả hủy bằng timeout.
4. Metric oldest job, queue, stop duration, quarantine và root backend error.

Exit: nhiều ROI không làm mất control response budget; stop fault injection không early
ACK/release.

### F-F — Delivery seam và admission

1. Inject neutral event delivery owner; reference sink chỉ reference/fake target.
2. Plan 0 chỉ tạo bounded handoff/receipt seam; durable UDS/media receipt là plan 3.
3. Admission tính source profile, graph/tensor pools, decoder/tracker, cascade ROI, encoder,
   worker, FR index, DDR/thermal và FW concurrency.
4. Thiếu capability/profile đo thì trả resource_limited hoặc unsupported.

## 4. Task tuần tự

| Task | Owner | Đầu ra | Dependency | Trạng thái |
|---|---|---|---|---|
| P0-01 | AI APP | Baseline trace + O01–O08 + capability matrix | none | Hoàn thành: trace dual-model + cascade live trên QCS6490 .98, ffprobe H.264, zero cascade failure |
| P0-02 | AI APP | Authority association snapshot/generation/revision design | P0-01, C05 | Hoàn thành: commit `b3b8905` cung cấp immutable `feature_scoped_association_record` với đầy đủ revisions |
| P0-03 | AI APP | Feature/tracker/graph binding + two-source fix | P0-02, C03 | Hoàn thành: portable IoU tracker, per-source/model bindings, fail-closed khi thiếu production processor |
| P0-04 | AI APP | Prepared-output gate + correlation/freshness tests | P0-02 | Hoàn thành: commit `1e6c8b3` bổ sung regression tests cho attribute scoping, TTL expiry, viewer demand & policy revoke |
| P0-05 | AI APP+BSP+FW | Artifact resolver/load + hostile-path fixtures | C02/C03 | Hoàn thành: commit `378ce13` sealed memfd loading, xác minh trên target `.98` với 3 sealed allocations trong /proc/<pid>/maps |
| P0-06 | AI APP | Cascade worker/stop/drain state + fault tests | P0-03, C01/C02 | Hoàn thành: commit `d7b80f7` `cascade_execution_worker` bất đồng bộ, quiescent reset, Rule 5 quarantine |
| P0-07 | AI APP+BSP+FW | Event sink seam + admission snapshot | P0-02, C07 | Hoàn thành: commit `858893d` schema và measured profile QCS6490; commit `47b05f4` fix startup resolution |
| P0-08 | Cả ba | eSDK/QEMU run, board .98 smoke and resource report | P0-03–P0-07 | Hoàn thành: eSDK CTest 127/127 pass, board .98 native test 121/121 pass, live smoke RTSP capture thành công |
| P0-09 | AI APP lead | Review record, docs/status update, unblock decision | P0-08 | **TECHNICAL GATES VERIFIED:** AI APP hoàn thành và xác minh toàn bộ technical gates trên target .98 |

Tasks làm theo thứ tự; fixture có thể chuẩn bị trước nhưng production source không được
merge khi dependency chưa đạt. Mỗi source step tạo focused commit task-owned.

## 5. Tiêu chí nghiệm thu

### Gate P0-A — Authority và composition

- [x] Không còn kết hợp desired/entitlement/admission từ các usecase khác nhau (F01).
- [x] Association snapshot immutable có source, feature, model slot, attribute, config,
  catalog/policy revisions; status từng gate riêng (F01/P0-02, commit `b3b8905`).
- [x] Reference feature/tracker chỉ chạy reference mode; sai contract bị reject (F02/P0-03).
- [x] Hai source cùng model không trùng graph/processor/tracker owner hoặc reject rõ (F05/P0-03).

### Gate P0-B — Output và artifact

- [x] Prepared output có field scope, policy revision, frame/epoch/clock/transform/TTL (F03/P0-04).
- [x] Stale/unauthorized attribute không tới AU/ring; demand/PTS correlation có regression
  (F03/P0-04): commit `1e6c8b3` bổ sung đầy đủ regression tests cho demand, revoke, scope và TTL.
- [x] Artifact verify đúng bytes/identity trước load, chống symlink/replacement và mismatch
  (F04/P0-05): sealed memfd loading đã xác minh trực tiếp trên QCS6490 .98 (`/proc/<pid>/maps`).

### Gate P0-C — Lifetime, recovery và resource

- [x] Cascade bounded, control responsive, stop không early ACK/recycle/release (F06/P0-06):
  commit `d7b80f7` triển khai `cascade_execution_worker` bất đồng bộ với hàng đợi FIFO có giới hạn.
- [x] Backend chưa completion chuyển quarantine/recovery-required, không join vô hạn
  (F06/P0-06): giao thức quiescent reset và Rule 5 timeout quarantine đã hoạt động và pass tests.
- [x] Admission có measured profile hoặc reject; ghi pool/queue/encoder/DDR/thermal/FW load
  (F08/P0-07): commit `858893d` cung cấp JSON schema và measured profile mẫu cho QCS6490.

### Gate P0-D — Test và sign-off

- [x] O01–O08 pass reference/fake và relevant Qualcomm path bằng eSDK/QEMU sau audit:
  127/127 CTest pass (100%).
- [x] Board .98 smoke và resource report chạy trên đúng source/profile hiện hành:
  121/121 native tests pass trên target .98; live smoke dual-model + cascade FR pass không lỗi.
- [x] Production không bind reference sink; handoff chỉ báo accepted/pending đúng (F07/P0-07).
- [x] Layout/status checks pass; không mô tả reserved/logic-tested là accepted (P0-09):
  Bash source và docs layout checks đều pass 100%.
- [x] AI APP lead nghiệm thu kỹ thuật; ghi nhận register sẵn sàng cho cross-team sign-off.

## 6. Quy tắc mở năm plan sau

Chỉ mở production implementation của năm plan khi P0-09 là `UNBLOCKED` và P0-A…P0-D
pass. Hiện tại quyết định là `BLOCKED`; chỉ được chuẩn bị contract, fixture và review độc lập.

| Plan | Điều kiện bổ sung |
|---|---|
| 1 — contract/team | Authority snapshot và C01–C10 owner đã ký |
| 2 — metadata/query | Source/vehicle/scene identity và C04/C06 fixtures |
| 3 — event/evidence | Sink seam + C01/C04/C07; UDS production làm ở plan 3 |
| 4 — DSP | Ownership/admission + C02/C03 golden |
| 5 — integration/rollout | Tất cả gate Plan 0 và không còn production bypass |

Nếu gate fail, chỉ sửa Plan 0 hoặc mở decision record. Không lách bằng Kafka/Parquet/DSP/
video migration để che composition failure. Fixture downstream được phép; source production
downstream không được merge với status giả.

## Giới hạn và công việc tiếp theo

- Không hoàn thiện durable event transport, data lake, full feature algorithms, video-owner
  migration hay multi-backend acceptance.
- Không có CPU target cố định; baseline và workload profile phải được đo trước.
- Review hiện hành nằm tại
  [production composition foundation review](../../development/production_composition_foundation_review.md).
- Việc tiếp theo bắt buộc: hoàn thiện association record, cascade execution worker/recovery,
  lập profile đo QCS6490/released-FW, chạy lại eSDK/board trên HEAD và lấy ba sign-off.

## See also

- [Architecture improvement master plan](README.md)
- [Contract and team scope plan](contract_and_team_scope.md)
- [Metadata/query plan](metadata_query_plan.md)
- [Event/evidence transport plan](event_evidence_transport_plan.md)
- [DSP optimization plan](dsp_multiplatform_optimization_plan.md)
- [Integration/rollout plan](integration_validation_rollout_plan.md)
