# Phân phối và quản lý ứng dụng usecase

Kế hoạch này định nghĩa cách đóng gói mỗi usecase thành một application/SKU độc lập, chỉ cho
thiết bị được cấp entitlement tải package và chỉ cho application đã cài đặt tham gia điều khiển
bật/tắt. Thiết kế giữ một LACAI runtime dùng chung để chia sẻ camera, model và accelerator.

**Status:** planned — đã đối chiếu contract và source hiện tại; chưa có package manager,
signed entitlement provider hay install inventory production. **Layer:** docs. **Source:**
`include/vqec/vision/ai/contracts/vqec_vision_usecase_activation.hpp`,
`src/runtime/feature_manager/vqec_vision_usecase_control_manager.cpp`,
`config/schemas/usecase_control_snapshot.schema.json`.

## Trách nhiệm

- Chốt ý nghĩa của “một usecase là một app” ở product, package và runtime.
- Tách authority của catalog, entitlement, installation, desired state, compatibility,
  admission và running readiness.
- Định nghĩa download/install/update/rollback/uninstall an toàn, có thể khôi phục sau mất điện.
- Cho phép nhiều app dùng chung model/feature mà không tải, load hoặc tính toán trùng lặp.
- Không biến app store, UI hoặc một file JSON tự khai báo thành nguồn cấp quyền.

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
Product UI / App Store
        |
        v
BSP+FW App Manager ---- signed entitlement / repository / install inventory
        |                                      |
        | committed inventory revision         | immutable package digests
        v                                      v
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
cung cấp. AI APP phát hành logical usecase bundle; BSP+FW App Manager chịu trách nhiệm transport,
signature, content store và atomic installation.

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

App manifest tham chiếu dependency immutable. Package manager lưu mỗi blob một lần; AI runtime
giữ một graph/context khi backend contract cho phép và reference-count theo effective app. Gỡ một
app không xóa dependency còn được app khác hoặc rollback generation tham chiếu.

## 2. Các nguồn authority độc lập

Không tiếp tục coi một startup snapshot chứa nhiều boolean là nguồn sự thật production. Source
hiện tại đã tách các gate về mặt logic, nhưng `usecase_control_snapshot.schema.json` vẫn nhận
`installed`, `entitled`, `supported`, `compatible` và `admitted` từ cùng document. Production phải
derive từng gate từ owner độc lập:

| State | Authority duy nhất | Không được suy ra từ |
|---|---|---|
| `catalog_available` | signed repository index do BSP+FW publish | UI list hoặc file upload |
| `entitled` | signed entitlement provider, scope device/customer/app/source/time | installed/model present |
| `download_allowed` | server authorization + entitlement hiện hành | URL biết trước hoặc UI hidden |
| `installed` | committed install receipt và complete verified dependency closure | directory/file tồn tại |
| `desired` | authenticated FW UI/control request | install hoặc entitlement |
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
stop/uninstall. Enable của app chưa cài phải bị backend từ chối với `not_installed`, kể cả khi gọi
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

### 3.2. BSP-owned distribution envelope và install inventory

C10 vẫn do BSP+FW sở hữu. Distribution envelope và inventory phải có:

- signed repository revision, package digest, size, channel và anti-rollback sequence;
- device/target compatibility và product trust-chain reference;
- bounded blob list; không cho path traversal, symlink, hard link, device node hoặc overwrite;
- install transaction ID, previous/current generation, state và stable reason;
- exact installed component digests, dependency references và rollback retention;
- fsync/atomic-commit evidence, disk quota và garbage-collection eligibility;
- package-manager signature/attestation trên complete inventory revision.

AI APP không tin một directory scan. Nó chỉ coi app installed sau khi verify complete inventory,
manifest signature/digest và dependency closure. D-Bus notification chỉ là hint; sau notification
AI APP đọc lại full immutable snapshot theo revision để chịu được lost/duplicate/reordered signal.

### 3.3. Entitlement snapshot

Entitlement v1 thuộc C05 semantic authority của AI APP; BSP+FW/backend là producer. Grant tối thiểu
chứa issuer/key, grant/revision, device/customer scope, `app_id`, source/attribute/output limits,
`not_before`, `expires_at`, offline/grace policy và signature.

