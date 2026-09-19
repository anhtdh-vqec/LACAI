# Phân phối và quản lý ứng dụng usecase

Kế hoạch này định nghĩa cách AI APP đóng gói và quản lý mỗi usecase thành một application/SKU độc
lập, chỉ cho thiết bị được cấp entitlement tải package và chỉ cho application đã cài đặt tham gia
điều khiển bật/tắt. Backend giao tiếp với AI-owned App Manager qua D-Bus v1; BSP/FW không tham gia
lifecycle usecase app. Thiết kế giữ một LACAI runtime dùng chung để chia sẻ camera, model và
accelerator.

**Status:** board-smoke — App Manager, D-Bus v1, declarative S04 manifest, Ed25519 package/grant
verification, persistent inventory và first-install/config/enable/disable path đã chạy board;
catalog/download, async journal, content store, update/rollback và backend conformance còn mở.
**Layer:** docs. **Source:**
`include/vqec/vision/ai/contracts/features/vqec_vision_usecase_activation.hpp`,
`src/runtime/feature_manager/vqec_vision_usecase_control_manager.cpp`,
`config/schemas/usecase_control_snapshot.schema.json`.

## Trách nhiệm

- Chốt ý nghĩa của “một usecase là một app” ở product, package và runtime.
- Tách authority của catalog, entitlement, installation, desired state, compatibility,
  admission và running readiness.
- Định nghĩa download/install/update/rollback/uninstall an toàn, có thể khôi phục sau mất điện.
- Cho phép nhiều app dùng chung model/feature mà không tải, load hoặc tính toán trùng lặp.
- Không biến app store, UI hoặc một file JSON tự khai báo thành nguồn cấp quyền.
- Không giao implementation, state authority hay acceptance của usecase App Manager cho BSP/FW.

## 1. Quyết định kiến trúc

### 1.1. Application là SKU và activation unit, không phải một process

Mỗi usecase thương mại, ví dụ `security.fire_smoke_detection` hoặc
`security.unauthorized_intrusion`, là một `app_id` ổn định và có package riêng. Khi cài app,
thiết bị nhận manifest, model/config/data cần thiết và install receipt. Khi bật app, LACAI tạo
runtime generation chỉ chứa dependency của các app đang effective.

Baseline dùng quan hệ một-một: `app_id == usecase_id` trong S01–S18. Display name/icon được localize
riêng và không phải identity. Một sales bundle có thể grant nhiều `app_id`, nhưng không gộp nhiều
usecase vào một install artifact vì sẽ làm mất independent install/update/revoke. Các ID hiện tại
như `person_detection` và `face_recognition` là engineering capability/profile, không phải app
thương mại cuối cùng: blacklist và attendance có thể cùng dùng face components nhưng vẫn là hai app
có rule, dữ liệu, quyền và output khác nhau.

Không chạy một daemon AI độc lập cho mỗi app ở baseline đầu tiên. Mười tám daemon sẽ mở lại cùng
camera, giữ nhiều bản sao detector/tracker/QNN context, cạnh tranh HTP/cDSP, nhân RAM và làm sai
ownership của RAW frame. Kiến trúc đích là:

```text
Backend / Product UI
        |
        | AI-owned D-Bus v1 contract; metadata/control + Unix FD transfer
        v
AI APP App Manager ---- private content store / install inventory / entitlement verifier
        |
        | committed app + desired + entitlement revisions
        v
AI APP gate reconciler ----------------> shared LACAI runtime
                                           |-- one camera acquisition per source
                                           |-- shared model graph by exact identity
                                           |-- per-usecase feature state
                                           `-- authorized output per app/usecase
