# Kế hoạch product slice khói/lửa và App Manager

Kế hoạch này chuyển `security.fire_smoke_detection` từ detector đang chạy thành usecase app
sản phẩm đầu tiên, đồng thời tách service main, xây App Manager dùng chung cho S01-S18 và đưa
event/evidence qua lát cắt đầu tiên của Plan 3. Đây là execution plan; các contract tổng quát
trong plan phân phối app, metadata và event/evidence vẫn là authority.

**Status:** board-smoke — AI-owned first-install S04 vertical slice, no-data-safe Qualcomm
activation, signed entitlement/configuration, metadata và reference-evidence đã pass trên board;
operation/content-store update/rollback, model-quality acceptance và released-FW evidence vẫn là
gate mở nên plan chưa đạt `accepted`. **Layer:** docs.
**Source:** `src/app/service/bootstrap/vqec_vision_service_main.cpp`,
`src/runtime/feature_manager/`, `manifests/models/yolo11n_fire_smoke/`,
`docs/planning/architecture_improvement/usecase_app_distribution_plan.md`,
`docs/planning/architecture_improvement/event_evidence_transport_plan.md`.

## Bằng chứng thực thi hiện tại

- Exact source `c01b748`; eSDK/QEMU expanded suite 162/162 pass ngày 2026-09-20.
- Service candidate
  `0f0f8d211df04ca0817a312b2ade8039664b21ac7bb0214f068f5f4f2ad73c39` và App Manager
  `acf4a14136b2982451e17ec560e3edf4c2b83aaa3948094d20c1196f6b94b095` chạy trên machine
  ID đã đăng ký. Chín focused S04/App Manager/metadata/evidence native tests pass.
- Fresh inventory chứng minh no app → signed install → signed entitlement → desired enable bằng
  snapshot revision 2 → 3 → 4. Service chạy trước App Manager và camera; `libQnnHtp.so` chưa map
  trước frame đầu tiên.
- Mười chu kỳ disable/enable cách nhau 5 giây pass; ring đều resume, FD 74 → 74, RSS
  210.628 → 228.340 KiB, dưới gate tăng 32 MiB.
- Full workload steady 5 phút đạt CPU trung bình 10,56% một core, RSS +8 KiB và 23 threads.
  RTSP H.264 1920×1080 đạt 30,124 FPS trong phép đo 8 giây.
- Startup 30 giây trung bình 13,36%, peak một mẫu 1 giây là 86%. `perf` quy peak cho
  `qnn_engine::prepare()`/`libQnnHtpPrepare.so::GraphPrepare` khi frame đầu tiên tới, không phải
  polling lúc thiếu media. Đây vẫn là cold-start budget mở cho hướng QNN context binary.
- Configuration revision 2 reconcile khi ring tiếp tục; AI reference evidence probe durable và
  completed, không retry/transport failure. Đây không phải natural S04 alarm hay released-FW
  acceptance.
- Contact sheet của compatibility fixture gần như tối hoàn toàn; chỉ cadence/codec/kích thước
  được chứng minh, không tuyên bố overlay fire/smoke accuracy.

Raw acceptance boundary và số đo nằm tại
[fire/smoke product slice validation](../../testing/fire_smoke_product_slice_validation.md).

## Quyết định đóng gate

| Phạm vi | Kết luận |
|---|---|
| M0 main/layer ownership | Đóng: `service_main` còn 8 dòng; bootstrap, generation, platform, feature, enrollment và output có owner riêng |
| S04 processor/config/metadata | Đóng ở mức logic-tested/board-smoke; real-sequence model quality chưa accepted |
| App Manager first install/control | Đóng ở mức board-smoke; là daemon/inventory/signature path thật, không phải mock |
| AI evidence transport | Đóng ở mức board-smoke với reference receiver |
| Package update/rollback | Mở: operation journal và content-addressed component generation chưa được triển khai |
| Qualcomm cold start | Mở: first-frame safety đạt, synchronous `GraphPrepare` peak chưa giảm |
| Released-FW evidence | Mở ngoài AI APP: chưa có receiver/media receipt C07 |