Repository server chỉ trả metadata/download authorization cho app được entitlement. Device vẫn
verify lại grant và package; ẩn nút trên UI không phải security. Offline install dùng cùng signed
`.vqapp` và vẫn phải có offline grant phù hợp. Không có entitlement thì upload local cũng bị từ chối.

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

1. Device authenticate với platform bằng identity do BSP provision.
2. App Manager lấy signed repository snapshot và signed entitlement snapshot.
3. UI chỉ hiện `Install` cho app có entitlement và target phù hợp.
4. Server kiểm quyền trước khi cấp short-lived download authorization; client không tự ghép URL.
5. Blob tải vào bounded staging, hỗ trợ resume theo chunk digest và quota.
6. Entitlement bị revoke giữa download không làm app installed; verify/commit phải kiểm lại revision.

### 5.2. Install

1. Verify repository metadata, package signature, digest, size và anti-rollback.
2. Parse manifest với bounded strict validator; reject unknown required semantics.
3. Resolve complete dependency closure; mọi mandatory blob phải có và đúng digest.
4. AI APP preflight target/runtime/schema/processor/model compatibility; không load model.
5. BSP kiểm storage/quota/permissions; AI admission chỉ đánh giá khả năng hợp lệ, không hứa mọi tổ
   hợp app đã cài đều chạy đồng thời.
6. Fsync blobs và candidate receipt, rồi atomically publish một inventory revision đầy đủ.
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

Update luôn side-by-side: tải/verify candidate mới, giữ current generation và rollback set. Nếu app
đang chạy, AI APP dừng output/scheduling của dependency bị thay, drain, chuyển inventory generation,
prepare runtime candidate, health-check rồi commit. Nếu prepare/health thất bại, package manager trả
inventory về generation trước và AI APP reconcile lại. Không overwrite artifact đang mmap/load.

Breaking model/preprocess/ontology/data change phải có migration và rollback tương thích. Data
migration dùng journal/checkpoint; không chạy arbitrary package script.

### 5.5. Uninstall

1. Package manager yêu cầu app về desired false.
2. AI APP chặn output, drain và trả quiesce receipt.
3. Package manager commit inventory không còn app.
4. Chỉ blob có reference count bằng zero và không thuộc rollback set mới được GC.

Không xóa gallery, metadata, event/evidence hoặc audit khi uninstall nếu chưa có `purge_data` action
riêng, scope riêng và retention policy. Nếu không chứng minh quiescence, uninstall trả
`recovery_required` và không xóa active artifact.

## 6. Boundary API

Không đưa download/install vào frame-processing service. Boundary tối thiểu:

| Boundary | Owner | Semantics |
|---|---|---|
| Repository/catalog/download API | BSP+FW platform | authenticated list/download, quota, resume, traffic metrics |
| Package manager API | BSP+FW | install/update/uninstall/status, atomic inventory, rollback |
| Install inventory port | BSP+FW producer, AI APP consumer | complete revisioned snapshot + signed receipt; notification is hint |
| Entitlement port | BSP+FW producer, AI APP schema owner | signed complete grant snapshot, trusted time/revocation |
| Desired control port | AI APP; authenticated FW caller | desired-only CAS/idempotency; cannot set installed/entitled |
| Compatibility/preflight port | AI APP | manifest/dependency/runtime/target validation without activation |
| Runtime status port | AI APP | independent gates, generation, stable reason, health and attributed metrics |

Giữ D-Bus cho desired/config/status tần suất thấp. Package bytes không đi qua D-Bus; dùng downloader
và filesystem/content store của BSP. Install inventory change có thể notify bằng D-Bus/UDS nhưng
state được phục hồi từ full snapshot, không phụ thuộc signal đã nhận.

`GetCapabilities` tiếp tục nghĩa “runtime biết xử lý usecase nào”, không đổi thành “app nào đã cài”.
UI join repository entitlement + package inventory + AI runtime status theo stable `app_id`.

## 7. Scope ba team

### BSP+FW

- Sở hữu app-store UI/backend client, device authentication, downloader, staging/content store,
  product trust roots, trusted time, disk quota, install transaction, inventory, rollback và
  supervisor integration.
- Enforce entitlement ở repository/download boundary và phát signed entitlement/install receipts.
- Không tự khai báo AI compatibility, model semantics, resource admission hay running readiness.
- Không chạy package shell script, không truyền arbitrary artifact path hoặc ép kill AI service.

### AI APP