```

Một app chỉ là một process riêng khi sau này có yêu cầu chạy code bên thứ ba không tin cậy.
Trường hợp đó phải dùng sandbox/out-of-process worker và neutral IPC riêng; không được nạp một
`.so` tùy ý vào tiến trình LACAI rồi gọi đó là isolation.

### 1.2. Package v1 là declarative và không có install script

Package v1 không phải `.exe`, không chứa `postinstall.sh` và không được phép chạy lệnh do package
cung cấp. AI APP phát hành logical usecase bundle và sở hữu App Manager, entitlement verification,
package ingest/staging, signature validation, private content store, atomic install và rollback. Backend
chỉ gọi D-Bus contract hoặc chuyển package bằng read-only Unix FD; backend không ghi trực tiếp vào
AI-owned store.

Bundle gồm một manifest đã ký và các blob immutable được định danh bằng SHA-256. File `.vqapp`
chỉ là container dùng để truyền online/offline; sau verify, payload được đưa vào content-addressed
store. Runtime chỉ đọc artifact qua package reference đã commit, không dùng path do UI gửi.

Trong baseline đầu tiên, feature processor là capability đã review trong LACAI runtime. App
package cung cấp manifest, model packages, ontology, rules và config. Cách này vẫn bảo đảm khách
hàng không được tải model/IP package nếu chưa có entitlement, nhưng không đưa native code chưa
review vào process. Dynamic native feature plugin là một contract/ADR khác.

### 1.3. Dependency được chia sẻ theo identity, không theo filename

Hai app có thể cùng dùng person detector, tracker hoặc face detector. Dependency chỉ được chia sẻ
khi tuple sau khớp chính xác:

```text
component_id + component_version + target_id + artifact_digest + semantic_contract_digest
```

App manifest tham chiếu dependency immutable. App Manager lưu mỗi blob một lần; AI runtime
giữ một graph/context khi backend contract cho phép và reference-count theo effective app. Gỡ một
app không xóa dependency còn được app khác hoặc rollback generation tham chiếu.

## 2. Các nguồn authority độc lập

Không tiếp tục coi một startup snapshot chứa nhiều boolean là nguồn sự thật production. Source
hiện tại đã tách các gate về mặt logic, nhưng `usecase_control_snapshot.schema.json` vẫn nhận
`installed`, `entitled`, `supported`, `compatible` và `admitted` từ cùng document. Production phải
derive từng gate từ owner độc lập:

| State | Authority duy nhất | Không được suy ra từ |
|---|---|---|
| `catalog_available` | AI APP App Manager verify AI-owned catalog schema/signature | UI list hoặc file upload |
| `entitled` | AI APP verifier trên signed backend grant, scope device/customer/app/source/time | installed/model present |
| `download_allowed` | AI APP policy từ entitlement hiện hành và authenticated backend peer | URL biết trước hoặc UI hidden |
| `installed` | AI-owned committed install receipt và complete verified dependency closure | directory/file tồn tại |
| `desired` | authenticated backend request qua AI-owned D-Bus | install hoặc entitlement |
| `supported` | AI APP compiled capability/processor registry | package tự khai báo |
| `compatible` | AI APP resolver đối chiếu target/runtime ABI/schema/dependency | cùng filename/version text |
| `admitted` | hardware/resource admission cho complete candidate | install thành công |
| `loaded` | runtime generation owner | accepted control command |
| `running` | source/model/feature readiness của published generation | loaded hoặc process alive |

Effective state vẫn giữ invariant:

```text
effective = installed AND entitled AND desired AND supported AND compatible AND admitted
```

`installed` là điều kiện bắt buộc để gửi `desired=true`. Disable được phép để hỗ trợ idempotent
stop/uninstall. Enable của app chưa cài phải bị App Manager từ chối với `not_installed`, kể cả khi gọi
thẳng API mà bỏ qua UI. Install luôn tạo `desired=false`; cài app không tự chạy camera/model.

Entitlement hết hạn hoặc bị revoke chặn output ngay, rồi stop/drain runtime. Desired intent có thể
được giữ để reconciliation tự chạy lại sau khi entitlement hợp lệ; UI phải hiển thị `locked`, không
được báo `running`. Nếu sản phẩm không muốn auto-resume thì thêm policy có version, không sửa ngầm
`desired` trong license handler.

## 3. Contract package và inventory

### 3.1. AI-owned usecase app manifest v1

Schema mới `usecase_app_manifest` bắt đầu ở version 1 và thuộc C05 semantic authority của AI APP.
Mỗi manifest bắt buộc có:

| Nhóm | Field bắt buộc |
|---|---|
| Identity | `app_id == usecase_id`, `app_version`, `usecase_version`, release sequence |
| Compatibility | exact runtime ABI/schema, target IDs, minimum platform capability set |
| Dependencies | typed component ID/version/target/digest/semantic-contract digest, required/optional |
| Models | exact C03 package references, role/dependency, accepted golden/quality receipt refs |
| Features | processor contract, configuration schema/digest, temporal/reset requirements |
| Permissions | requested source/attribute/output/evidence/query scopes; chỉ là request, không phải grant |
| Resources | resident/tensor/temporal/preview/storage/network bounds và declared workload |
| Data | data schema/migration ID, retention class, uninstall/purge policy |
| Supply chain | payload digest/size/media type, SBOM, provenance, known limits, rollback predecessor |

`app_version` là product release version, không phải schema version. Schema/ABI LACAI vẫn là
baseline version 1 theo canonical version registry. Mọi artifact reference là immutable; URL và
absolute deployment path không nằm trong semantic manifest.

### 3.2. AI-owned distribution envelope và install inventory

Usecase application distribution là subdomain của C05 do AI APP sở hữu, không phải C10 system
deployment. C10 vẫn chỉ áp dụng cho base LACAI runtime, OS launcher/image và system rollback bên
ngoài plan này. AI APP định nghĩa và implement distribution envelope, private store và inventory:

- signed repository revision, package digest, size, channel và anti-rollback sequence;
- device/target compatibility và product trust-chain reference;
- bounded blob list; không cho path traversal, symlink, hard link, device node hoặc overwrite;
- install transaction ID, previous/current generation, state và stable reason;
- exact installed component digests, dependency references và rollback retention;
- fsync/atomic-commit evidence, disk quota và garbage-collection eligibility;
- integrity-protected receipt trên complete inventory revision; chỉ gọi là signature/attestation khi
  khóa và primitive tương ứng thực sự được platform cung cấp và review.

Runtime không tin một directory scan hay backend assertion. Nó chỉ coi app installed sau khi
AI-owned App Manager verify complete inventory, manifest signature/digest và dependency closure.
App Manager publish full immutable snapshot theo revision để runtime chịu được
lost/duplicate/reordered D-Bus request hoặc process restart.

### 3.3. Entitlement snapshot

Entitlement v1 thuộc C05 authority của AI APP. Backend là producer bên ngoài và phải phát đúng
AI-owned schema; AI APP là verifier và quyết định accept/reject. Grant tối thiểu chứa issuer/key,
grant/revision, device/customer scope, `app_id`, source/attribute/output limits, `not_before`,
`expires_at`, offline/grace policy và signature.

Backend chỉ được request/stage package mà grant cho phép. AI APP vẫn verify lại grant và package;
ẩn nút trên UI không phải security. Offline install dùng cùng signed `.vqapp` và vẫn phải có offline
grant phù hợp. Không có entitlement thì package FD hoặc local upload cũng bị từ chối.

## 4. State machine hiển thị cho UI

| State | UI action | Runtime meaning |
|---|---|---|
| `not_entitled` | hidden hoặc locked | không cấp download token |
| `entitled_not_installed` | `Install` | chưa có install receipt |
| `downloading` | progress/cancel | blob ở staging, chưa installed |
| `verifying` | progress | signature/digest/schema/quota check |
| `installing` | progress | candidate inventory chưa commit |
| `installed_disabled` | `Play`, settings, uninstall | desired false, không load compute |
| `enabling` | stop/cancel theo policy | generation đang reconcile |
| `running` | `Stop`, settings | published generation ready |
| `disabling` | locked | output blocked, work đang drain |
| `locked` | renew/view reason | installed nhưng entitlement không hợp lệ |
| `incompatible` | update/remove | target/runtime/dependency mismatch |
| `resource_limited` | stop app khác/retry | package hợp lệ nhưng candidate không admitted |
| `faulted` | retry/rollback/diagnostics | runtime generation không publish được |
| `update_available` | update | current app vẫn giữ nguyên đến atomic commit |
| `uninstalling` | locked | disable/drain trước khi bỏ receipt |

Các cột CPU/RAM/network của screenshot không được lấy từ `/proc` rồi gán giả cho từng app vì mọi
app dùng chung process. AI APP cung cấp attributed metrics theo model submissions, feature work,
pool ownership và shared-cost policy. UI phải đánh dấu `shared/estimated` nếu chi phí không thể tách
chính xác. Process crash/kill là runtime event; chỉ gắn fault cho app khi có correlation generation.

## 5. Workflow bắt buộc

### 5.1. Discover và download

1. AI APP App Manager bind configured backend well-known name sang unique D-Bus sender và từ chối
   peer khác; caller-supplied customer/device ID không phải authentication.
2. Backend provision signed catalog và entitlement snapshot theo AI-owned schema/revision.
3. App Manager verify grant rồi trả danh sách app/action cho backend UI qua D-Bus.
4. Backend gọi `StagePackage` rồi `Install`; package data đi bằng read-only Unix FD hoặc descriptor tải đã
   được contract cho phép, không đi bằng D-Bus byte array lớn.
5. App Manager tải/copy blob vào bounded private staging, kiểm size/chunk digest và quota.
6. Entitlement bị revoke giữa download không làm app installed; verify/commit kiểm lại revision.

### 5.2. Install

1. Verify repository metadata, package signature, digest, size và anti-rollback.
2. Parse manifest với bounded strict validator; reject unknown required semantics.
3. Resolve complete dependency closure; mọi mandatory blob phải có và đúng digest.
4. AI APP preflight target/runtime/schema/processor/model compatibility; không load model.
5. AI APP kiểm private storage/quota/permissions; admission chỉ đánh giá khả năng hợp lệ, không hứa
   mọi tổ hợp app đã cài đều chạy đồng thời.
6. App Manager fsync blobs và candidate receipt, rồi atomically publish một inventory revision đầy đủ.
7. AI APP re-read inventory, derive `installed=true`, publish status `installed_disabled`.

Power loss trước bước 6 để lại orphan staging có thể GC, không đổi current inventory. Power loss sau
commit phải reconstruct đúng revision khi reboot. Disk full giữ nguyên generation đang chạy.

### 5.3. Enable và disable

Enable đi qua `ApplyDesiredPlan`, nhưng backend phải kiểm app installed từ trusted inventory. Sau đó
reconciler lấy entitlement, capability, compatibility và admission mới nhất, tạo dependency closure,
stop/drain generation cũ, chuẩn bị candidate và chỉ publish khi toàn bộ owner ready.

Disable chặn scheduling/output của app, drain work và release dependency chỉ khi không còn effective
consumer. Shared detector/tracker/context không reset nếu app khác vẫn dùng. Tắt app không uninstall
package và không xóa dữ liệu nghiệp vụ.

### 5.4. Update và rollback

Update luôn side-by-side: App Manager tải/verify candidate mới, giữ current generation và rollback
set. Nếu app đang chạy, AI APP dừng output/scheduling của dependency bị thay, drain, chuyển inventory
generation, prepare runtime candidate, health-check rồi commit. Nếu prepare/health thất bại, App
Manager trả inventory về generation trước và runtime reconcile lại. Không overwrite artifact đang
mmap/load.

Breaking model/preprocess/ontology/data change phải có migration và rollback tương thích. Data
migration dùng journal/checkpoint; không chạy arbitrary package script.

### 5.5. Uninstall

1. App Manager áp desired false trong cùng serialized control transaction.
2. AI APP chặn output, drain và trả quiesce receipt.
3. App Manager commit inventory không còn app.
4. Chỉ blob có reference count bằng zero và không thuộc rollback set mới được GC.

Không xóa gallery, metadata, event/evidence hoặc audit khi uninstall nếu chưa có `purge_data` action
riêng, scope riêng và retention policy. Nếu không chứng minh quiescence, uninstall trả
`recovery_required` và không xóa active artifact.

## 6. Boundary API

Không đưa download/install vào frame-processing service. AI APP cung cấp một facade duy nhất
`com.vqec.AiVision.AppManager1`; backend là authenticated peer và không được gọi trực tiếp runtime
manager, sửa inventory hay ghi vào private content store.

Baseline triển khai App Manager thành service/process AI-owned tách khỏi realtime LACAI runtime,
nhưng phát hành trong cùng product boundary. App Manager chịu blocking I/O, crypto, fsync, recovery
và quota; runtime chỉ nhận immutable committed snapshot/control qua neutral internal port. App
Manager crash hoặc disk-full không được giữ raw frame, tensor hay hardware completion owner.

| Method/boundary | Authority | Semantics bắt buộc |
|---|---|---|
| `ApplyBackendSnapshot` | AI APP schema; backend producer | complete signed catalog/entitlement snapshot, monotonic revision, CAS/idempotency |
| `StagePackage` | AI APP | nhận read-only Unix FD + declared size/digest; bounded copy, verify rồi đóng staging transaction |
| `Install` / `Update` | AI APP | preflight dependency/target/quota, atomic generation, health check, commit hoặc rollback |
| `Uninstall` | AI APP | disable, block output, drain, commit inventory, reference-counted GC; purge là action riêng |
| `ApplyDesiredPlan` | AI APP; backend caller | desired-only complete snapshot; không được đặt installed/entitled/supported/admitted |
| `ListApplications` | AI APP | authoritative UI projection của catalog, entitlement, inventory và runtime gates |
| `GetOperationStatus` | AI APP | durable operation/revision/generation, stable reason và progress có giới hạn |
| `CancelOperation` | AI APP | best-effort trước commit point; không biến cancellation thành hardware completion |

D-Bus chỉ mang metadata/control bounded. Package hoặc snapshot lớn không đi bằng byte array; backend
truyền Unix FD (`h`) với size/digest đã khai báo, App Manager sao chép có giới hạn vào staging riêng,
verify signature/digest rồi mới publish. FD close, peer disconnect hoặc timeout không đồng nghĩa copy,
verify, inference hay drain đã hoàn tất.

AI APP bind configured backend well-known name sang unique sender và từ chối sender khác. Request có
bounded idempotency key, expected revision và payload digest; retry cùng payload trả kết quả cũ, reuse
key với payload khác bị từ chối. Signal chỉ là hint; sau reconnect backend gọi status/full snapshot để
phục hồi, không suy trạng thái từ signal có thể mất.

Production dùng system bus policy: chỉ service UID đã cấu hình được own backend well-known name và
gọi `AppManager1`; App Manager theo dõi `NameOwnerChanged` và rebind unique owner. UID/name chỉ xác
thực local peer, còn catalog/grant/package vẫn phải verify chữ ký và scope. Session bus không phải
production default.

Mutating method chỉ validate/enqueue rồi trả `operation_id`, accepted revision và stable reason; D-Bus
handler không chờ download, fsync, model prepare hay hardware drain. `GetOperationStatus` báo một trong
`queued`, `staging`, `verifying`, `installing`, `reconciling`, `committed`, `rolled_back`, `cancelled`,
`failed`, `recovery_required`. `accepted` không có nghĩa `installed` hoặc `running`.

Với package FD, App Manager `fstat`, giới hạn loại/size, sao chép đúng declared byte count vào file
private mới, tính digest trong lúc copy, fsync rồi mới verify/publish. Memfd nên có write/grow/shrink
seals; regular file vẫn luôn được copy và verify để không tin tính immutable của peer. Không nhận path
từ backend và không mmap trực tiếp FD của peer làm runtime artifact.

`GetCapabilities` tiếp tục nghĩa “runtime biết xử lý usecase nào”, không đổi thành “app nào đã cài”.
Backend/UI dùng `ListApplications` làm product view theo stable `app_id`; không tự join file/DB nội bộ.

## 7. Phạm vi owner và peer

### AI APP

- Sở hữu App Manager, D-Bus v1, schema catalog/entitlement/package, verifier, package ingest/staging,
  private content store, quota, install inventory/journal, update/rollback/uninstall/recovery,
  dependency resolver, desired/runtime reconciliation, audit và metrics.
- Là authority duy nhất của installed/compatible/supported/admitted/loaded/running và install receipt.
- Bind backend identity từ transport; không tin `customer_id`, role hay entitlement boolean trong
  payload nếu không có authenticated/signed authority tương ứng.
- Không chạy arbitrary package script, không nhận arbitrary deployment path và không ghi đè artifact
  đang load. Hot path frame/inference không thực hiện download, verify hoặc filesystem transaction.

### Backend

- Là external producer/client, không phải state authority bên trong device. Backend phải theo D-Bus,
  schema, bounds, revision, error taxonomy và conformance fixtures do AI APP phát hành.
- Cấp signed catalog/entitlement/package metadata và package FD; gọi install/update/uninstall/desired;
  hiển thị đúng status/reason do App Manager trả về.
- Không được ghi private store/inventory, tự đặt installed/running, bypass entitlement, suy readiness từ
  method return hoặc yêu cầu runtime load artifact path do backend chọn.
- Nếu backend nối cloud, backend/local gateway tự bridge remote protocol sang local D-Bus; D-Bus không
  phải giao thức Internet và AI APP không phụ thuộc cloud trong frame-processing path.

### AI Model

- Giao C03 model component với artifact, exact IO/preprocess/decode/ontology, resource envelope,
  golden/quality/known limits và rollback compatibility.
- Không tạo app entitlement, install state, source assignment hoặc UI toggle.
- Khi một model dùng chung nhiều app, semantic/digest identity phải bất biến; thay semantics là
  component version mới và chạy lại downstream acceptance.

### BSP+FW

- Không có implementation task, state authority hay acceptance sign-off trong vòng đời usecase app.
- C10 chỉ áp dụng cho base OS/system image, LACAI runtime service, UID/volume/supervision và OTA của
  nền tảng; C10 không được dùng để giành quyền App Manager, usecase inventory hay private app store.
- Device/media/accelerator/evidence boundaries khác vẫn theo C01/C02/C07/C08/C10 tương ứng và nằm
  ngoài plan phân phối usecase app này.

## 8. Kế hoạch triển khai

Lát cắt triển khai đầu tiên là `security.fire_smoke_detection`. Trình tự tách service main,
đưa configuration payload vào processor, xây App Manager production và kiểm thử end-to-end
được khóa tại [kế hoạch product slice khói/lửa](fire_smoke_product_slice_plan.md). UAP-01–UAP-10
vẫn là contract tổng quát cho S01-S18; việc S04 pass không được dùng để bỏ các gate supply-chain,
recovery hoặc shared-dependency bên dưới.

| Task | Owner | Đầu ra | Tiêu chí hoàn thành |
|---|---|---|---|
| UAP-01 | AI APP lead | ADR app-as-SKU/shared-runtime, D-Bus facade, authority matrix và backend conformance profile | AI APP khóa owner, non-goals, peer auth, trust/rollback decision; backend không có quyền đổi semantics |
| UAP-02 | AI APP | `usecase_app_manifest` v1 schema, C++ neutral contract, valid/error fixtures | strict bounds/version/digest/dependency tests pass eSDK |
| UAP-03 | AI APP | entitlement snapshot v1, verifier, system-bus policy và D-Bus peer authentication | wrong sender/UID/device/customer/app/time/signature fail closed; name-owner change rebind đúng |
| UAP-04 | AI APP | distribution envelope, FD staging, private content store, journal và install inventory v1 | power-loss/disk-full/partial-download/FD-close giữ current revision |
| UAP-05 | AI APP | pure dependency/compatibility resolver và preflight CLI/port | shared/missing/conflict/cycle/rollback cases deterministic |
| UAP-06 | AI APP | tách gate providers khỏi startup booleans; trusted install/entitlement ports | UI chỉ thay desired; forged gate field không ảnh hưởng state |
| UAP-07 | AI APP | runtime generation reconcile install/update/uninstall | drain đúng, no auto-start, shared dependency không unload sớm |
| UAP-08 | AI APP | `AppManager1` Install/Update/Uninstall/List/status API và backend conformance fixtures | direct D-Bus và UI có cùng authorization/state behavior |
| UAP-09 | AI APP | per-app metrics, audit, traffic accounting và diagnostics | không gán giả shared CPU/RAM/crash; revision/digest trace được |
| UAP-10 | AI APP lead | QCS6490 end-to-end/recovery/soak acceptance với C03 và backend peer | đủ gate mục 9, AI APP C05 receipt, AI Model C03 receipt và backend conformance report |

Thứ tự bắt buộc: UAP-01 → UAP-02/03/04 → UAP-05 → UAP-06/07 → UAP-08/09 → UAP-10.
UAP-02/03/04 có thể làm song song sau khi ADR khóa boundary. Không bắt đầu UI production bằng API
giả trước khi authority/state machine được duyệt.

## 9. Tiêu chí nghiệm thu

### Authorization và supply chain

- [ ] App không entitlement không xuất hiện như installable và direct download bị từ chối.
- [ ] Offline upload không có grant/signature phù hợp bị từ chối; UI hidden không phải gate duy nhất.
- [ ] Tamper manifest/blob, wrong target/runtime ABI, expired/revoked grant và rollback sequence cũ
  đều fail closed với stable reason.
- [ ] Archive traversal/symlink/device-node/oversize/bomb không ghi ra ngoài staging hoặc vượt quota.
- [ ] Exact manifest, SBOM, model/golden receipt và install inventory digest truy vết được.

### Install và control

- [ ] Cài app thành công tạo `installed_disabled`; không mở camera/model và không tự bật.
- [ ] `desired=true` cho app chưa installed bị App Manager từ chối; sửa UI không bypass được.
- [ ] Installed, entitled nhưng incompatible/resource-limited không chạy và có reason đúng.
- [ ] Command accepted, loaded và running vẫn là ba trạng thái khác nhau.
- [ ] Reboot phục hồi inventory/desired policy đúng; không mặc định bật toàn bộ app đã cài.

### Shared dependency và lifecycle

- [ ] Hai app dùng cùng exact dependency chỉ lưu/load một lần khi contract cho phép.
- [ ] Disable/uninstall một app không unload/reset dependency app khác còn dùng.
- [ ] Revocation trong inference/retry chặn output ngay rồi drain; buffer chỉ release sau completion.
- [ ] Update side-by-side không overwrite loaded artifact; failed health-check rollback generation cũ.
- [ ] Power loss ở mọi bước, disk full, duplicate/reordered notification và process restart không tạo
  half-installed state hoặc mất current generation.
- [ ] Uninstall không xóa dữ liệu người dùng nếu không có authorized purge transaction riêng.
- [ ] Wrong D-Bus sender, stale revision, reused idempotency key khác payload, FD size/digest mismatch,
  oversized byte array và disconnect đều fail closed; reconnect phục hồi bằng authoritative status.

### Product và board

- [ ] Install/enable/disable/update/uninstall chạy trên QCS6490 được xác thực theo board record,
  với exact digests và audit revisions.
- [ ] Test ít nhất một cặp app chia sẻ detector và một app cascade; model load/unload khớp expectation.
- [ ] Long soak theo release workload không tăng RSS/FD/thread, không orphan staging/receipt và không
  làm giảm FPS/CPU gate đã ký ngoài budget mới được chấp thuận.
- [ ] AI-owned `AppManager1` và backend pass cùng bộ C05 conformance; model component pass C03.

## 10. Migration từ source hiện tại

1. Giữ resolver và D-Bus desired-plan hiện có; không viết control plane thứ hai.
2. Giữ `person_detection`/`face_recognition` hiện tại như internal development profiles; product app
   catalog dùng đúng stable S01–S18, không quảng bá model/feature ID thành SKU.
3. Đánh dấu startup snapshot hiện tại là fixture/bootstrap input, không là production authority.
4. Thêm read-only `install_inventory_port` và `entitlement_port`; compose requests từ từng provider.
5. Chuyển `installed` từ JSON boolean sang verified install receipt; chuyển `entitled` sang signed grant.
6. Derive `supported`, `compatible`, `admitted` từ registry/resolver/profile thay vì trusted boolean.
7. Cho manager reject enable khi installed false nhưng vẫn giữ independent status fields.
8. Sau khi hot inventory reconciliation và reboot recovery pass, mới nối Install/Update/Uninstall UI.
9. `UsecaseControl1` hiện tại trở thành internal compatibility/control seam; backend product chỉ gọi
   `AppManager1`, không điều khiển đồng thời cả hai facade.
10. Legacy bundle/startup snapshot chỉ gỡ sau backend D-Bus v1 conformance và rollback acceptance.

## Giới hạn và công việc tiếp theo

- Chưa chọn transport container vật lý cuối cùng (`tar.zst` hay immutable image do AI APP quản lý);
  quyết định thuộc UAP-01/UAP-04 và không thay semantic manifest/content-addressed identity.
- Product trust algorithm, key rotation, trusted time, offline grace, server API và storage quota cần
  AI APP security design khóa trong contract với backend; không tự tạo crypto scheme mà chọn primitive
  đã review và giữ key material ngoài log/package.
- Dynamic third-party native plugin, multi-tenant device và cross-device license transfer nằm ngoài
  baseline này.
- Plan này không tuyên bố signed entitlement, app store hay atomic install đã source-delivered.

## See also

- [Fire/smoke product slice](fire_smoke_product_slice_plan.md)
- [Contract và phạm vi ba team](contract_and_team_scope.md)
- [Integration contract registry](../../contracts/integration_contract_registry.md)
- [Current usecase activation seam](../../contracts/fw_usecase_control.md)
- [System deployment and control boundaries](../../contracts/fw_control.md)
- [Usecase activation](../../architecture/usecase_activation.md)
- [Model package registry](../../architecture/model_package_registry.md)
- [Artifact digest](../../architecture/artifact_digest.md)
- [Integration and rollout](integration_validation_rollout_plan.md)