Vì ba dòng cuối chưa đạt, tài liệu này không được đổi thành `accepted` hoặc mô tả “đã đóng toàn
bộ”. Phần S04 first-install thuộc AI APP đã đủ để tiếp tục tích hợp; product distribution update
và released-FW acceptance phải có evidence riêng.

## Trách nhiệm

- Tạo thứ tự triển khai có dependency, output và gate nghiệm thu rõ để không làm big-bang.
- Giữ một runtime chia sẻ camera/model/accelerator; một usecase app là SKU và activation unit,
  không phải một process inference riêng.
- Đưa toàn bộ control/config/install authority vào AI-owned App Manager; backend chỉ là D-Bus
  peer theo contract AI APP, BSP/FW không sở hữu lifecycle app.
- Productize S04 bằng processor, cấu hình, event, metadata và evidence thật; không coi overlay
  detection là alarm nghiệp vụ.
- Không đổi hành vi production trong bước tách `service_main`; refactor phải có characterization
  gate riêng trước khi thêm chức năng.
- Không mô tả reference receiver là FW acceptance và không ngoại suy S04 thành 18-usecase
  capacity.

## 1. Kết quả audit và quyết định khóa trước khi code

### 1.1. Trạng thái source hiện tại

| Hạng mục | Bằng chứng hiện tại | Khoảng trống phải đóng |
|---|---|---|
| Service entrypoint | `service_main` 8 dòng; bootstrap/generation/platform/feature/enrollment/output đã tách owner | Generation controller còn lớn nhưng không còn là executable monolith |
| Feature config | Signed package/configuration CAS đi qua App Manager và typed factory | Model-quality bounds/receipt do AI Model chưa accepted |
| Fire/smoke model | QCS6490 package, generic DSP preprocess/decode và temporal S04 processor đã chạy | Real-sequence M0-M5 alarm quality receipt chưa có |
| Usecase identity | Registry dùng `security.fire_smoke_detection` | Fixture đang dùng `fire_smoke_detection` và không có `feature_ids` |
| Threshold | Decoder package đang có confidence `0.25`, IoU `0.45` | Đây là decode defaults cố định trong package; chưa có app configuration authority |
| Event output | Neutral dispatch, durable SQLite outbox, UDS v1, retry/revoke worker và receipt có source/test | Released-FW receiver/media receipt chưa có |
| Metadata | P2 projection nhận stable S04 episode/contribution và query Q08/Q15 | Natural event correlation trên real sequence chưa được board-accepted |
| App lifecycle | Ed25519 verifier, persistent inventory, D-Bus daemon, runtime snapshot và reconcile đã chạy board | Async journal/content store/update/rollback/catalog API còn mở |

### 1.2. Quyết định kiến trúc

1. App Manager là một AI APP control-plane service riêng, nhưng không nằm trong frame hot path.
   Nó expose `AppManager1` v1 trên system D-Bus cho backend và publish immutable control snapshot
   cho LACAI runtime qua adapter AI-owned.
2. Runtime không đọc trực tiếp database/content store của App Manager. Neutral ports nhận một
   snapshot đã commit gồm inventory, entitlement, desired state và config revision. D-Bus chỉ
   là adapter control-plane; core/runtime không phụ thuộc GIO.
3. Giữ shared LACAI runtime. S04 cài đặt một declarative `.vqapp`, không nạp native `.so` từ
   package và không chạy install script.
4. App Manager dùng transactional inventory riêng với metadata nghiệp vụ. Không gộp install,
   operation journal và entitlement vào P2 database vì owner, retention và failure domain khác.
5. Fire/smoke detector tạo candidate observations. Processor `fire_smoke_alarm` mới chịu trách
   nhiệm xác nhận theo thời gian/không gian, tạo incident, lifecycle event và evidence intent.
6. Event/evidence dùng generic envelope v1 ngay từ đầu nhưng chỉ enable schema payload S04 trong
   product slice đầu tiên. Usecase tiếp theo đăng ký payload/schema, không viết transport mới.
7. Mọi schema/wire LACAI bắt đầu version 1 và lấy numeric baseline từ version registry. Không tạo
   `v2` cho các contract mới trong baseline này.