- Sở hữu stable app/usecase IDs, semantic app manifest, dependency resolver, compatibility,
  admission composition, desired control, runtime reconciliation, shared dependency lifetime,
  output authorization, status/reason và per-app attributed metrics.
- Build logical app bundle từ component đã accept và giao immutable digest/SBOM cho release pipeline.
- Không tự cấp entitlement, không tự ký BSP install receipt và không tải package trong hot path.

### AI Model

- Giao C03 model component với artifact, exact IO/preprocess/decode/ontology, resource envelope,
  golden/quality/known limits và rollback compatibility.
- Không tạo app entitlement, install state, source assignment hoặc UI toggle.
- Khi một model dùng chung nhiều app, semantic/digest identity phải bất biến; thay semantics là
  component version mới và chạy lại downstream acceptance.

## 8. Kế hoạch triển khai

| Task | Owner | Đầu ra | Tiêu chí hoàn thành |
|---|---|---|---|
| UAP-01 | AI APP lead + BSP lead | ADR app-as-SKU/shared-runtime và authority matrix | ký owner, non-goals, trust/rollback decision |
| UAP-02 | AI APP | `usecase_app_manifest` v1 schema, C++ neutral contract, valid/error fixtures | strict bounds/version/digest/dependency tests pass eSDK |
| UAP-03 | AI APP + BSP | entitlement snapshot v1 và verifier port | wrong device/customer/app/time/signature fail closed |
| UAP-04 | BSP+FW | repository/distribution envelope, content store và install inventory v1 | power-loss/disk-full/partial-download tests giữ current revision |
| UAP-05 | AI APP | pure dependency/compatibility resolver và preflight CLI/port | shared/missing/conflict/cycle/rollback cases deterministic |
| UAP-06 | AI APP | tách gate providers khỏi startup booleans; trusted install/entitlement ports | UI chỉ thay desired; forged gate field không ảnh hưởng state |
| UAP-07 | AI APP | runtime generation reconcile install/update/uninstall | drain đúng, no auto-start, shared dependency không unload sớm |
| UAP-08 | BSP+FW | Install/Update/Uninstall/List UI/API | direct API và UI có cùng authorization/state behavior |
| UAP-09 | AI APP + BSP | metrics, audit, traffic accounting, diagnostics | không gán giả shared CPU/RAM/crash; revision/digest trace được |
| UAP-10 | Cả ba | QCS6490 end-to-end/recovery/soak acceptance | đủ gate mục 9 và receipts C03/C05/C10 |

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
- [ ] `desired=true` cho app chưa installed bị backend từ chối; sửa UI không bypass được.
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

### Product và board

- [ ] Install/enable/disable/update/uninstall chạy trên `.98` với exact digests và audit revisions.
- [ ] Test ít nhất một cặp app chia sẻ detector và một app cascade; model load/unload khớp expectation.
- [ ] Long soak theo release workload không tăng RSS/FD/thread, không orphan staging/receipt và không
  làm giảm FPS/CPU gate đã ký ngoài budget mới được chấp thuận.
- [ ] Released-FW App Manager/launcher/UI và AI APP cùng pass receipt C05/C10; model component pass C03.

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
9. Legacy bundle/startup snapshot chỉ gỡ sau released-FW migration/rollback acceptance.

## Giới hạn và công việc tiếp theo

- Chưa chọn transport container vật lý cuối cùng (`tar.zst`, immutable image hay cơ chế BSP có sẵn);
  quyết định thuộc UAP-01/UAP-04 và không thay semantic manifest/content-addressed identity.
- Product trust algorithm, key rotation, trusted time, offline grace, server API và storage quota cần
  BSP/security owner chốt; AI APP không tự tạo crypto scheme.
- Dynamic third-party native plugin, multi-tenant device và cross-device license transfer nằm ngoài
  baseline này.
- Plan này không tuyên bố signed entitlement, app store hay atomic install đã source-delivered.

## See also

- [Contract và phạm vi ba team](contract_and_team_scope.md)
- [Integration contract registry](../../contracts/integration_contract_registry.md)
- [FW usecase activation](../../contracts/fw_usecase_control.md)
- [FW control and package boundary](../../contracts/fw_control.md)
- [Usecase activation](../../architecture/usecase_activation.md)
- [Model package registry](../../architecture/model_package_registry.md)
- [Artifact digest](../../architecture/artifact_digest.md)
- [Integration and rollout](integration_validation_rollout_plan.md)