Baseline chọn một private AI-owned `RuntimeControl1` trên system D-Bus cho App Manager -> runtime
vì đây là control ít tần suất. Runtime nhận signal chỉ như wake-up rồi gọi `GetSnapshot(revision)`
để lấy full bounded snapshot; nó không tái dựng authority bằng signal delta. Adapter implement
neutral runtime-control port, nên GIO không đi vào core/runtime. `UsecaseControl1` hiện tại chỉ là
seam migration và bị gỡ khỏi product backend sau khi conformance/rollback pass; backend chỉ gọi
`AppManager1`.

### 1.3. Điều kiện để “chỉ cập nhật model”

Một model fire/smoke mới được cập nhật chỉ bằng component package khi đồng thời thỏa:

- giữ cùng model role và semantic class IDs `smoke`/`fire`;
- IO/preprocess/decode vẫn khớp contract mà runtime đã support, hoặc dùng một decoder contract
  đã đăng ký sẵn;
- output coordinate, score semantics và candidate-quality envelope tương thích;
- có digest, resource envelope, golden/quality receipt và rollback compatibility;
- app manifest cho phép component version đó và admission candidate pass.

Thay tensor semantics, class ontology, decoder semantics hoặc feature input contract là breaking
change. Khi đó phải cập nhật compatibility declaration/acceptance; không được tráo artifact rồi
giữ nguyên version. “Không sửa C++” là mục tiêu cho compatible model update, không phải lời hứa cho
mọi model bất kỳ.

## 2. Kiến trúc đích của vertical slice

```text
Backend/UI
   | AppManager1 v1: catalog, install, configure, desired, operation/status
   v
AI APP App Manager
   |-- entitlement verifier
   |-- package staging + signature/digest verifier
   |-- content-addressed store + transactional inventory
   |-- revisioned app configuration + operation journal
   `-- immutable runtime-control snapshot
                    |
                    v
Shared LACAI runtime generation
   camera -> preprocess/DSP -> QNN fire/smoke -> decoder candidates
                                              -> fire_smoke_alarm processor
                                                   |-- event episode -> P2 metadata
                                                   `-- evidence intent -> durable outbox
                                                                        |
                                                                        v
                                                        UDS v1 -> FW evidence service
                                                                  -> receipts/media reference
```

App Manager có thể restart mà generation đang chạy không bị mất ngay. Runtime tiếp tục snapshot
đã commit gần nhất nhưng entitlement expiry/revocation deadline phải được enforce cục bộ. Runtime
restart lấy full snapshot theo revision, không rebuild state bằng chuỗi signal có thể mất hoặc đảo
thứ tự.

## 3. Phân lớp cấu hình và authority

Không gom mọi giá trị gọi là “threshold” vào một JSON. Mỗi nhóm có owner, khả năng thay đổi và
gate khác nhau.

| Nhóm | Ví dụ | Authority | Cách cập nhật |
|---|---|---|---|
| Model-locked | tensor IO, quantization, preprocess, class order, score semantics, decoder contract | AI Model C03 package; AI APP validate | Cập nhật model component side-by-side |
| Decoder operating envelope | candidate confidence floor, NMS method/IoU, max candidates | Model package khai báo default và allowed bounds | Chỉ override nếu decoder contract đánh dấu mutable; validate trước activation |
| App behavior | fire/smoke enable, alarm confidence, confirmation/clear time, update cadence, minimum area, zone rules | AI APP configuration schema; backend request qua App Manager | Config transaction tạo revision mới |
| Evidence policy | profile ref, pre/post duration, snapshot/clip intent, severity mapping | AI APP app/config policy, bị entitlement scope giới hạn | Config transaction + policy reconciliation |
| Deployment/resource | source assignment, model cadence, memory/compute budget, preview | AI APP deployment/admission | Generation reconciliation |

Nguyên tắc candidate floor:

- Decoder phải giữ đủ candidates để processor áp alarm threshold mà không rerun inference.
- Mỗi alarm threshold phải lớn hơn hoặc bằng candidate floor effective.
- Yêu cầu thấp hơn floor bị từ chối với stable reason, hoặc cần model/package revision mới.
- Giá trị `0.25`/`0.45` hiện tại chỉ là package defaults cần đưa qua quality gate; không sao chép
  thành hằng số trong processor, App Manager hoặc service main.

### 3.1. Fire/smoke configuration v1

Schema cấu hình S04 tối thiểu gồm các nhóm sau; giá trị default và bounds phải xuất phát từ app
manifest/model quality receipt, không được rải trong source.

| Nhóm | Field semantic |
|---|---|
| Identity | `app_id`, configuration schema, config revision, expected app/model compatibility |
| Classes | enable fire/smoke, per-class alarm confidence, minimum region area ratio |
| Temporal | confirmation duration/count, clear duration/count, bounded update interval |
| Spatial | scene/zone revision refs, include/exclude zones, candidate association rule |
| Incident | merge/split policy, maximum active incidents, source-gap and epoch disposition |
| Severity | deterministic mapping từ class/confidence/duration/zone sang severity |
| Evidence | profile ref, pre/post duration, snapshot/clip intent, retry deadline |
| Metadata | retention/access profile refs và aggregation dimensions được phép |

JSON chỉ được parse/validate trên cold path. Factory phải copy thành typed immutable config trong
processor. Hot path không parse JSON, không giữ borrowed payload và không đọc App Manager DB.

## 4. Tách service main trước khi thêm chức năng

### 4.1. Mục tiêu cấu trúc

`vqec_vision_service_main.cpp` chỉ còn signal registration, parse options, tạo top-level service
controller và map kết quả thành exit code. Mục tiêu review là không quá 250 dòng; đây là guard về
maintainability, không thay thế kiểm tra dependency/lifetime.

| Owner mới | Trách nhiệm duy nhất | Không được làm |
|---|---|---|
| `service_startup` | Load/cross-validate catalog, deployment, package, hardware và control snapshot | Không tạo thread hoặc tiến runtime |
| `service_generation_controller` | Reconcile revision, prepare/publish/drain một generation | Không biết GStreamer/QNN concrete type |
| `service_feature_activation` | Tạo request từ trusted authorities, bind config payload và output scopes | Không tự cấp installed/entitled/admitted |
| `service_output_runtime` | Sở hữu policy gate, metadata projection, event/outbox binding | Không chạy model hoặc camera loop |
| `service_execution_loop` | Tiến executor/cascade/control callbacks theo bounded cadence | Không load config/package |
| `service_shutdown` | Thứ tự stop, drain, receipt/metrics và exit decision | Không release owner trước completion |
| `cascade_runtime_owner` | Chuẩn bị/start/drain/stop graph/session cascade | Không nằm trong generic service bootstrap |

Tên/file ID cuối cùng phải được đăng ký trong naming registry trước khi thêm source. Có thể gộp hai
owner nhỏ nếu dependency graph chứng minh cùng một responsibility; không tạo một file `utils` hoặc
`common` để chuyển monolith sang vị trí khác.

### 4.2. Invariant refactor

- Không đổi CLI, default, wire/schema, feature behavior, output policy hoặc log/exit semantics.
- Không thêm App Manager/fire processor vào cùng commit di chuyển source.
- Owner lifetime giữ nguyên: graph/session/frame/tensor chỉ release sau completion/drain hiện hành.
- Không đưa Qualcomm/GIO/SQLite types vào neutral contracts/core/runtime.
- Mỗi extracted unit có test trực tiếp; top-level harness vẫn chạy cùng fixture trước/sau.
- Không để cyclic static-library dependency; service composition phụ thuộc port/interface hướng vào.

### 4.3. Gate M0

- `service_main` đạt mục tiêu kích thước và không còn business/composition algorithm.
- Characterization test bao phủ startup success/failure, generation rollover, feature authority,
  metadata required/optional, cascade drain, signal stop và exit decision.
- eSDK configure/build/CTest giữ nguyên pass count của baseline tại thời điểm bắt đầu.
- QEMU chạy các test logic mới; board chạy cùng S04 detection/preview workload 5 phút và không
  regression FPS/CPU/RSS/FD so với baseline có cùng commit/config.
- Chỉ sau M0 mới merge functional App Manager/S04 work vào branch tích hợp.

## 5. App Manager production baseline

### 5.1. Module và process

| Module | Layer/vai trò | Output chính |
|---|---|---|
| App contracts | `include/.../contracts` | manifest, entitlement, inventory, operation, config và status v1 |
| Package resolver | core/runtime cold path | strict validation, dependency closure, compatibility và anti-rollback |
| App Manager service | `src/app/management` | serialized operations, state machine và runtime reconcile |
| D-Bus adapter | adapter AI control | `AppManager1`, sender binding, bounded FD ingest, status signals |
| Inventory adapter | storage adapter | dedicated transactional DB/journal, recovery và read-only snapshots |
| Content store | storage adapter | immutable digest-addressed blobs, quota/reference/GC |
| Runtime control adapter | AI internal control | publish/get full immutable snapshot theo revision |
| Executable | minimal main | process bootstrap/supervision only |

App Manager database giữ catalog revision, grants, install receipts, desired/config revisions,
operations và audit. Model/app blobs nằm ở content store, không nhét vào database. Filesystem commit
dùng staging riêng, digest/size validation, fsync và atomic publish. Disk full hoặc power loss không
được làm hỏng current inventory.

### 5.2. API v1 bắt buộc

`AppManager1` phải có các nhóm method bất đồng bộ:

- capabilities/catalog: list/get app, compatibility và action availability;
- package: stage bằng read-only FD, install, update, rollback, uninstall;
- configuration: get schema/effective config, validate, apply bằng expected revision;
- control: enable/disable bằng expected revision;
- operations: get/cancel operation, list audit/status;
- metrics: attributed work và shared-cost indicator, không gán giả CPU/RAM process cho app.

Mutating call chỉ trả accepted `operation_id`; `accepted`, `installed`, `loaded` và `running` là
các state khác nhau. Duplicate idempotency key cùng payload trả cùng operation; cùng key khác
payload trả conflict. Wrong sender, stale revision, invalid entitlement hoặc app chưa installed
phải fail closed kể cả UI đã ẩn nút.

### 5.3. Fire/smoke `.vqapp` đầu tiên

Package declarative S04 chứa:

- manifest `app_id = security.fire_smoke_detection`;
- exact reference tới fire/smoke model component và semantic contract digest;
- feature processor contract/configuration schema digest;
- requested source/event/evidence/metadata scopes;
- resource envelope và supported target/runtime ABI;
- defaults/bounds, quality/golden receipt references và rollback predecessor;
- SBOM/provenance/known-limit metadata.

Feature processor binary vẫn được compile/review cùng LACAI runtime. Package không mang code native
tùy ý. Install luôn kết thúc ở `installed_disabled`; enable là transaction riêng.

FS-07 cũng phải chuẩn hóa repository fixtures/catalog cùng lúc:

- dùng canonical ID `security.fire_smoke_detection` ở mọi app/usecase association;
- thêm `fire_smoke_alarm` vào feature catalog và `feature_ids` của S04;
- bind dependency role tới exact fire/smoke model catalog đang load, không giữ catalog ref của
  person/face fixture;
- derive event schema/config từ committed app/feature snapshot, không yêu cầu operator truyền
  `--event-schema-id` để product path chạy đúng;
- giữ CLI/harness fixture chỉ cho development và đánh dấu rõ không phải production authority.

## 6. Product hóa fire/smoke usecase

### 6.1. Processor và incident state machine

Thêm feature package `fire_smoke_alarm` sau neutral factory/registry hiện có. Mỗi
source/class/spatial cluster có bounded state:

```text
idle -> candidate -> active -> clearing -> ended
                    |             |
                    `-> update* <-'
epoch/config/model/source gap -> interrupted
```

- `candidate` tích lũy bằng capture time và bounded evidence, không chỉ số vòng lặp CPU.
- `active` chỉ phát `started` một lần sau confirmation rule.
- `update` rate-limited, dùng cùng stable event ID và tăng revision.
- `ended` chỉ phát sau clear rule; mất source, epoch/config/model đổi tạo `interrupted` với reason.
- Spatial association dùng source-coordinate/scene revision rõ; không gộp box bằng pixel magic.
- Số candidate/incident có bound từ resource profile; overflow có metric/disposition.

### 6.2. Event schema v1

Event S04 tối thiểu chứa:

- stable event ID/revision và lifecycle kind;
- canonical app/usecase/feature/source/boot/epoch IDs;
- event/capture interval, UTC mapping uncertainty;
- fire/smoke class set, confidence summary và bounded region/hotspot;
- scene/zone, model, decoder, config, policy và entitlement revisions;
- severity, interruption/end reason và source-gap quality;
- evidence intent/request correlation, không chứa video bytes;
- access/output scopes đã được gate theo actual payload.

Không phát alarm mỗi detection frame. Raw/compacted region observations đi metadata lane theo P2;
incident event là semantic fact riêng phục vụ thống kê số vụ, vị trí và evidence.

### 6.3. Metadata projection S04

Trong cùng transactional ingest của semantic event:

- upsert một `event_episode` theo event ID/revision;
- append bounded contribution cho time/zone/class/severity rollup;
- ghi evidence outbox intent nếu policy cho phép;
- lưu model/config/scene/policy provenance;
- query Q08 trả incident timeline/detail và Q15 trả count/rate/hotspot theo interval/zone/class.

Retry cùng revision phải idempotent. Update/End out-of-order có deterministic disposition; late event
được watermark/quality flag xử lý, không âm thầm double count.

## 7. Event/evidence vertical slice cho S04

Plan 3 vẫn dùng generic UDS `SOCK_SEQPACKET` + durable outbox. Lát cắt này chỉ mở
`security.fire_smoke_detection` nhưng không hardcode transport theo model.

1. Khóa envelope/ACK/error/state v1 và S04 payload mapping.
2. Ghi episode/contribution/evidence command trong một transaction hoặc cơ chế atomic handoff đã
   chứng minh; `durable_local` chỉ có sau commit theo sync policy.
3. Worker ưu tiên alarm, bounded retry/backoff, kiểm entitlement/policy lại trước retry.
4. UDS adapter verify peer/UID/schema/source/epoch/size; duplicate request ID dedup.
5. AI-owned reference receiver kiểm framing, ACK, lost-ACK/restart và receipt trên eSDK/board.
6. FW released receiver trả `accepted/recording/ready/partial/failed` và actual media interval.
7. Metadata cập nhật evidence reference từ receipt, không tạo incident hoặc clip thứ hai.

Reference receiver đủ cho logic-tested/board-smoke của AI side, không đủ đóng C07 hay gọi evidence
production. Full acceptance cần BSP+FW released service và receipt theo Plan 3.

## 8. Work breakdown và dependency

### 8.1. Các milestone bắt buộc

| ID | Công việc | Đầu ra | Gate hoàn thành |
|---|---|---|---|
| FS-00 | Freeze baseline | Exact commit/config/model digests, 5-minute S04 FPS/CPU/RSS/FD, test count | Baseline tái lập được trên eSDK/QEMU/board |
| FS-01 | Characterize main | Tests cho startup/generation/output/cascade/shutdown | Test fail khi thay đổi semantics đã khóa |
| FS-02 | Tách main | Các owner mục 4 và CMake target một chiều | M0 pass; không functional change |
| FS-03 | ADR/contract app | Authority, process/IPC, manifest/config/inventory/operation v1 | Lead duyệt; fixtures strict/bounded |
| FS-04 | Persistent App Manager core | Resolver, store, inventory, journal, recovery, entitlement ports | Unit/fault tests power loss/disk full/restart pass |
| FS-05 | `AppManager1` | D-Bus daemon, peer auth, FD stage, async operations/status | Backend conformance + negative tests pass |
| FS-06 | Runtime snapshot reconcile | Trusted install/entitlement/config providers; generation apply | No startup booleans as production authority |
| FS-07 | S04 package/config | Canonical app manifest, schema/default/bounds và compatibility | Install thành `installed_disabled`; invalid config fail closed |
| FS-08 | S04 processor | Typed config, temporal/spatial incident state machine | Golden temporal/epoch/gap/overflow tests pass |
| FS-09 | S04 event/metadata | Event v1, Q08/Q15 projection/rollup và access rules | Idempotent replay/concurrent query tests pass |
| FS-10 | Evidence AI side | Outbox, UDS client, reference receiver, receipt reconcile | Lost ACK/restart/duplicate/revoke tests pass |
| FS-11 | Model update/rollback | Side-by-side compatible component update | Không sửa C++; health fail rollback generation cũ |
| FS-12 | Board vertical acceptance | Install/config/enable/event/query/evidence/disable/update/uninstall | Toàn bộ gate mục 9 có evidence |
| FS-13 | FW acceptance | Released FW receiver/media receipt | C07 owner ký; Plan 3 S04 slice accepted |

Dependency chính:

```text
FS-00 -> FS-01 -> FS-02 -> FS-03
                         |-> FS-04 -> FS-05 -> FS-06 --|
                         `-> FS-07 -> FS-08 -> FS-09 --+-> FS-12 -> FS-13
                                             FS-10 ---|
                                             FS-11 ---|
```

FS-04/05 và FS-07/08 có thể phát triển độc lập sau FS-03, nhưng chỉ tích hợp qua contract đã khóa.
Không đưa App Manager/fire logic vào branch chính trước M0.

### 8.2. Chuỗi commit khuyến nghị

1. Characterization tests và baseline evidence.
2. Mỗi extracted owner/CMake target là một commit refactor có tests.
3. ADR + schemas/contracts/fixtures App Manager.
4. Core resolver/inventory/store và fault tests.
5. D-Bus facade/daemon và conformance tests.
6. Runtime trusted snapshot/reconcile.
7. S04 manifest/config/factory/processor.
8. Event + metadata projection.
9. Outbox + UDS reference receiver.
10. End-to-end scripts, board evidence và status documentation.

Mỗi commit chỉ chứa task-owned files, chạy layout/naming checks và eSDK tests liên quan. Không gộp
refactor, schema breaking change và product behavior vào một commit khó rollback.

## 9. Ma trận kiểm thử và tiêu chí nghiệm thu

### 9.1. Contract, security và recovery

- [ ] Parser từ chối unknown required field, oversize/depth/count, bad version/digest/signature,
  path traversal, symlink/device node và wrong target.
- [ ] Wrong D-Bus sender/UID, stale revision, expired/revoked grant và app chưa installed không thể
  stage/install/configure/enable bằng direct call.
- [ ] Power loss ở mỗi install/update commit point phục hồi current inventory hoặc candidate rõ,
  không half-installed state.
- [ ] Disk full, FD close/change, App Manager/runtime restart, duplicate/reordered signal không mất
  committed operation hoặc tạo generation mơ hồ.
- [ ] Entitlement/config revision thay đổi chặn output đúng thời điểm và drain an toàn.

### 9.2. S04 functional và data

- [ ] Cùng golden sequence tạo cùng incident lifecycle/event revisions sau restart/replay.
- [ ] Thay alarm threshold/confirmation/clear/ROI/evidence policy qua App Manager có hiệu lực ở
  generation revision mới, không rebuild binary và không parse JSON trên hot path.
- [ ] Fire-only, smoke-only, đồng thời, flicker, overlap, source gap, epoch reset, config/model swap,
  overload và no-detection đều có expected disposition.
- [ ] Không alarm-per-frame; duplicate event revision không double count hoặc duplicate evidence.
- [ ] Q08 trả đúng incident/detail/provenance/evidence; Q15 trả count/rate/hotspot theo time/zone/class.
- [ ] Payload/access rule không lộ field ngoài entitlement; revoke chặn retry/output cũ.

### 9.3. App lifecycle

- [ ] Catalog -> entitlement -> stage -> install kết thúc `installed_disabled`.
- [ ] Enable chỉ khi installed/entitled/supported/compatible/admitted; accepted khác running.
- [ ] Configure dùng CAS revision; invalid config không đổi effective generation.
- [ ] Disable chặn output rồi drain; uninstall không xóa metadata nếu không có purge transaction.
- [ ] Compatible model update chỉ thay package/manifest reference, không sửa C++ và rollback được.
- [ ] Shared component reference count không unload/xóa blob còn generation/rollback khác dùng.
- [ ] App Manager nạp được catalog đủ S01-S18, hiển thị đúng unsupported/not-installed/locked state
  và không có nhánh logic hardcode S04 trong resolver, inventory, D-Bus hoặc reconciler.
- [ ] Hai fixture app dùng chung component chứng minh dependency/refcount generic; unknown app hoặc
  package tự khai capability bị từ chối theo registry authority.

### 9.4. Event/evidence

- [ ] Durable local, FW accepted, recording và ready/partial/failed quan sát riêng.
- [ ] Lost ACK/retry/restart với cùng request không tạo clip thứ hai.
- [ ] Wrong peer/schema/version/source/epoch/size bị reject có reason và metric.
- [ ] Actual evidence interval/gap/media ID được ghi lại và query correlation đúng.
- [ ] Alarm lane không bị metadata query/rollup/GC/App Manager operation làm starvation.

### 9.5. eSDK, board và resource

- [ ] Mọi C++ configure/build/test dùng approved eSDK; logic tests chạy QEMU khi phù hợp.
- [ ] Board identity đúng hồ sơ QCS6490; workspace `/opt/lacai`; runbook không chứa credentials.
- [ ] S04 standalone xử lý stream 30 FPS trong 5 phút, average process CPU không vượt 12% một
  logical core theo target đã thống nhất, trừ khi lead ký budget mới kèm A/B evidence.
- [ ] Full workload hiện hành giữ 30 FPS và không vượt gate 15% đã đo/được ký cho cùng profile;
  App Manager idle không tạo polling spike đáng kể.
- [ ] Startup CPU spike được đo riêng (peak, duration, work attribution) và có budget; không che
  bằng average 5 phút.
- [ ] RSS/FD/thread/allocation ổn định; install/update/config/event storm không leak, không orphan
  staging/outbox và không giữ DMA/tensor owner sau completion.
- [ ] Preview overlay xem được bằng VLC, event metadata khớp frame/capture time và evidence chạy
  độc lập viewer demand.

CPU được đo cùng source/model/cadence/preview/logging và cùng công cụ; không so `top` với normalized
per-core metric khác định nghĩa. 18-usecase target cần capacity plan riêng sau khi có resource
envelope của từng app; S04 pass không chứng minh 18 app đạt 80% core.

## 10. Definition of done

Product slice chỉ được đóng khi:

1. M0 refactor pass và `service_main` không còn monolith.
2. App Manager là persistent production path, không phải mock/fixture; backend conformance pass.
3. S04 có canonical manifest, typed config, temporal incident processor, event và P2 metadata/query.
4. Install/config/enable/disable/update/rollback/uninstall chạy end-to-end trên board.
5. AI-side outbox/UDS/recovery pass; evidence production chỉ được đánh accepted sau FS-13.
6. Model update tương thích được chứng minh không cần sửa source; breaking update bị reject rõ.
7. Functional, fault, security, CPU/FPS/startup/RSS/FD gates có raw evidence và exact revisions.
8. Capability/status docs chỉ nâng đúng mức `source-delivered`, `logic-tested`, `board-smoke` hoặc
   `accepted` tương ứng; không dùng một gate để suy ra gate khác.

## Giới hạn và công việc tiếp theo

- Kế hoạch chưa chọn primitive chữ ký/trust-store cuối cùng; FS-03 phải khóa bằng ADR và dùng
  primitive đã review, không tự thiết kế crypto.
- App Manager lifecycle thuộc AI APP; released FW chỉ tham gia evidence receiver/media ở FS-13.
- S05-S18 dùng lại App Manager/transport sau S04, nhưng mỗi app vẫn cần processor semantics,
  configuration schema, data/query/evidence mapping và quality receipt riêng.
- P2 metadata đã accepted không tự làm S04 product-ready; S04 projection chỉ accepted sau FS-09/12.

## See also

- [Architecture improvement master plan](README.md)
- [Usecase app distribution plan](usecase_app_distribution_plan.md)
- [Metadata and query plan](metadata_query_plan.md)
- [Event and evidence transport plan](event_evidence_transport_plan.md)
- [Contract and team scope](contract_and_team_scope.md)
- [System architecture](../../architecture/system_architecture.md)
- [Feature activation manager](../../architecture/feature_activation_manager.md)
- [Feature event dispatch](../../architecture/feature_event_dispatch.md)
