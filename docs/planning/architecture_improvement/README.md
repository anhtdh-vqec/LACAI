# Đánh giá kiến trúc và kế hoạch cải tiến AI APP

Tài liệu đầu mối cho đợt cải tiến LACAI: tổng hợp đánh giá kiến trúc hiện tại, thiết kế
metadata/trajectory hai đường local và Kafka, giao tiếp evidence với FW, phân chia phạm vi
AI/FW, và hướng giảm tải ARM bằng DSP nhưng giữ khả năng thay nền tảng.
Ngày rà soát: 2026-09-20. Đây là tài liệu điều phối triển khai và review; trạng thái của từng
plan chỉ được xác lập theo gate và bằng chứng riêng của plan đó.

- **Status:** board-smoke — Plan 0, Plan 2 và phạm vi AI APP của Plan 4 đã qua gate riêng;
  product slice S04/App Manager đã qua board-smoke nhưng chưa đạt acceptance toàn bộ.
- **Layer:** docs
- **Naming registry:** không áp dụng; thư mục kế hoạch, không khai báo source mới.
- **Depends on:** hợp đồng LACAI hiện tại, source AI APP và source FW tham chiếu.
- **Used by:** ba team BSP+FW / AI APP / AI Model; AI APP lead điều phối kiến trúc tích hợp.

## Trách nhiệm

- Phân biệt rõ điều đã thấy trong source, số liệu lịch sử, suy luận và thiết kế tương lai.
- Đề xuất ranh giới có thể kiểm thử, chủ sở hữu, thứ tự thực hiện và tiêu chí nghiệm thu.
- Không thay đổi ABI, quyền sở hữu encode, entitlement hoặc quy tắc DMA bằng tài liệu này.
- Không thay thế hồ sơ acceptance; không khẳng định đã đạt CPU 15–20%, zero-copy hay
  tương thích FW phát hành khi chưa đo đúng workload.
- Không sao chép source/SDK/model từ repository FW vào LACAI.

## Nội dung

README này chứa toàn bộ phân tích; không cần đọc thêm một báo cáo thứ hai để biết đề xuất.

| Phần | Nội dung |
|---|---|
| 1 | Kết luận và quyết định kiến trúc nên chọn |
| 2 | Phạm vi, phiên bản và độ tin cậy của bằng chứng |
| 3 | Điểm tốt cần giữ và điểm yếu tổng thể |
| 4 | Các khoảng trống cụ thể trong source hiện tại |
| 5 | Phạm vi ba team, contract bàn giao và quyền quyết định |
| 6 | Dữ liệu và nghiệp vụ truy vấn chung cho security/traffic; lựa chọn storage theo workload |
| 7 | Kafka và đường dữ liệu trung tâm |
| 8 | Event IPC và vòng đời evidence |
| 9 | So sánh CPU với app FW tham chiếu |
| 10 | DSP, memory ownership và adapter đa nền tảng |
| 11 | Phương pháp benchmark và kiểm chứng |
| 12 | Lộ trình, đầu ra và checklist review tiếp |
| 13 | Plan 0 bắt buộc: đóng production composition trước |
| 14 | Các execution plan có task và acceptance để giao agent |

## 1. Kết luận

Nền móng LACAI phù hợp để tiếp tục phát triển: tách neutral contracts/ports khỏi SDK,
giữ ownership frame, có admission và lifecycle, có exact-frame cascade. Không nên viết
lại toàn bộ chỉ để giảm CPU. Tuy nhiên, khoảng cách từ framework chạy được đến sản phẩm
hoàn chỉnh nằm ở **composition thực tế, đường output bền vững và hiệu quả memory/compute**.

Đề xuất chính:

1. Giữ AI runtime tập trung vào perception, tracking, thuộc tính, luật sự kiện và
   authorization. Tách I/O lưu trữ, truy vấn và Kafka ra khỏi vòng xử lý frame.
2. Chốt **data catalog + query catalog security/traffic + ngân sách** trước khi chọn engine.
   SQLite cho facts/index/outbox là phương án khởi đầu để thử nghiệm; bổ sung Parquet/DuckDB
   nếu lịch sử/analytics cần. Đây là các ứng viên có tiêu chí chọn ở mục 6, không phải stack
   bắt buộc cho mọi camera. Metadata/query/export thuộc AI APP, không có team Data thứ tư.
3. Không chọn “Parquet thay SQL”. SQL là ngôn ngữ truy vấn; Parquet là định dạng file.
   Truy vấn nhanh phụ thuộc vào biểu diễn nghiệp vụ, index, pruning và giới hạn workload.
4. Event quan trọng sang FW: ưu tiên **UDS `SOCK_SEQPACKET` + outbox bền vững + ACK
   ứng dụng + dedup**. SHM là lựa chọn sau cho metadata live lớn, không thay outbox.
5. Đích dài hạn: FW sở hữu video encode/render/ring/evidence; AI gửi annotation và yêu cầu
   evidence đã được cấp quyền. Giữ adapter encode tương thích trong giai đoạn chuyển tiếp.
6. Giảm ARM CPU bằng chuỗi **cDSP preprocess → QNN HTP → cDSP postprocess**, dùng pool
   và registered buffers có ownership rõ ràng. Sau đó xử lý face alignment/ROI và copy
   preview còn lại. Không mặc định QNN async là điều kiện để CPU thấp.
7. Chưa có cơ sở kết luận cùng workload sẽ từ 100% xuống 15–20%. App FW có cơ chế offload
   đáng học, nhưng số model, cadence, recognition, BSP và đường vào chưa tương đương.

## 2. Bằng chứng và giới hạn rà soát

### 2.1. Các mốc source

| Nguồn | Mốc xem xét | Ý nghĩa |
|---|---|---|
| LACAI | `4689940` | Source hiện tại dùng để chỉ ra các vấn đề bên dưới |
| FW tham chiếu | `50bf3295a19d8a9cb887dbff2a9fbcdad9668add` | Đọc `application/ai_app` trong repository `vqec_camera_service`; không sửa |
| Baseline FW của hợp đồng LACAI | `139d335913e19e5a33a36fa8f8d706009892db44` | Mốc khác với app DSP vừa đọc; không suy ra hai ABI giống nhau |

Các đường dẫn `application/ai_app/...` bên dưới thuộc repository FW, không phải LACAI.
Số dòng là vị trí tham khảo ở các commit trên, có thể thay đổi khi cải tiến.

Nhóm source được đối chiếu: composition/service, runtime/session/pump/cascade, contracts,
feature/output dispatch, Qualcomm preprocess/QNN/renderer, gallery, tests/CI;
phía FW gồm flow, FastRPC/DSP kernels, QNN binding, buffer registry, compose/encode,
event media sink và cấu hình artifact.

### 2.2. Mức độ bằng chứng

- **Quan sát source:** đủ để kết luận một đường gọi có copy, đang dùng reference sink,
  hay bỏ qua `source_slot`; chưa đủ quy chính xác bao nhiêu phần trăm CPU cho bước đó.
- **Số liệu LACAI lịch sử:** hồ sơ
  [FR production validation](../../testing/face_recognition_production_validation.md),
  ngày 2026-09-16, có workload và giới hạn riêng.
- **Số liệu app FW:** README của app ghi nhận thử nghiệm ngày 2026-09-09/10.
  Hai báo cáo chi tiết `docs/2026-09-09-phase1-results.md` và
  `docs/2026-09-10-phase2-results.md` được README dẫn nhưng không tìm thấy trong checkout
  FW đang đọc. Vì vậy coi số liệu đó là báo cáo lịch sử, chưa tự tái lập.
- Đợt review trước đã chạy bộ binary eSDK mở rộng: 123/123 CTest
  qua QEMU thành công. Không phải clean rebuild của thay đổi mới; không chứng minh DMA,
  QoS, CPU hay conformance với FW phát hành.
- Lần bổ sung README này không chạy thêm benchmark board, không đổi source C++, không
  cài thư viện mới và không truy cập board FW tham chiếu.
- Kiểm tra tài liệu sau khi bổ sung: docs layout pass 180 Markdown; source layout bản
  Bash pass 414 source/tool files. Không có `pwsh` trong môi trường nên không chạy bản
  PowerShell. Đây là kiểm tra cấu trúc/liên kết, không phải kiểm chứng các thiết kế đề xuất.

## 3. Điểm tốt cần giữ và nhược điểm tổng thể

### 3.1. Điểm mạnh

| Thiết kế hiện có | Giá trị cần giữ |
|---|---|
| Neutral ports tách SDK/vendor | Có thể thay Qualcomm bằng nền tảng khác mà không viết lại luật AI |
| Một acquisition cho một RAW source, fan-out giữ cùng owner | Tránh mỗi model mở một camera lease và tránh ACK sớm |
| Source epoch, submission ticket, model slot | Có nền tảng chống gán nhầm kết quả sau reset/drop/reconfigure |
| Exact-frame cascade | Face alignment dùng đúng pixels đã tạo detection, không lấy frame mới nhất tùy ý |
| Bounded queues/pools và kiểm tra contract | Có thể kiểm soát overload thay vì tích lũy latency vô hạn |
| Tách installed/entitled/desired/supported/admitted/running | Mô hình trạng thái đúng cho sản phẩm thương mại |
| Output gate theo thuộc tính thực tế | Có chỗ kiểm quyền metadata/identity, không chỉ nút bật/tắt UI |
| Gallery bảo vệ tách khỏi vector index dẫn xuất | Không coi index tìm kiếm là nguồn dữ liệu sinh trắc học duy nhất |
| ADR ghi rõ module reserved/unwired | Tránh nhầm helper đã test thành capability đã tích hợp |

### 3.2. Nhược điểm kiến trúc

- Nhiều contract/helper đúng nhưng composition production chưa đi qua đầy đủ các gate.
  Tính đúng cục bộ không tự tạo ra tính đúng end-to-end.
- Composition root còn trộn harness, policy fixture, adapter production, FR, control và
  output. `service_main` khoảng 2.000 dòng làm khó xác định authority và kiểm thử lifecycle.
- Thiên về hoàn thiện hạ tầng trước sản phẩm: một số feature chỉ có thư mục/README;
  đăng ký được processor không chứng minh đúng nghiệp vụ.
- Bounded execution chưa đồng nghĩa mọi lời gọi đều có latency hữu hạn. SDK đồng bộ và
  `join()` vẫn có thể kéo dài stop/control.
- Memory abstraction còn nghiêng về CPU-owned byte vectors. Hợp đồng đúng dtype/layout
  nhưng chưa thuận lợi cho chuỗi DSP/QNN không đưa toàn bộ tensor về ARM.
- Storage/event transport/operational recovery còn thiếu; đây là phần sản phẩm cốt lõi,
  không phải chỉ thêm một sink cuối pipeline.

## 4. Khoảng trống cụ thể cần sửa

Ưu tiên trong bảng là đề xuất review: P0 chặn an toàn/đúng semantics; P1 chặn mục tiêu sản phẩm;
P2 tăng khả năng vận hành. Không có nghĩa mọi dòng đã gây lỗi ở mọi cấu hình.
Các nhãn A01–A16 dưới đây chỉ thuộc README này, không ánh xạ tự động sang ID trong
`architecture_alignment_review.md`; khi mở issue cần giữ đường dẫn tài liệu làm namespace.

| ID | Ưu tiên | Bằng chứng tại source hiện tại | Hướng sửa và điều kiện xong |
|---|---|---|---|
| A01 | P0 | `service_main.cpp:1150–1195` dựng request với `desired_enabled_`, `entitlement_granted_`, `resource_admitted_` đều `true`, rồi tạo output policy | Production phải dùng snapshot authority thật theo source/feature/attribute/revision; fixture chỉ thuộc harness; test hai feature dùng chung model nhưng chỉ một được cấp quyền |
| A02 | P0 | `production_platform.cpp:148–168,603–614` đăng ký cùng factory tạo `reference_zone_feature` cho các processor contract | Chỉ đăng ký implementation đúng schema/nghiệp vụ; unsupported phải bị từ chối, không giả thành feature khác |
| A03 | P1 | `service_main.cpp:1080` dùng `reference_event_sink`; sink chỉ giữ event cuối và đếm delivery | Cần output owner, durable outbox, transport và receipt; dispatch helper hiện tại không có những chức năng này |
| A04 | P1 | `production_platform.cpp:638–668` graph/processor bỏ qua `source_slot`; composition factory từ chối graph pointer trùng | Cần instance/binding theo source-model hoặc executor dùng chung có ticket/fairness được thiết kế rõ; không bỏ check trùng để che lỗi |
| A05 | P1 | Production yêu cầu dimensions nguồn bằng nhau khi chia decoder, secondary cascade chỉ một source; preview ring cũng giới hạn một source | Công bố capability đúng; tách per-binding transform/tracker/decoder state; test cùng detector trên hai nguồn khác kích thước |
| A06 | P0 | `production_platform.cpp:680` gọi renderer trực tiếp; `qtiv_renderer.cpp:458–522` copy, encode, ghi ring riêng | Đi qua authorization/freshness/correlation/demand gate thật; helper `encoded_dispatch` có source không có nghĩa đường production dùng nó |
| A07 | P1 | Renderer tạo PTS từ bộ đếm submission; cache overlay trong service ghép latest observations của nhiều model | Giữ mapping frame/epoch/clock; TTL riêng từng observation; không coi box cũ và video mới là cùng frame; evidence không dùng timestamp suy đoán |
| A08 | P0 | Production lấy đường model library từ JSON; helper `artifact_digest` chưa đóng kín đường load artifact | Tích hợp manifest đã xác thực, allowed roots và tải artifact bất biến; hash không phải chữ ký, receipt không tự chống TOCTOU |
| A09 | P1 | Executor gọi cascade đồng bộ; coordinator align/quantize/submit theo ROI; stop pump join worker | Bounded task scheduling, control responsive, stop có trạng thái drain/quarantine; không giả hủy DMA bằng timeout |
| A10 | P1 | `fastcv_processor`, `fastcv_aligner`, QNN output copy và renderer full-frame copy vẫn có ARM work | Profile rồi bỏ copy/convert đúng điểm; offload DSP theo mục 10, không đánh giá chỉ bằng tên FastCV |
| A11 | P1 | Attribute reader/catalog có nhưng chưa có pipeline sản phẩm tạo thuộc tính màu áo kèm freshness/quality | Cần person/torso ROI, attribute producer, temporal smoothing và golden; có storage không tự tạo khả năng tìm người áo đỏ |
| A12 | P1 | Nhiều thư mục feature, pose/OCR vẫn placeholder; production tracker là reference tracker | Lập danh sách accepted feature rõ; nghiệm thu dataset cho tracker/feature; FR matching không tự bằng attendance/PAD đầy đủ |
| A13 | P1 | Resource envelope/capability khai báo chưa bằng measured admission; recovery controller còn reserved | Admission từ profile đo trên board và FW concurrent load; recovery chỉ sau quiesce/reset contract |
| A14 | P1 | Gallery có mã hóa và atomic persistence, key đang file-backed; rotation/hardware key/anti-rollback chưa hoàn chỉnh | Threat model, bind gallery identity vào dữ liệu xác thực, rotation/recovery; không xuất gallery/embedding sang metadata mặc định |
| A15 | P2 | CI còn job cấu hình host Clang; eSDK jobs phụ thuộc biến runner; expanded chưa bao phủ mọi option production | Đồng bộ CI với quy tắc eSDK; job thực chạy có chứng cứ; ma trận bật FastCV/control/enrollment; không lấy job bị skip làm pass |
| A16 | P2 | Một số trang mô tả thiếu renderer trong khi source đã có; status helper và production dễ bị gộp | Mỗi capability ghi rõ source, wired, logic-tested, board-smoke, accepted; một đầu mối status và link bằng chứng |

Các file source chính để sửa sau review:
[service main](../../../src/app/service/bootstrap/vqec_vision_service_main.cpp),
[production platform](../../../src/app/platform/vqec_vision_production_platform.cpp),
[composition factory](../../../src/app/composition/vqec_vision_runtime_composition_factory.cpp),
[renderer](../../../src/adapters/qualcomm/media/vqec_vision_qtiv_renderer.cpp),
[cascade coordinator](../../../src/app/cascade/vqec_vision_cascade_coordinator.cpp),
[CI](../../../.github/workflows/ci.yml).

Lưu ý A01: preload usecase filtering đã tồn tại, không phải toàn bộ entitlement vắng mặt.
Vấn đề là việc một model được load không cấp quyền tự động cho mọi feature cùng dùng model đó.
Tương tự A08 không phủ nhận helper hash; vấn đề nằm ở chuỗi trust tới lần load thực tế.

## 5. Kiến trúc đích và phạm vi team

### 5.1. Các thành phần logic, không phải ba team mới

```text
FW RAW source -- frame lease --> AI runtime
                                 | perception / track / attrs / rules
                                 | output authorization
                                 +-- annotation live -------------> FW render/video
                                 |
                                 +-- bounded metadata/event IPC --> Edge data service
                                                                    | journal + hot index
                                                                    +--> local query / Parquet
                                                                    +--> FW event outbox --UDS--> FW evidence
                                                                    +--> Kafka outbox ---------> Center

FW control/config -- D-Bus --> AI runtime
FW evidence result ---------> Edge data service --> evidence_ref / cloud update
```

Đây là ranh giới logic; đề xuất deployment đầu tiên là một AI runtime và một edge data
service riêng, không phải một process cho mỗi feature. FW tiếp tục sở hữu các service video
và storage. Kafka producer có thể là worker của edge data service, không cần thêm daemon
ngay. Query/compaction bị giới hạn tài nguyên và không được khóa writer event.

Chi phí thêm process là IPC, packaging, health và phiên bản schema; đổi lại tránh để disk,
compaction, TLS/Kafka và query lớn làm nghẽn frame loop. Nếu board không đủ RAM, vẫn giữ
ranh giới port và worker riêng trước; chỉ đổi deployment sau khi đo, không trộn lại ownership.

### 5.2. Phạm vi ba team và một owner cho từng đầu ra

Cơ cấu áp dụng: **BSP+FW / AI APP / AI Model**. `Edge data service`, `platform adapter`
và `center connector` là module/workstream, không phải team độc lập. AI APP lead sở hữu
kiến trúc tích hợp AI, data/query catalog và acceptance sản phẩm AI; không tự duyệt thay
BSP+FW cho driver/ABI/media hoặc AI Model cho chất lượng model.

| Đầu ra | Team chịu trách nhiệm chính | Team phối hợp và giới hạn |
|---|---|---|
| Sensor/ISP, capture, demux/decode, RAW service | BSP+FW | AI APP khai báo nhu cầu profile; không mở sensor/RTSP riêng |
| Allocator, DMA/cache/fence/reset, SDK và DSP execution environment | BSP+FW | Cung cấp primitive, toolchain, signing và bằng chứng; AI APP không đoán completion |
| Model, ontology nhãn/thuộc tính, preprocess/decode semantics | AI Model | AI APP review khả năng tích hợp; không chỉ nhận một file model |
| Production preprocess/decoder/tracker adapter, kể cả DSP skel/kernel do ứng dụng sở hữu | AI APP | AI Model cấp thuật toán/golden; BSP+FW cấp và qualify môi trường phần cứng; không giao mơ hồ “tối ưu DSP cho BSP” |
| Tracking runtime, fusion, attribute freshness, feature rules | AI APP | AI Model cấp reference/quality contract; detection không tự là event |
| Usecase catalog và ánh xạ thương mại → compute/data/query | AI APP | Backend dùng D-Bus/schema AI APP cho UI; AI Model xác nhận dependency/quality; BSP+FW không tham gia app lifecycle |
| Scene/lane/rule/calibration schema và tính hợp lệ khi áp dụng | AI APP | AI Model cung cấp phương pháp/sai số; BSP+FW thu nhận cấu hình, profile và hiện trạng camera |
| Timestamp/exposure/PTZ/profile facts, signal/controller input | BSP+FW | AI APP chỉ tính traffic metric khi validity/time contract đáp ứng |
| Metadata journal, local DB/index, archive, query engine/API | AI APP | BSP+FW cấp volume/quota/security context; không truy cập schema DB riêng của nhau |
| Metadata authorization, retention logic, deletion/export records | AI APP | BSP+FW cung cấp authenticated principal/policy/provisioning; không đồng nhất đăng nhập UI với quyền mọi thuộc tính |
| Query UI/API gateway, video playback và media access | BSP+FW | Gọi API metadata của AI APP; không chạy SQL trực tiếp trên file DB AI |
| Evidence prebuffer/clip/snapshot/media lifecycle/upload | BSP+FW | AI APP tạo intent, retry và đối chiếu receipt; không tự ghi video thay FW |
| Annotation semantics và quyền field | AI APP | BSP+FW render theo frame/TTL/scope sau migration; không suy luận identity từ label |
| Preview render/encode/ring | AI APP hiện tại; BSP+FW ở kiến trúc đích | Chuyển owner theo mục 5.3, không có hai writer hay khoảng trống tương thích |
| Kafka metadata producer, schema, outbox, replay | AI APP | BSP+FW cấp kết nối/credential handle/quota; AI Model không làm transport |
| Center metadata ingest/lake/query application nếu thuộc sản phẩm này | AI APP | Workstream server tách khỏi runtime edge; không mặc định có team Data khác tiếp nhận |
| Hạ tầng chạy center: broker/storage/network/secrets/monitoring deployment | BSP+FW, đầu mối tích hợp đề xuất | Nếu ngoài năng lực/phạm vi hiện tại phải chốt người tiếp nhận; không coi camera delivery là đã giao xong center |
| FR gallery, enrollment, matching, protected store/index | AI APP | Giữ ADR 0004; BSP+FW cung cấp secure-storage primitive và UI/transport |
| Dataset, training/export/quantization, quality report và reference outputs | AI Model | AI APP cung cấp lỗi thực tế/trace đúng quyền; không gửi dữ liệu nhạy cảm tự động |
| Base OS/system image, LACAI service supervision, OTA/rollback, FW resource reservation | BSP+FW | AI APP/AI Model cung cấp runtime compatibility, health và version manifest; đây là C10, không phải usecase app lifecycle |
| Usecase App Manager, entitlement verify, private store, inventory, install/update/rollback/uninstall | AI APP | Backend là authenticated D-Bus peer theo contract AI APP; BSP+FW không có state authority hay acceptance gate |

Tách **chủ sở hữu dữ liệu logic** khỏi **chủ sở hữu thiết bị lưu trữ**: metadata schema/index
do AI APP quản lý, media do BSP+FW quản lý; cùng nằm trên flash không làm chúng thành một DB.
Mỗi team viết service/adapter phía mình; public schema có một owner, không hai bản fork.

Lưu ý chuyển tiếp: `feature_catalog.md` hiện còn ghi retrieval backend thuộc FW. Phạm vi
metadata/query AI APP ở đây là hướng cải tiến theo cơ cấu vừa chốt; phải cập nhật authority
docs/contract bằng thay đổi được duyệt trước khi ship. README không âm thầm supersede
hợp đồng FW hiện hành. Center là deliverable riêng có người/ngân sách, không phát sinh
vô hạn bên trong task “thêm Kafka”.

### 5.3. Chuyển encode sang FW thế nào cho an toàn

Đồng ý rằng encode không phải năng lực nghiệp vụ cốt lõi của AI APP. Tuy nhiên,
[hợp đồng hiện tại](../../contracts/fw_release_compatibility.md) và
[system architecture](../../architecture/system_architecture.md) vẫn yêu cầu AI sản xuất
preview overlay/encode/ring. Không được bỏ renderer ngay rồi coi đó là tối ưu hoàn thành.

Các bước chuyển:

1. AI/FW thống nhất annotation contract: source/frame/epoch, clock mapping, geometry space,
   transform, confidence, TTL, policy revision, thứ tự update và hành vi revoke.
2. FW công bố capability render/encode/evidence và registry output versioned. Annotation
   trễ phải bị bỏ hoặc xử lý bằng bounded alignment window; không giữ video vô hạn.
3. FW luôn có encoded prebuffer nếu nghiệp vụ cần evidence, dù không có người xem preview.
   `no viewer` không đồng nghĩa `no evidence consumer`.
4. Giữ chế độ `legacy_ai_encode` và thêm `fw_video_owner` qua cấu hình/capability negotiation.
   Tên ở đây là khái niệm đề xuất, chưa phải API. Chỉ một writer cho mỗi ring/stream.
5. Chạy conformance RTSP/UI/evidence, profile change, first viewer, last viewer, late join,
   source reset và rollback. Chỉ gỡ adapter cũ sau khi FW owner ký acceptance.
6. Cập nhật ADR, system architecture và FW contract trước khi đổi implementation boundary.

FW có thể dùng stream encode sẵn cho evidence nếu đáp ứng yêu cầu sản phẩm; không mặc định
cần encode lần nữa. Nếu cần burn-in annotation, FW sở hữu nhánh đó. Overlay phía client
tiết kiệm video work nhưng không thay evidence burn-in khi sản phẩm yêu cầu.

Chuyển encode chỉ giảm CPU/RSS của process AI; phải đo tổng CPU/DDR/power của hệ thống để
biết có tối ưu thật hay chỉ chuyển chi phí sang FW.

### 5.4. Danh mục contract để chia việc ngay

Các ID C01–C10 là backlog contract của kế hoạch, không phải endpoint đã phát hành.
Owner soạn schema và version; phía nhận phải ký conformance. Transport được chọn sau
semantics; một file IDL không thay mô tả ownership, failure và authority.

| ID / owner | Bên bàn giao → bên nhận | Nội dung bắt buộc | Gate nghiệm thu |
|---|---|---|---|
| C01 RAW source / BSP+FW | BSP+FW → AI APP | Source/profile identity, planes/stride/modifier, frame/epoch/clock, lease/sync/completion, acquire/release/recovery | Padded frame, slow consumer, reconnect, crash/quiesce; không ACK sớm |
| C02 Accelerator platform / BSP+FW | BSP+FW → AI APP | SDK/ABI/toolchain/version, allocator/import/cache/fence, HTP/cDSP capabilities, signing, thermal/resource envelope, reset semantics | Diagnostic runner + memory/completion traces trên target; AI APP adapter test cùng model golden |
| C03 Model integration kit / AI Model | AI Model → AI APP | Artifact provenance, ontology, tensor/pre/decode/quantization, temporal/quality/unknown, reference code + golden tensors/observations, supported profiles và KPI | M0–M4 theo model integration; đổi semantics phải đổi version; không nhận binary-only |
| C04 Scene/calibration/time / AI APP | BSP+FW → AI APP; AI Model review phương pháp | Camera pose/profile revision, line/zone/lane map, coordinate units, calibration validity/error, signal state/source/clock/freshness | Profile/PTZ đổi làm invalid đúng; thiếu input trả unsupported/unknown; không bịa tốc độ hoặc pha đèn |
| C05 Control/auth/resource / AI APP | BSP+FW ↔ AI APP | Desired/config revisions, authenticated principal/grant, source/field/export scopes, capability/admission/readiness và quota | Stale/CAS/retry/revoke tests; accepted khác running, model loaded khác feature entitled |
| C06 Metadata/query / AI APP | AI runtime → data service; BSP+FW ↔ query API | Data/query catalog mục 6, schema/revision/ID, supported predicates, projection, snapshot/paging, completeness/retention/errors, ingest receipt | Golden query set security+traffic, auth field-level, hot/cold consistency, crash và SLO |
| C07 Event/evidence / AI APP | AI APP ↔ BSP+FW | Event/command IDs, phases/revisions, authorized intent, ack level, pre/post-roll, actual interval, dedup/receipt/reconcile | Lost ACK/restart/duplicate/end-before-start/disk-full; media status không lẫn event status |
| C08 Annotation/video / AI APP | AI APP → BSP+FW | Frame/epoch/clock/transform/TTL, allowed labels, capability/demand, legacy/new ownership mode | Late overlay/revoke/profile switch/first viewer; một writer; FW ký migration |
| C09 Cloud metadata / AI APP | Edge AI APP → center; BSP+FW vận hành endpoint | Schema/partition/order, outbox/dedup, update/tombstone, broker vs lake receipt, export scope và offline quota | Replay/late data/delete/ambiguous ACK; lake commit không suy từ produce success |
| C10 Deployment/operations / BSP+FW | Cả ba team → BSP+FW tích hợp | Base system/runtime manifests/SBOM, config roots, UID/volume/quota/key handles, health/reason codes, compatibility/OTA/rollback; usecase apps thuộc AI APP C05 | Clean base install/reboot/upgrade/rollback/mixed-load soak; health trả đúng degraded/fault |

C03 kế thừa [model integration](../../contracts/model_integration.md); C01/C05 kế thừa
[FW–AI contract](../../contracts/fw_ai_app_contract.md) và
[usecase control](../../contracts/fw_usecase_control.md). Không tạo endpoint cạnh tranh với
contract đang có; bổ sung version/gap có chủ đích.

Mỗi contract phải đi kèm: schema/IDL + fixtures hợp lệ/lỗi + compatibility matrix + max
bytes/rate/queue/deadline + retry/idempotency + crash/stop/revoke + owner từng resource +
test harness hai phía. Đưa cả `not_supported`, `partial`, `resource_exhausted` và version
mismatch vào gate, không chỉ happy path. Tham số triển khai được validate, không hardcode.

### 5.5. Quyền quyết định và điểm dừng bàn giao

- **AI APP lead:** duyệt data/query/feature contract, decomposition, runtime/storage/export
  architecture và integration parity; giữ backlog cross-team. Không nhận feature “xong”
  nếu chỉ model mAP tốt hoặc FW endpoint trả `ok`.
- **BSP+FW lead:** duyệt DMA/driver/SDK/ABI, resource reservation, media/storage durability,
  source/signal/time facts, deployment và target compatibility.
- **AI Model lead:** duyệt ontology/model/preprocess/decode/quality/golden, điều kiện không
  quan sát được và chất lượng trên dataset. Không tự chọn runtime policy/retention/UI.
- **Qua boundary:** cả owner lẫn consumer ký version + tests; thay ABI/ownership/entitlement
  cần lead và owner review theo quy tắc dự án. Nghiệp vụ sản phẩm còn chưa chốt được ghi
  capability pending, không thay bằng phỏng đoán thuật toán.
- **Kết thúc edge:** local query + FW evidence + broker delivery có chứng cứ. **Kết thúc
  center:** ingest/lake commit/query/purge có chứng cứ riêng. Bàn giao Kafka schema không
  mặc nhiên bao gồm xây toàn bộ platform center.

## 6. Metadata và nghiệp vụ truy vấn chung cho security/traffic

### 6.1. Phạm vi và cách đi từ nghiệp vụ đến storage

Danh sách lead cung cấp có **18 mục**, không phải 16. Mục này giữ đủ 18 và dùng nhãn
S01–S18 để truy vết yêu cầu; đó là nhãn kế hoạch, **chưa phải `usecase_id` phát hành**.
Không tự gộp blacklist với điểm danh, hoặc abandoned với truy vết đồ thất lạc để giảm số.
Ghi chú `15/10` của ANPR là mốc người dùng cung cấp, chưa có năm/workload/gate xác nhận;
không chuyển thành cam kết release trong tài liệu này.

Kiến trúc không phải “database cho 18 bài”: xây mô hình đối tượng, thời gian, không gian,
quan hệ, phép đo và sự kiện dùng chung; security/traffic thêm ontology, rule và read model.
Đơn vị mở rộng là schema/query capability có version, không thêm cột cho từng model.

Thứ tự chốt: **nghiệp vụ → dữ liệu cần giữ → semantics truy vấn → retention/SLO → index
và storage → benchmark → lựa chọn engine**. Không bắt đầu bằng Parquet rồi ép tất cả
nghiệp vụ vào scan file; cũng không mặc định một relational DB chịu mọi tải tương lai.

“Đầy đủ” ở đây là catalog các họ truy vấn cho phạm vi đã biết, kèm cơ chế đăng ký mở rộng;
không hứa mọi câu hỏi ngôn ngữ tự nhiên hay usecase traffic tương lai đều trả lời được từ
metadata đã lưu. Dữ liệu không thu, hết retention hoặc không đủ chất lượng không thể được
khôi phục chính xác bằng một query tốt hơn.

### 6.2. Ma trận 18 usecase security đã được cung cấp

Các query ID Q01–Q30 được định nghĩa tại mục 6.7. Mỗi dòng phải có feature contract,
model/data dependency, quyền lưu/xuất, quality gate và dataset riêng trước activation.

| ID / nghiệp vụ | Facts và dữ liệu cần giữ | Nghiệp vụ truy vấn | Điều kiện/giới hạn phải biểu diễn |
|---|---|---|---|
| S01 Hút thuốc khu vực cấm | Person/hand/object association, smoking episode, zone/rule revision, evidence refs | Q02/Q05/Q08: ai/track nào, khu vực nào, lúc nào, số episode | Candidate vật nhỏ không tự là hành vi hút thuốc; temporal confirmation, unknown do che khuất |
| S02 Vật thể nghi ngờ/vũ khí | Object class/candidates, person-object relation theo interval, threat episode | Q01/Q02/Q06/Q08: vật gì, xuất hiện/cầm bởi track nào, hành trình liên quan | Vật giống vũ khí là candidate; gần nhau không chứng minh sở hữu hoặc ý định |
| S03 PPE | Person/body/PPE association, từng item present/absent/not_observable, requirement theo zone/role | Q01/Q05/Q08/Q15: thiếu mũ/áo trong zone nào, bao lâu, tỷ lệ trên số quan sát hợp lệ | Không quan sát thấy khác đã xác định không đeo; role không được tự suy đoán từ ảnh |
| S04 Cháy/khói | Scene/region observation, class/score/extent, episode start/update/end, evidence | Q08/Q15: vị trí, onset, thời lượng, recurrence, trạng thái xử lý | Event có thể không có person/track; kết thúc quan sát không chứng minh hết nguy hiểm |
| S05 Người trong blacklist | Face quality, identity candidates, match decision, gallery/watchlist revision, person-track link | Q09/Q08/Q26: lần xuất hiện, watchlist tại thời điểm đó, lý do match và evidence | Match candidate khác xác nhận con người; không ghi embedding vào event/log chung |
| S06 Nhận diện điểm danh | Recognition encounters, identity decision, attendance session/schedule/timezone, check-in/out candidate, correction/audit | Q10/Q09: có mặt/vắng/chưa xác định, đầu/cuối phiên, trùng/đi muộn theo policy | Nhận diện một frame không là chấm công hoàn chỉnh; lịch/roster và quyết định vận hành phải có authority |
| S07 Tuổi/Giới tính | Estimated age band, schema-defined gender label/candidates, confidence/unknown, observation interval | Q01/Q14/Q15: phân bố theo vùng/giờ trong phạm vi được phép | Chỉ là ước lượng từ model, không phải tuổi/giới tính đã xác minh; không suy thêm thuộc tính nhạy cảm |
| S08 Heatmap theo thời gian | Position/dwell contributions, grid/coordinate revision, time bucket, observed coverage | Q14/Q15: density/dwell/occupancy theo thời gian/vùng | Track count, frame sample count và thời gian hiện diện là ba đại lượng khác nhau |
| S09 Xâm nhập trái phép | Entry/crossing/dwell fact, zone/rule/schedule, authorized-access input nếu có | Q04/Q05/Q08: vượt line/đi vào vùng theo hướng, ngoài lịch, lịch sử episode | Phát hiện người không tự chứng minh không có quyền; thiếu access input phải giới hạn nghĩa rule |
| S10 Thống kê người ra/vào | Directional crossings, count contribution, dedup key, reset/gap và bucket revision | Q04/Q14/Q15: lượt vào/ra, net flow, breakdown theo zone/time | Lượt qua khác unique người; occupancy từ in-out cần baseline và coverage |
| S11 Theo dõi người | Observation/track segments, trajectory chunks, attributes và association hypotheses | Q01/Q03/Q04/Q05/Q07: tìm, dựng đường đi, qua A rồi B, ứng viên liên camera | Local track ID không là person identity; gap/ID switch phải hiện diện |
| S12 Cảnh báo theo ngữ cảnh VLM | Context/rule/prompt revision, model version, structured claims, frame/clip references, verification disposition | Q17/Q08/Q26: cảnh báo theo rule, xem claim và bằng chứng, lỗi/hallucination/correction | Free text không là fact mặc định; VLM không cấp quyền, tự gọi action hay sửa rule |
| S13 Vật bỏ quên/biến mất | Object inventory/track, stationary/presence intervals, relation history, missing observation và episode | Q06/Q11/Q08: vật ở đâu, bao lâu, trước/sau mất, người liên quan | Bỏ quên, bị lấy đi, ra khỏi FOV và occluded không được gộp thành một nhãn |
| S14 Truy vết đồ thất lạc | Object descriptors/attributes, track trajectory, sightings, carry/near relations; embedding tham chiếu nếu có quyền | Q01/Q03/Q06/Q07/Q16: tìm bằng thuộc tính/ảnh, last seen, hành trình và ứng viên | Đồ được báo mất là business input; similarity không chứng minh cùng vật/sở hữu |
| S15 Hành lí/xe đẩy | Entity category, MOT segments, movement/dwell, person-object relations, zone passages | Q03/Q04/Q05/Q06/Q14: tuyến đi, lưu lâu, số lượng, liên kết người-hành lí | Association phải có interval/confidence; không gán người gần nhất làm chủ vĩnh viễn |
| S16 Biển số xe/ANPR | Vehicle/plate tracks, association, raw/normalized OCR/candidates, quality, passage và evidence | Q12/Q13/Q18: exact/partial plate, lượt qua, liên kết xe và hình xác minh | Plate string không là global vehicle identity; OCR không chắc chắn và version normalization phải giữ |
| S17 Tụ tập đám đông | Group/region episode, participant refs khi đáng tin, count/density/dwell time series | Q08/Q14/Q15: thời điểm/vùng tụ tập, quy mô/đỉnh/thời lượng | Dense scene có thể không track đủ từng người; phương pháp count phải ghi rõ |
| S18 Bất thường/ẩu đả/xung đột | Action hypotheses theo interval, participant/pose refs có hạn, temporal episode, evidence | Q08/Q06/Q15/Q26: loại hành vi, thời điểm, nhóm liên quan, review false positives | Không kết luận ý định/quan hệ xã hội; classifier score không tự là xác suất đã calibration |

Không bắt buộc lưu toàn bộ keypoints, crops hoặc tensor vì một usecase dùng chúng lúc
inference. Lưu cái cần để query/giải thích theo policy; dữ liệu debug/golden là kênh riêng.
Chưa có producer đạt chất lượng thì query tương ứng phải báo chưa hỗ trợ, không chỉ trả
mảng rỗng làm người dùng hiểu rằng “không xảy ra”.

### 6.3. Traffic mở rộng dùng cùng nền dữ liệu

ANPR trong S16 đã là miền giao nhau. Traffic không chỉ thêm `vehicle` vào bbox: cần lane,
road geometry, measurement, signal/external facts và chất lượng thời gian/hiệu chuẩn.

| Nhóm traffic | Dữ liệu bổ sung | Query và gate |
|---|---|---|
| Đếm/phân loại phương tiện | Vehicle class/attributes, directional passage, lane assignment interval | Q18/Q14; ontology và count error theo điều kiện ngày/đêm, không cần nhận diện từng xe thật |
| Lane/hướng/rẽ | Lane topology, permitted directions/movements, lane transition, entry/exit leg | Q19/Q04; rule/geometry revision, shared boundary và lane unknown |
| ANPR/parking/access | Plate consensus + passage, entry/exit encounter và roster/session nếu dùng | Q12/Q13/Q20; ghép phiên có ambiguity/timeout; không ép mọi entry có exit |
| Tốc độ/gia tốc | Position/time samples, speed estimate, method, unit, calibration/time uncertainty | Q21; metric hợp lệ chỉ trong calibration domain/time; không coi pixel/s là km/h |
| Dừng/đỗ/queue/congestion | Stop/dwell interval, lane queue estimate, occupancy/flow/speed bucket và coverage | Q20/Q22; phân biệt dừng tại đèn với đỗ theo rule; queue length có unit/method |
| Ngược chiều/sai làn/chuyển làn/stop-line | Passage/movement facts + versioned allowed movement/rule | Q19/Q23; event candidate cần đủ geometry, direction và interval |
| Đèn đỏ | Authoritative signal-phase interval, approach/lane mapping, stop-line crossing, synchronized time | Q23; missing/stale/ambiguous signal ⇒ không xác nhận vượt đèn |
| Hành trình/OD/travel time | Entry-exit associations, topology, travel-time bounds, plate/ReID evidence | Q24/Q07; edge một nguồn chỉ trả phạm vi thấy; liên camera/thiết bị cần federation/center |
| Pedestrian/near-miss/incident | Person/vehicle relations, trajectories/measurements, incident candidates, evidence | Q06/Q23; chỉ đăng ký metric và model đã qualify, không tự suy đoán tai nạn từ bbox gần |

Tốc độ/vi phạm trong catalog này là **kết quả phân tích hoặc candidate có chất lượng công
bố**, không tuyên bố đủ điều kiện xử phạt/chứng nhận đo lường. Acceptance cho mục đích đó
là gate riêng của sản phẩm. PTZ, zoom, camera rung/di chuyển, profile/crop hoặc calibration
thay đổi phải invalidate phép đo phụ thuộc; giữ lịch sử để query đúng bản cũ.

Đối chiếu interoperability: ONVIF Profile M có metadata generic object, human body/face,
vehicle, plate và geolocation. Nên chuẩn bị mapping ở adapter BSP+FW; không lấy XML/wire
schema làm storage schema nội bộ hoặc tự tuyên bố ONVIF conformance.
Tham khảo [ONVIF Profile M](https://www.onvif.org/profiles/profile-m/).

### 6.4. Data catalog: cần lưu gì, ai tạo và giữ đến mức nào

Quy ước: **D** là bản ghi quyết định/fact đã chấp nhận cần bền vững; **H** là chi tiết
sampling có hạn; **P** là read model/index/aggregate dẫn xuất, có version. Identity,
embedding và biển số là metadata thông thường theo policy sản phẩm hiện tại; access domain
vẫn kiểm entitlement/output nhưng không tạo lớp storage nhạy cảm riêng. Đây là lớp dữ liệu,
không phải số ngày retention.
Một fact do AI suy ra vẫn có uncertainty; chữ “fact” không biến nó thành ground truth.

| ID / record family | Trường cần thiết ngoài envelope chung | Nguồn/owner semantic | Lớp và mục đích |
|---|---|---|---|
| D01 `source_scene_revision` | Source/profile, FOV/orientation, coordinate spaces, scene/zone/line/lane/grid/topology revisions, calibration validity/domain/error | BSP+FW cấp source facts; AI APP quản lý scene/calibration snapshot | D; giải thích lịch sử, không ghi đè cấu hình cũ |
| D02 `observation_sample` | Entity category/class candidates, bbox/mask ref/keypoints cần thiết, score/quality, observed/predicted, frame/ticket | AI APP từ model kit AI Model | H; trace/recompute có giới hạn; không mọi frame mặc định |
| D03 `track_segment` | Local track key, segment start/end, close reason, gaps, representative observation refs, spatial bounds | AI APP tracker | D; continuous local identity, không identity con người |
| D04 `trajectory_chunk` | Time-ordered anchors/coordinates, sample mode, uncertainty, coordinate revision, observed/predicted và gap markers | AI APP | H/D theo usecase; đường đi và geometry query |
| D05 `attribute_assertion` | Subject ref, typed schema/value/candidates, confidence/quality, observed/valid interval, freshness, fusion/model provenance | AI APP; AI Model chốt ontology/quality | D hoặc H; access domain theo field; không chỉ latest value |
| D06 `relation_interval` | Subject/object refs, relation type, valid interval, confidence, evidence refs, method | AI APP; model có thể cấp association candidates | D; carries/near/person-face/vehicle-plate/group-member; không ép thành ownership |
| D07 `passage` / `presence_interval` | Subject ref, scene primitive/revision, enter/cross/exit/direction, begin/end, crossing evidence, open/closed/uncertain | AI APP geometric/temporal rules | D; dùng chung gate, lane, intrusion, dwell và counting |
| D08 `measurement_sample` | Quantity, value/unit, method/version, time/window, calibration ref, uncertainty/validity | AI APP từ platform/model facts | H/D; speed, distance, density, queue length; không số float vô nghĩa |
| D09 `external_state_interval` | Signal/access/roster/schedule revision, value, source authority, effective time, receive time, TTL và quality | BSP+FW bàn giao; AI APP lưu bản cần cho quyết định | D; query/giải thích theo trạng thái tại thời điểm xảy ra |
| D10 `event_episode` | Feature/rule/type, lifecycle, participants/region, onset/end, severity, claims/decision/evidence refs | AI APP | D; có thể scene-only, không bắt buộc track |
| D11 `recognition_encounter` / `attendance_decision` | Identity candidate, gallery/roster revision, score/threshold/quality, session, confirmation/correction actor/reason | AI APP; BSP+FW nhận quyết định người vận hành qua API | D; metadata thông thường, tách match, visit/encounter và attendance decision |
| D12 `plate_read` / `plate_consensus` | Raw OCR, normalized text + normalization revision, alphabet/region candidates, per-read quality, vehicle relation và passage ref | AI APP từ ANPR kit | D; metadata thông thường, giữ alternatives, không biến OCR thành đăng ký xe |
| D13 `context_assessment` | VLM model/prompt/rule/context revisions, bounded structured claims, supporting frames, verifier/review status | AI APP; AI Model cấp output/quality contract | D; raw free text debug chỉ opt-in có quota/quyền |
| D14 `aggregate_bucket` | Dimensions/revisions, time interval, count/sum/histogram/sketch, contribution version, coverage denominator và completeness | AI APP | P; density/count/traffic stats; phân biệt exact/approximate |
| D15 `evidence_reference` | FW request/media IDs, lifecycle, actual interval/gaps, source/frame mapping, retention/availability và digest khi có | BSP+FW owns media; AI APP owns link | D; clip/crop bytes không vào DB metadata chung |
| D16 `association_hypothesis` | Track/entity links, camera pair/domain, candidate score/method, validity, accepted/rejected/review revision | AI APP | D; access domain theo output, cross-camera không sửa mất track gốc |
| D17 `coverage_health_interval` | Feature enabled/running, source loss, model fault, dropped/sample rates, clock/calibration health, storage/export gaps | AI APP + BSP+FW health facts | D; phân biệt “không có kết quả” và “không có dữ liệu” |
| D18 `delivery_archive_audit` | Per-sink outbox, receipt, file manifest/checksum/coverage, schema migration, access/correction/deletion audit | AI APP; BSP+FW cấp volume/security context | D/P; vận hành, replay, purge và provenance |

Không một usecase phải emit đủ D01–D18. Mỗi usecase đăng ký `produces`, `requires`, `retains`,
`query_capabilities`, schema versions và bounded rate/cardinality; admission tổng hợp phần
chia sẻ, tránh ghi cùng track/attribute 18 lần. Một canonical fact có thể liên quan nhiều
feature nhưng quyền query/export phải kiểm theo field và mục đích, không kế thừa mọi quyền
của feature đầu tiên tạo nó.

**Envelope chung:** record ID + revision + supersedes/tombstone nếu có; device/source,
boot/session/epoch, producer/model/ontology/config/rule revisions, event/valid time và
recorded time, clock mapping/uncertainty, quality/provenance, classification/quyền tham chiếu.
Trường không áp dụng phải được biểu diễn rõ, không nhét ID giả hoặc chuỗi rỗng có nhiều nghĩa.

### 6.5. Identity, thời gian, geometry và dữ liệu thiếu

1. `track_key` gồm device/source + boot/session identity + epoch + local track ID; observation,
   segment, passage, event và media có ID riêng. Face identity, plate text và vehicle/person
   entity hypothesis không dùng chung khóa với track. Không merge destructive khi ReID đổi ý.
2. Bảo toàn cả **valid/event time** lẫn **recorded/correction time**: có thể hỏi “kết quả đã
   biết lúc vận hành” và “kết quả mới nhất sau sửa”. Update append revision; read model chọn
   snapshot, không âm thầm viết lại decision cũ hay gửi cảnh báo hồi tố như realtime.
3. Duration dùng monotonic với boot/clock domain; correlation/query lịch dùng UTC mapping
   và uncertainty. `[begin,end)` là quy ước interval; timezone/shift thuộc query/calendar
   policy. Clock jump hoặc multi-camera skew làm kết quả phụ thuộc thời gian thành uncertain.
4. Image pixel, normalized image, ground-plane mét và geo CRS là các space khác nhau.
   Point cần anchor semantics (ví dụ footpoint), transform/calibration revision. Không tính
   khoảng cách giữa hai camera nếu chưa có mapping hợp lệ.
5. Attribute có typed value/candidates và trạng thái `known`, `unknown`, `not_observable`,
   `not_supported`, `expired` hoặc không thu theo policy; missing không mặc nhiên `false`.
   Không ép tuổi/gender, PPE, plate vào cùng một chuỗi label không schema.
6. Relations/attribute tồn tại theo interval. Màu tại lúc crossing khác màu từng thấy; biển
   số đọc trước/sau phải gắn đúng xe và valid interval. Ngưỡng model score không được coi
   như xác suất thống nhất giữa hai model/version khi chưa calibration.
7. Reconnect/gap không tự là crossing, mất object hay zero occupancy. Không nối thẳng
   trajectory qua gap dài rồi khẳng định đi qua mọi polygon trên đường nối đó.
8. Track split/merge/ID switch, observation loss và correction phải có representation;
   operator xác nhận identity/event là decision riêng có actor/reason, không sửa raw model output.

### 6.6. Chiến lược thu nhận và retention theo nghiệp vụ

| Tầng | Giữ dữ liệu | Chính sách |
|---|---|---|
| Live state | Latest tracks/annotation, bounded temporal windows | RAM có TTL; mất khi reboot là semantics rõ, không dùng làm journal |
| Detail history | Samples/trajectory/selected model details | Theo time hoặc distance/error-bound; budget từng source/usecase; retain boundary points |
| Search facts | Segments, attributes/relations, passages, decisions/events | Ghi khi thay đổi/đóng interval; snapshot định kỳ cho interval dài; đủ cho query chính |
| Aggregates | Count/dwell/flow/density/speed buckets | Retention riêng, có coverage/revision và rebuild limit |
| Evidence/identity vectors | Media FW; gallery/index và embedding metadata AI | Retention/quota riêng theo workload; export theo entitlement, không copy model tensor/gallery snapshot |
| Debug/quality fixtures | Tensors, crops, pose sequences, VLM context chi tiết | Opt-in, bounded, access-controlled; không bật production vĩnh viễn |

Đặc biệt với traffic: sampling 1 Hz đủ minh họa đường đi chưa chắc đủ speed/stop-line/
near-miss. Tính fact quyết định từ chuỗi chất lượng phù hợp **trước** khi downsample; giữ
supporting samples hoặc evidence reference theo policy để giải thích lại. Simplification
có error bound nhưng không bảo đảm tái tạo mọi rule tương lai; query hồi tố phải trả mức
precision, gaps và có thể không kết luận được.

Muốn vẽ zone mới và hỏi lịch sử: cần trajectory đủ chi tiết còn retention. Chỉ giữ crossing
của zone cũ không làm được việc đó. Nếu cần rerun model mới phải còn media và compute budget,
quyền replay; đây là job mới có provenance, không phải SQL tự khôi phục dữ liệu đã bỏ.

### 6.7. Query catalog: nghiệp vụ phải được thiết kế trước

Mức triển khai: **E** là API edge nền tảng cần chuẩn bị; **C** là có điều kiện theo producer,
calibration/quyền; **F** là federation/center cho nhiều thiết bị. Ký hiệu không nói capability
đã tồn tại trong code. Query nào chưa qualify thì capability API phải báo chưa hỗ trợ.

| ID | Câu hỏi/đầu ra | Dữ liệu bắt buộc và semantics chính | Mức |
|---|---|---|---|
| Q01 Tìm đối tượng theo thuộc tính | Người áo đỏ, xe màu trắng, hành lí loại X trong thời gian/vùng | D03/D05/D07; AND/OR typed, thời điểm hiệu lực, unknown, class/model ontology | E+C |
| Q02 Thuộc tính tại sự kiện | Người áo đỏ lúc qua cổng A hoặc mang vật tại episode | D05/D06 join D07/D10 đúng time/subject, không latest attribute | E+C |
| Q03 Chi tiết hành trình | Track này ở đâu theo timeline; last seen; media liên quan | D03/D04/D15/D17; segment/gap/coordinate space, không fabricate điểm | E |
| Q04 Tuyến/hướng qua vùng | Qua A rồi B trong Δt, không qua C trong khoảng được quan sát, enter/exit/line crossing | D07; thứ tự/recross/coverage và cùng identity scope; absence chỉ xác nhận khi coverage đủ | E |
| Q05 Hiện diện/lưu lại | Ở polygon lúc t, overlap khoảng t0–t1, dwell vượt rule, loitering | D07/D04/D17; open intervals, clipped duration, observed vs inferred | E |
| Q06 Quan hệ đối tượng | Ai/track nào mang/gần/đi cùng vật, plate gắn xe nào, participants episode | D06/D16; temporal overlap/confidence, association không là ownership | E+C |
| Q07 Liên camera/thiết bị | Ứng viên cùng người/xe/vật, last seen toàn hệ thống | D16 + descriptors có quyền + topology/time bounds; không global identity chắc chắn | C+F |
| Q08 Tìm/đối soát sự kiện | Type/severity/state/zone/time, active/ended, duplicates, evidence pending | D10/D15; episode khác notification/update, occurred khác reviewed | E |
| Q09 Nhận diện/watchlist | Identity candidate xuất hiện lúc nào, thuộc list nào khi đó | D11 + gallery/watchlist revision; confidence, review, quyền nhận diện | C |
| Q10 Điểm danh | Theo roster/session/ca: có mặt/vắng/chưa đủ dữ liệu, first/last, correction | D09/D11/D17; encounter dedup, schedule timezone, identity decision và coverage | C |
| Q11 Vật thiếu/thất lạc | Last seen, stationary/removed interval, ai ở gần trước/sau | D03–D07/D10; missing vs occluded vs out-of-view; reported-loss input | C |
| Q12 Tìm biển số | Exact/normalized/prefix hoặc pattern được hỗ trợ, time/source/class/color filters | D12/D05/D07; raw vs consensus, pattern cost limit và access-domain authorization | C |
| Q13 Fuzzy OCR/plate history | Tìm gần giống, alternatives, đọc lại từng lần để xác minh | D12; normalization/alphabet/version, candidate scoring; fuzzy không exact match | C |
| Q14 Thống kê flow/heatmap | Lượt/unique track, in-out, density/dwell/occupancy theo bucket/zone/class | D07/D14/D17; unit/counting basis, baseline, coverage, exact/approximate | E+C |
| Q15 So sánh/xu hướng | Các khung giờ/ngày/zone, demographic/PPE/event rate, histogram | D14 + definitions/version/denominator; không so raw count trên coverage khác nhau | E+C |
| Q16 Tìm bằng ảnh/text embedding | Top-k đối tượng/đồ tương tự, có filter time/source/class | Authorized descriptors/index + D03; embedding space/version, ranking và recall, không nhận dạng chắc chắn | C+F |
| Q17 Tìm cảnh báo VLM | Rule/prompt version, structured claim, supporting frames, approved/rejected | D13/D10/D15; free-text search optional, không tự biến prompt thành SQL/action | C |
| Q18 Lưu lượng xe | Passage count theo class/lane/direction/time | D07/D05/D14/D17; counting line revision, loại bỏ duplicate passage | C |
| Q19 Chuyển động/lane | Rẽ trái/phải, đổi làn, ngược hướng, sai movement | D01/D07/D10; lane/topology/rule revision và unknown lane | C |
| Q20 Dừng/đỗ/parking session | Xe dừng/đỗ bao lâu, entry chưa exit, occupancy bãi | D07/D09/D12/D14; match ambiguous/open session, dwell không mặc nhiên vi phạm | C |
| Q21 Phép đo tốc độ | Vượt ngưỡng policy, phân bố tốc độ, đo lại vì calibration đổi | D08/D01/D17; unit/method/error/validity, new computation revision | C |
| Q22 Queue/congestion | Queue length, stop time, lane occupancy, flow giảm theo thời gian | D08/D14/D17; phương pháp và baseline; không đồng nhất số bbox với mét queue | C |
| Q23 Candidate vi phạm/incident | Stop-line lúc đèn đỏ, wrong-way, incident và evidence để review | D07–D10/D15; signal/camera clock validity, rule và reason trace | C |
| Q24 OD/travel time/headway | Từ nhánh A đến B, thời gian hành trình, khoảng cách thời gian giữa lượt | D07/D16/D01; matching scope, censoring, clock uncertainty; center khi nhiều thiết bị | C+F |
| Q25 Geometry hồi tố | Vẽ polygon/line mới, tìm trajectory lịch sử cắt/ở trong đó | D04/D01/D17; exact geometry sau candidate filter, sampling error/gaps; asynchronous khi lớn | C |
| Q26 Giải thích/tái lập quyết định | Vì sao event/identity/violation được tạo, biết gì ở revision r | Provenance D01/D05–D13, model/rule versions, review/audit; raw evidence còn hay hết retention | E |
| Q27 Completeness/health | Nguồn nào thiếu dữ liệu, query này bao phủ bao nhiêu, backlog theo sink | D17/D18; disabled/fault/lost/expired tách với zero result | E |
| Q28 Export/report | Xuất kết quả/paging/job, phát lại metadata theo snapshot | Authorized projection + query snapshot/manifest; bounded bytes/time, audit và cancellation | E+C |
| Q29 Sửa/xóa/retention | Sửa decision, xóa record/phạm vi có quyền, purge status các bản sao | Revision/tombstone + D18; không erase audit tùy ý, cloud/media receipts riêng | E |
| Q30 Recompute/backfill | Dựng lại aggregate/read model hoặc rerun rule/model trên lịch sử | Retained dependencies/version/job budget; output provenance mới, không alarm realtime lại mặc định | C+F |

Đây là họ query, không 30 endpoint buộc tạo ngay. Có thể gom vào search, timeline,
aggregate, explain, job và admin APIs. Mỗi query khai báo filter/projection hỗ trợ, required
fields, phạm vi source/identity, quyền, lịch sử còn truy vấn được, budget và độ chính xác.

### 6.8. Query contract: kết quả đúng quan trọng hơn câu SQL

Request tối thiểu: query kind/version, source set, time range/time basis, entity/feature
filters typed, scene/rule revisions hoặc chế độ historical/current rõ, attribute validity
mode, quality threshold policy, projection, sort, page/cursor, deadline và principal scope.

Response phải có:

- record/entity/event refs phù hợp, snapshot/query ID và next cursor ổn định;
- dữ liệu đã ẩn theo quyền; không lộ field ngoài access domain qua count/filter/sort hay autocomplete;
- coverage/watermark, gaps, retention boundary, quality/unknown và result mode
  `complete`, `partial`, `approximate`, `unsupported` hoặc `budget_exceeded` theo contract;
- model/ontology/calibration/rule revisions quan trọng và scope `local`/`federated`;
- aggregation basis/denominator và evidence availability, không hứa mọi hit có video.

`complete` chỉ nghĩa đã đọc đủ phạm vi dữ liệu hợp lệ theo snapshot, không nghĩa detector
phát hiện mọi đối tượng thực tế. “Không có event” khác “feature tắt”, “không có data” và
“record đã bị purge”. Query phủ nhiều version phải normalize bằng mapping được duyệt hoặc
trả nhóm riêng; không đổi label index sang nghĩa mới âm thầm.

Paging bằng keyset cursor + snapshot lease có TTL; query hết snapshot phải trả lỗi ổn định,
không lặp/mất trang vì ingest/compaction. Query lớn chuyển async job có cancel/deadline,
scan/memory/temp-disk budget. Không giữ read transaction vô hạn khiến WAL tăng mãi.
UI không gửi SQL tùy ý; VLM/text search cũng qua typed query plan, allowlist và authorization.

### 6.9. Từ query catalog đến read models và index

Canonical data dùng subject/attribute/relation/interval tổng quát; **không dùng một bảng
EAV hoặc JSON duy nhất cho mọi query nóng**. Tạo projections typed theo workload đã chốt,
đồng thời giữ provenance/correction để rebuild. Không duplicate tensor/raw observations
cho mỗi projection.

| Query family | Projection/index ứng viên | Hạn chế phải đo |
|---|---|---|
| Q01/Q02/Q04/Q18 | `passage_search` + selected attribute-at-passage; composite equality prefix rồi time range | Một index không tối ưu mọi tổ hợp attrs; kiểm selectivity/write amplification |
| Q03/Q05/Q25 | Track/source/time index, segment bounds, optional spatial index + trajectory chunks | Bounds chỉ lọc ứng viên; exact geometry không nối qua gap tùy ý |
| Q06/Q07/Q24 | Relation/association adjacency theo subject/object + interval; passage sequence | Không cần graph DB ngay; high fan-out/cross-device có thể vượt edge budget |
| Q08/Q23/Q26 | Event type/source/time/state index + decision/provenance refs | Active events và lịch sử có workload khác; correction phải cập nhật index |
| Q09/Q10 | Identity/session/time encounters + attendance projection | Quyền đặc biệt, dedup theo session policy không một frame một người |
| Q12/Q13 | Plate exact/normalized/time + candidate index cho pattern/fuzzy được duyệt | B-tree thường không giải mọi wildcard/edit-distance; fuzzy cần bước verify và budget |
| Q14/Q15/Q21/Q22 | Buckets/histograms theo source/zone/lane/time/class/method | Distinct/sketch và percentile không cộng như count; baseline/coverage phải còn |
| Q16/Q17 | Optional vector/text index có filter và schema version | Không thay index time/zone/plate; embedding model mismatch phải reject |
| Q27–Q30 | Coverage, manifest, outbox, audit và job indexes | Query/export không được làm đói ingest/evidence lane |

Ví dụ “áo đỏ tại cổng A” trở thành Q02 trên passage + attribute validity. Cùng cơ chế xử lý
“xe tải trắng vào lane 2” mà không thêm `shirt_color` vào mọi loại event. Projection có
thể có typed `upper_clothing_color` hoặc `vehicle_color` theo subject type; phải pin schema
và freshness, không dùng một `color` thiếu nghĩa.

Nếu filter thuộc tính hiếm thay đổi thường xuyên, benchmark normalized interval joins
với selected projections; không tạo sẵn tích Descartes mọi màu/loại/zone/model. Vector search
chỉ là optional capability cho similarity; không dùng gallery FR hiện tại như một index
chung cho cả áo, xe và biển số.

SQLite hỗ trợ multi-column/covering index; R-tree hỗ trợ tìm spatial bounds. Những khả năng
này cho phép thử các query chọn lọc mà không quét mọi frame, nhưng không bảo đảm SLO khi
chưa benchmark. Time filter vẫn giữ integer timestamp riêng; không ép UTC nanoseconds vào
spatial float index làm mất precision. Xem [query planner](https://www.sqlite.org/queryplanner.html)
và [R-tree](https://www.sqlite.org/rtree.html).

### 6.10. So sánh storage khách quan, chưa khóa vào một format

SQL là ngôn ngữ; Parquet là format; DuckDB/SQLite là engine. Không có lựa chọn nào tự
hiểu trajectory, identity hoặc rule traffic. Đánh giá trên Q01–Q30 và retention workload.

| Phương án | Mạnh ở đâu | Điểm yếu/chi phí | Chọn khi |
|---|---|---|---|
| SQLite-only + typed projections | Transaction, selective lookup, outbox, ít dependency; phù hợp edge nhỏ | Single-writer budget, large scans/index growth/checkpoint có thể ảnh hưởng ingest | Retention và query benchmark đạt; đây là baseline tối thiểu để đối chứng |
| DuckDB local tables và/hoặc Parquet | Columnar analytics, batch export và historical scan | Không mặc định thay transactional writer/point-update/outbox; concurrency/RAM/latency phải qualify | Analytics/retention là tải chính, hoặc worker đọc lịch sử tách biệt |
| Hybrid transactional facts/index + immutable columnar history | Nóng chọn lọc nhanh, lịch sử nén/scan; boundary rõ | Hai đường read/retention/correction; manifest/snapshot/dedup phức tạp hơn | Baseline không đạt dung lượng/scan nhưng có budget vận hành hybrid |
| KV/LSM engine + index tự quản | Kiểm soát key layout và write-heavy access patterns | Phải tự xây secondary indexes, transactions/query semantics, compaction tuning và recovery | Chỉ sau benchmark chứng minh engine hiện có không đạt; không chọn vì “binary nhanh” |
| Server search/analytics/spatial engine | Multi-device joins, heavy analytics và scale lớn | Hạ tầng/mạng/ops, không thay khả năng local offline | Q07/Q24/Q30 toàn hệ thống hoặc historical workload vượt edge |

**Đề xuất có điều kiện:** bắt đầu bằng canonical schema, bounded ingest, SQLite facts/index/
outbox và API Q01–Q08/Q14/Q27 làm baseline. Bật producer nào thì mới quảng bá query tương
ứng. Spike hybrid với Parquet/DuckDB trên cùng dataset; chọn nếu chứng minh lợi ích đủ bù
chi phí. Không triển khai custom storage engine, graph DB hay tất cả loại index trước.

Parquet reader của DuckDB có filter/projection pushdown và statistics-based skipping;
file layout/row groups vẫn quyết định lượng đọc. Không có global trajectory index chỉ
vì file là Parquet. Tham khảo [DuckDB Parquet](https://duckdb.org/docs/current/data/parquet/overview).

Native C++ không buộc dùng Arrow: AI APP có thể gọi DuckDB C API qua adapter. Arrow C++
reader/writer là lựa chọn khác nếu cần record-batch pipeline; không nạp cả Arrow và DuckDB
chỉ để ghi cùng file. Pin release, cross-build eSDK và đo package/RSS; tài liệu Arrow hiện
đòi C++20 trong khi LACAI là C++17, cần dependency boundary/version được duyệt, không tự
nâng standard dự án. Xem [DuckDB C API](https://duckdb.org/docs/current/clients/c/overview),
[Arrow Parquet](https://arrow.apache.org/docs/cpp/parquet.html) và
[Arrow build](https://arrow.apache.org/docs/developers/cpp/building.html).

### 6.11. Durability, query snapshot và hot/cold lifecycle

1. Ingest validate identity/schema/bounds/quyền, dedup record/revision. Một transaction ghi
   canonical fact, hot projection hoặc projection-work item, và per-sink outbox cần thiết.
   Nếu projection async, công bố projection watermark; durable record chưa chắc query thấy ngay.
2. Durable receipt chỉ sau commit theo fsync/storage policy đã ký. RAM queue acceptance,
   broker ACK, FW ACK và local durability là các mức khác nhau.
3. Archive theo immutable record sequence/revision; ghi temp file, finalize footer, sync,
   same-filesystem rename/directory sync, rồi publish manifest transaction.
4. Manifest ghi file ID/hash/schema, source/time/sequence coverage, stats và generation.
   Crash trước publish tạo orphan; không query mọi file bằng glob như đều đã commit.
5. Query lấy hot/cold snapshot và projection watermarks nhất quán; overlap dedup theo ID +
   revision, tombstone/correction có precedence. Event time không thay ingestion sequence
   vì late records có thể rơi vào partition cũ.
6. Compaction/reindex publish generation mới, retire sau reader drain; cursor lease hữu hạn.
   Chỉ purge hot detail sau archive verification và các consumer bắt buộc được bảo toàn.
7. Nếu giữ index/facts suốt search retention nhưng detail ngắn hơn, query chính vẫn trả hit
   còn drill-down phải báo detail/evidence expired. Không quảng bá replay khi không còn input.

Partition theo time và bounded source grouping; không file/partition cho từng track,
plate hay identity. Sort/row-group/compression/flush-age từ workload, không một bộ hardcode.
Long-running track cần checkpoint/chunk incremental, không chờ track kết thúc mới thấy data.

SQLite WAL có writer/checkpoint/durability trade-offs, không mặc định miễn chi phí disk;
đo với policy thực ship, không benchmark chế độ sync yếu rồi hứa mất điện không mất record.
Tham khảo [SQLite WAL](https://www.sqlite.org/wal.html).

### 6.12. Aggregates, correction và truy vấn hồi tố

- Count contribution có ID/dedup key riêng; duplicate notification không làm tăng số đếm.
  Passage count khác unique local tracks, khác unique người/xe thật.
- Occupancy cần initial state và detection coverage; `in-out` sau reset không tự là số người
  hiện có. Dwell/heatmap cần duration weighting, không cộng frame count phụ thuộc FPS.
- Average giữ numerator/denominator; speed mean phải khai báo sampling basis. Percentile và
  unique count phải giữ mergeable representation hoặc recompute; approximate có sai số rõ.
- Late attribute/plate consensus có thể sửa projection; late End sửa duration bucket bằng
  contribution revision, không cộng lại toàn episode. Dataset window có finality/late policy.
- Recompute với rule/calibration/model mới tạo computation revision, không thay provenance
  cũ. Không tự phát lại alarm, attendance action hay yêu cầu clip đã tạo trong backfill.
- Query scene hiện tại trên dữ liệu scene cũ là chế độ explicit, có transform hợp lệ; mặc
  định dùng historical revision. Không retroactively đổi lane của toàn bộ passage.

### 6.13. Quyền, retention và policy là một phần data contract

Identity, embedding và biển số được lưu như metadata thông thường trong thư mục metadata
được cấu hình; P2 không yêu cầu encryption-at-rest hoặc protected directory riêng để activate.
Phân lớp access domain ít nhất: aggregate, object/trajectory, visual attributes, identity,
plate, vector, media và operational audit. Đây là phạm vi entitlement/output, không phải
phân loại nhạy cảm. AI APP kiểm quyền ingest, search/filter/projection/export/retry; BSP+FW
cấp principal/grant và quản lý media access.

Cloud export vẫn là capability riêng: feature được bật không tự cấp quyền xuất. Deployment
có thể bổ sung ACL/encryption/key policy nghiêm ngặt hơn, nhưng đó là policy cấu hình và không
được hardcode làm điều kiện chung của metadata architecture.

Purge đi qua hot rows, cold files/compaction, derived/vector indexes, caches, outbox và center
receipts theo policy. Media delete do BSP+FW thực thi, AI APP cập nhật reference. Không nói
“đã xóa” chỉ vì xóa một SQLite row; audit chỉ giữ bounded facts cần cho lifecycle.
Xóa/revoke và các yêu cầu giữ dữ liệu được giải quyết bằng policy được cấp quyền, có state
và reason; không tự quyết theo feature code. Retention từng loại phải được cấu hình rõ.

### 6.14. Sizing và benchmark gắn với catalog truy vấn

```text
raw_detail_bytes/day = sources × active_tracks × samples/s × bytes/sample × 86,400
facts_bytes/day      = accepted_facts/s × mean_record_bytes × 86,400
offline_spool_bytes  = authorized_export_bytes/s × offline_seconds + retry/index overhead
peak_disk_budget     = retained_hot + cold + WAL + outbox + compaction_temp + reserve
```

Ví dụ sizing, không là default: 16 nguồn × 20 track × 5 Hz × 128 byte ≈ 17,7 GB/ngày
chưa index/WAL/nén. Security đông người và traffic dòng xe liên tục có cardinality/lifetime
khác nhau; cùng FPS không cùng storage load. Đo bytes/record, index amplification và nén
trên dữ liệu thật/synthetic đại diện trước chốt quota.

Benchmark tối thiểu phải có:

- Security: các workload nhóm S01–S18, màu phổ biến/hiếm, crowd, nhiều relations, recognition
  và VLM chỉ khi producer được bật; không dùng một sample áo đỏ làm đại diện mọi query.
- Traffic: vehicle passages liên tục, OCR nhiều lần/xe, high-cardinality plate, lane transition,
  signal/clock/calibration invalid, long stopped objects và corrections đến trễ.
- Hot/cold/cold-cache, query nhiều ngày, đồng thời ingest/evidence/compaction/Kafka offline;
  load tăng đến admission limit, restart/disk-full và source gap.
- Q01–Q30: required dataset/producer, retention horizon, concurrency, p50/p95/p99,
  rows/bytes scanned, CPU/RSS/temp disk, exactness/recall khi phù hợp, timeout/cancel và
  mức suy giảm FPS/event ACK khi query chạy.

AI APP lead cùng BSP+FW chốt ngân sách thành config/SLO theo profile camera và AI Box.
Query-heavy/center workload có budget riêng. Disk đầy phải từ chối durable acceptance có
reason/degraded policy, không overwrite alarm đã ACK; live samples có thể drop theo policy
và coverage counters. Không vừa bounded storage vừa bảo đảm lịch sử/offline vô hạn.

### 6.15. Gói triển khai đầu tiên và tiêu chí xong mục 6

| Bước | AI APP phải giao | BSP+FW phải giao | AI Model phải giao |
|---|---|---|---|
| M1 Catalog và contract | Map 18 mục → stable usecase IDs; D01–D18/Q01–Q30 registry, field units/null/quality/access-domain, C04/C06/C09 drafts | Source/time/volume/auth/media facts và giới hạn thiết bị; review wire/API | Ontology/schema/quality/unsupported của từng producer; package/golden availability |
| M2 Nền dùng chung | Envelope/identity/interval/revision/coverage validators; bounded ingest, query facade và synthetic fixtures person/vehicle/scene | Fault-capable mock RAW/time/evidence/control + quota/clock failure fixtures | Golden observations/attributes/plate candidates; không cần chờ đủ 18 model để test storage |
| M3 Vertical security + traffic | Person attribute-at-passage/timeline/count và vehicle/plate passage dùng cùng core schema; local facts/index/outbox | Query UI/client skeleton, evidence receipt/replay; không SQL direct | Color/person/ANPR kit nào sẵn thì qualify; chưa có dùng fixture và ghi rõ logic-only |
| M4 Storage decision | A/B baseline vs hybrid với query catalog, ADR lựa chọn, retention/correction/purge tests | Flash/RAM/CPU/thermal quota và power-cut qualification | Review sampling/retention có giữ đủ dữ liệu cho accuracy/trace requirements |
| M5 Mở rộng theo dependency | Attendance/relations/VLM/measurements/federation từng feature gate | Signal/calibration/profile/center deployment theo capability được giao | Kits/quality reports theo từng feature, không claim từ shared detector |

Definition of done: mỗi usecase đã có dữ liệu và query mapping, mỗi query có semantics,
producer prerequisites, index/access path ứng viên, quyền, SLO và golden expected results;
unsupported/partial/expired được test như first-class outcomes. Core schema chấp nhận
person/vehicle/baggage/scene event bằng fixtures, không hardcode security-only. Engine
chỉ được chốt sau benchmark; hoàn thành registry không đồng nghĩa 18 usecase đã chạy.

## 7. Kafka và center data lake

### 7.1. Hai đường độc lập từ dữ liệu đã chấp nhận

Local lưu/query và cloud export cùng dùng record identity/schema, nhưng có checkpoint riêng.
Kafka mất mạng không ngăn local query hoặc FW evidence. FW lỗi không chặn Kafka nếu record
được phép xuất; evidence có thể cập nhật trạng thái sau.

Luồng đề xuất:

```text
hot transaction + kafka_outbox
    -> bounded exporter
    -> librdkafka delivery report
    -> broker acknowledgement
    -> mark cloud delivery
    -> center consumer -> durable lake commit -> serving/index
```

`produce()` chấp nhận vào queue RAM không phải broker ACK. Bật idempotent producer với cấu
hình tương thích, giới hạn queue/batch/timeout và theo dõi delivery report. Application
retry khi persistence chưa rõ vẫn cần stable record ID và dedup; producer idempotence
không tự tạo exactly-once xuyên local DB, restart và sink data lake.
Xem [librdkafka delivery semantics](https://docs.confluent.io/platform/current/clients/librdkafka/html/md_INTRODUCTION.html)
và [configuration](https://docs.confluent.io/platform/current/clients/librdkafka/html/md_CONFIGURATION.html).

### 7.2. Contract dữ liệu cloud

- Record envelope: schema/version, device/source/boot/epoch, record ID/revision, event time,
  ingest time, sequence, model/config/policy provenance; payload typed và có size limit.
- Partition key theo phạm vi ordering cần giữ, thường device/source hoặc track/event key.
  Không hứa tổng thứ tự giữa mọi partition; update/end phải có sequence/revision.
- Retry giữ nguyên identity. Center upsert/dedup theo ID + revision; consumer chỉ commit
  offset khi downstream durability thỏa contract. Xử lý late/out-of-order rõ ràng.
- Topic phân theo loại dữ liệu và policy, không topic cho mỗi track. Schema registry và
  compatibility gate trước rollout; tenant separation/TLS/ACL do platform cấp.
- Kafka chở metadata, event và media references. Video/ảnh evidence đi đường upload của
  FW/object storage được duyệt; không nhét clip lớn vào message.
- Center phải có lake writer, catalog/schema evolution, partition/compaction/retention,
  query serving và access control. Broker không thay những thành phần này.
  AI APP chủ trì application/schema; BSP+FW là đầu mối deployment/infrastructure theo
  phạm vi mục 5.2. Trước khi làm R5 phải chốt capacity và người nhận phần server; chỉ
  hoàn thành exporter edge không được báo là đã triển khai center.
- Nếu yêu cầu biết “đã vào lake”, cần receipt/metric sau lake commit. Broker ACK chỉ là
  mức delivery tới Kafka, không chứng minh file đã query được ở center.

### 7.3. Offline, revoke và vận hành

Backlog phải được sizing từ tốc độ record × thời gian offline cho phép; export có retry
backoff/jitter, bandwidth budget và priority. Không đẩy lại cả lịch sử mỗi khi reconnect.
Giữ watermark riêng cho FW, Kafka và archive; cleanup phụ thuộc các đích bắt buộc còn cần
record, không một cờ `delivered` dùng chung.

Kiểm quyền tại ingest chưa đủ: queued export/retry phải được kiểm tra theo chính sách
revocation. Bản ghi bị cấm gửi phải có terminal disposition/audit, không lặng lẽ sửa
revision để gửi dưới quyền mới. Local retention/read permission và cloud export permission
là hai phạm vi khác nhau; revoke không tự có nghĩa xóa mọi dữ liệu nếu policy chưa định nghĩa.

## 8. Event sang FW và tạo evidence

### 8.1. Chọn transport theo semantics

| Cơ chế | Phù hợp | Hạn chế/quyết định |
|---|---|---|
| D-Bus | Config, bật/tắt, query state, control ít tần suất | Không tự sai cho mọi event; nhưng không chọn làm bulk metadata bus hoặc cơ chế durable alarm |
| UDS `SOCK_SEQPACKET` | Event records, ACK/receipt, reconnect giữa process cùng máy | Chọn mặc định cho event; vẫn cần journal/dedup/version/timeout |
| UDS `SOCK_STREAM` | Khả năng tương thích nếu seqpacket không có | Cần length framing, partial read/write và giới hạn parser |
| SHM ring + notification | Live annotation/metadata tần suất cao, latest-wins | Chỉ thêm sau khi đo bottleneck; phức tạp ABI, crash recovery và ownership |

Linux UDS seqpacket giữ message boundary và thứ tự; peer credentials có thể hỗ trợ kiểm tra
đầu kia. Đây không phải bảo đảm bên nhận đã lưu event. Socket permissions, `SO_PEERCRED`,
deployment peer identity và authorization vẫn phải cấu hình/validate.
Tham khảo [unix(7)](https://man7.org/linux/man-pages/man7/unix.7.html).

Không gửi native C++ struct, pointer, `std::string` hay layout `std::atomic` trực tiếp làm
wire contract. Chọn schema serializer có version, bounded decoder, unknown-field policy,
max record size và endianness rõ. Protobuf/FlatBuffers là ứng viên để benchmark; không
cần tự phát minh ABI chỉ để giảm vài byte event.

### 8.2. Tận dụng contract hiện có nhưng không đánh đồng acceptance

[Feature event dispatch](../../architecture/feature_event_dispatch.md) hiện quy định sink
`ok` là đã copy/nhận ownership cần thiết; không có nghĩa FW ACK hoặc đã tạo evidence.
Không được âm thầm đổi ý nghĩa `ok` thành ghi disk đồng bộ ngay trên frame thread.

Đề xuất thêm delivery owner ngoài hot loop:

1. AI tạo event ID ổn định, validate fields và gate quyền, chuyển bản copy metadata nhỏ vào
   bounded queue. Không giữ tensor/frame chỉ để chờ network/disk.
2. Edge data writer journal event + FW outbox. Trả receipt `durably_accepted` qua đường
   phản hồi; đến đây mới có bảo đảm replay sau process restart theo storage contract.
3. Trước durable receipt, AI còn trách nhiệm retry theo cùng ID; nếu toàn process mất điện
   trước commit thì có cửa sổ mất event. Phải công bố cửa sổ này, không gọi RAM queue là
   “không mất event”. Nếu sản phẩm không chấp nhận, acceptance path phải chờ durable
   completion bất đồng bộ và có upstream replay/checkpoint phù hợp.
4. Worker gửi FW command từ outbox, giữ quyền source/feature/evidence policy hợp lệ khi retry.
5. FW dedup bằng request ID/revision, ghi durable inbox/job trước ACK. ACK mất thì gửi lại
   cùng request không tạo clip mới.
6. FW trả trạng thái tạo media và cuối cùng `ready`, `failed` hoặc `partial`; edge cập nhật
   `evidence_ref` và tạo cloud update riêng.

Tách lane priority cho alarm khỏi bulk observation; compaction/query không được giữ lock
hay chiếm toàn bộ worker của lane này. Nếu latency fsync vượt SLO pre-roll, cần thay storage
budget hoặc contract acceptance, không đẩy thẳng RAM rồi vẫn gọi là durable.

### 8.3. Envelope và state machine

| Nhóm | Trường/semantics cần có |
|---|---|
| Wire | protocol major/minor, schema ID/version, payload length, message kind |
| Identity | event ID, event revision/sequence, request ID, source/boot/epoch, feature ID |
| Thời gian | capture monotonic/domain, UTC mapping/uncertainty, event begin/end |
| Provenance | model, config, zone và policy revisions; track/observation references |
| Evidence intent | capture source/profile reference, pre/post duration, priority, deadline, authorized media scope |
| Delivery | producer generation, sequence, ack level, retryable/terminal reason |
| Receipt | evidence/media ID, actual interval, missing ranges, durable disposition |

```text
event:     started -> updated* -> ended / interrupted
delivery:  queued -> durably_accepted -> fw_accepted -> terminal
evidence:  requested -> recording -> finalizing -> ready / partial / failed
```

Ba state machine không phải một cờ. Event đã `ended` nhưng media đang finalizing là bình
thường. `fw_accepted` chưa chứng minh có file. Update/End trước Start do replay/out-of-order
phải có rule xử lý; dedup key cần chứa revision/command kind, không chỉ event ID duy nhất
khi có nhiều update cho cùng event.

Event ID có thể dùng UUID hoặc device/boot/sequence có collision contract; request ID dẫn
xuất xác định từ event + loại evidence + revision. Kích thước/thuật toán phải chốt trong
schema, không quyết định rải rác ở feature.

### 8.4. Pre-roll và các lỗi cần thiết kế trước

- FW cần encoded prebuffer đang hoạt động; nếu encoder chỉ chạy khi có viewer thì có thể
  không còn dữ liệu trước event. Evidence subscriber/demand phải độc lập viewer UI.
- Dung lượng prebuffer phải tính bitrate, GOP/keyframe, pre-roll, thời gian phát hiện,
  journal và delivery latency. Chỉ tính số giây pre-roll là thiếu.
- Evidence source có thể khác RAW inference profile; cần source registry/clock mapping,
  không đoán `third` hay ID nội bộ DB từ tên camera.
- Khi không đủ lịch sử lúc startup, trả actual interval/partial, không báo đủ clip.
- Storage full, FW restart, clock jump, source profile đổi, event kéo dài, duplicate Start,
  lost ACK, End bị thiếu: mỗi trường hợp phải có disposition và recovery test.
- Nếu FW đã lưu media nhưng receipt mất, query/reconcile bằng request ID; không tạo lại
  evidence vô hạn. Media permission được kiểm tra độc lập metadata permission.

### 8.5. Khi nào mới dùng SHM

SHM đáng cân nhắc cho batch bbox/keypoints/live annotation nếu UDS copy/serialization đã
được chứng minh là bottleneck. Không lấy video frame rate nhân lên để mặc định alarm cũng
cần SHM. Nếu triển khai phải có:

- fixed-capacity layout versioned, sequence/generation và size validation;
- writer/reader ownership, memory ordering, wraparound, crash/stale-consumer protocol;
- eventfd hoặc notification phù hợp, lost wakeup handling;
- overflow policy riêng cho live và event; không overwrite alarm chưa được nhận bền vững;
- pinned IPC SDK/ABI và test hai process, không chia sẻ STL objects;
- xử lý descriptor/FD transfer có lease nếu sau này có payload lớn.

## 9. Vì sao app FW có thể dùng ít ARM CPU hơn

### 9.1. Không so sánh hai con số khác workload

| Tiêu chí | LACAI | App FW đang tham chiếu |
|---|---|---|
| CPU đã ghi trong hồ sơ | 95,38–96,13%; cửa sổ nóng 106,36% một logical CPU | README ghi khoảng 25% một core ở live test; 15–20% là số người dùng cung cấp, chưa tái lập |
| Model/workload | Có person cùng FD + alignment + EdgeFace + matching trong các cửa sổ nêu trên | Face/person/firesmoke; README nói EdgeFace phase 3 chưa thực hiện |
| Cadence cấu hình | Hồ sơ có person khoảng 25 FPS, FD+FR khoảng 20,78–24,60 FPS | `artifact/ai_app.json`: face 15 FPS, person 10, firesmoke 10 |
| BSP trong bằng chứng | Target dự án QCS6490 / Qualcomm Linux 1.8 | README lịch sử ghi QLI 1.6, QAIRT 2.43 |
| Đường vào | Có validation qua FW simulator; chưa đồng nhất mọi bài với released camera | Live test README dùng RTSP decode; camera sensor chưa test trong mốc đó |
| Demand gating | Đường renderer production cần hoàn thiện wiring | Cơ chế có, nhưng config được đọc đặt `ring_demand_gating=false` |

Không coi FPS stream 30 là mỗi model infer 30 lần/s. Cũng không so CPU chia cho 8 core
với CPU của process theo một logical CPU. Số 106% là có thể xảy ra vì process nhiều thread.
Các kết quả LACAI xem tại [hồ sơ FR](../../testing/face_recognition_production_validation.md).

### 9.2. Cơ chế tiết kiệm CPU quan sát được trong app FW

| Cơ chế | Nơi đọc trong `application/ai_app` | Vì sao hữu ích |
|---|---|---|
| Pool rpcmem cho input và output, register một lần | `src/flows/flow.cpp:137–177`, `src/qnn/qnn_model.cpp:62–108` | Giảm allocation/register/copy lặp lại; QNN dùng memhandle hai chiều |
| cDSP preprocess ghi thẳng tensor input | `src/flows/flow.cpp:222–314`, `dsp/src/pre.c` | Scale Y/UV, NV12→RGB, pad/widen ở DSP, không vòng pixel lớn trên ARM |
| cDSP decode/NMS và trả compact results | `dsp/src/post_person_yolov8n.c`, `post_face_scrfd.c`, `post_firesmoke_yolov5.c` | ARM không cần nhận rồi duyệt toàn bộ output tensor để lấy ít bbox/kps |
| Mapping allocation dùng lại | `src/dsp/dsp_buffer_registry.cpp`, `rpcmem_pool.cpp` | Tránh mmap/register mỗi frame khi allocation pool ổn định |
| Compose vào output pool trên DSP | `src/app/application.cpp:334`, `dsp/src/compose.c` | Copy/draw lớn không chạy trên ARM; encoder nhập dmabuf |
| Cadence + mailbox latest-only | `src/flows/flow.cpp`, `src/app/live_loop.cpp`, artifact config | Không infer mọi frame, không tích lũy backlog |
| Optical flow chạy cDSP giữa các detection | `src/dsp/dsp_flow.cpp`, `dsp/src/flow.c` | Duy trì chuyển động giữa inference thưa; cần acceptance độ trôi riêng |

QNN của app này vẫn gọi `graphExecute` đồng bộ. Vì vậy “synchronous QNN” không tự giải thích
CPU 100%; thread có thể chờ DSP mà ít tiêu ARM. Cần phân biệt **thời gian chờ/latency** với
**CPU time thực thi**. Async/worker cải thiện concurrency/control, không tự bỏ memcpy/loops.

README FW báo spike một frame/50 lần: person host CPU 1,13 ms/lần, face 1,71 ms/lần;
preprocess DSP khoảng 8,8–9,1 ms ở input 640². Đây là số liệu lịch sử đơn workload,
không dự báo hiệu năng LACAI. Chính README nói pad/widen còn scalar DSP, chưa phải HVX;
offload giảm ARM nhưng không đồng nghĩa mỗi stage nhanh hơn.

### 9.3. Những phần LACAI còn đưa việc về ARM

- [FastCV processor](../../../src/adapters/qualcomm/gstreamer/vqec_vision_fastcv_processor.cpp):
  còn RGB intermediate và bước widen/copy phía CPU. Tên FastCV không chứng minh mọi bước
  nằm trên DSP hay tensor đã đi thẳng vào QNN memory handle.
- [FastCV aligner](../../../src/adapters/qualcomm/media/vqec_vision_fastcv_aligner.cpp):
  map frame, chuẩn bị RGB/planes, warp/interleave/copy theo ROI. Cần đo riêng khi số face tăng.
- [QNN engine](../../../src/adapters/qualcomm/qnn/vqec_vision_qnn_engine.cpp)
  đã có output rpcmem registration, nhưng đường execute vẫn nhận input byte vector dạng
  RAW client buffer, rồi `memcpy` output workspace/registered buffer sang `tensor_blob`.
  **Không đúng nếu kết luận LACAI chưa dùng memRegister ở đâu cả.**
- Decoder đọc tensors trên ARM; với detector raw tensor lớn, chi phí này đáng profile
  cùng dequantize, decode và NMS, không chỉ nhìn thời gian HTP.
- Renderer copy full NV12 surface; overlay cache và output polling vẫn ở service path.
- Cascade gọi theo face, control loop có polling interval và log/formatting theo result.
  Cần flamegraph/counters để định lượng; không quy mọi tải cho polling khi chưa đo.

### 9.4. Không bê nguyên thiết kế app FW

- DSP kernels/model tensor specs của app gắn với các model/dtype/layout cụ thể.
  Ví dụ RGB→u16 `v * 257` không phải quantization chung cho mọi catalog model.
- Mapping registry dùng recent-use/LRU và trả pointer; LACAI cần active mapping lease,
  allocation generation và completion, không thay bằng heuristic “gần đây chắc còn sống”.
- Event sink app FW bỏ `Updated`, bounded queue drop-oldest, worker gọi media backend.
  Có retry cùng command nhưng không phải durable event log. Không dùng chính sách đó
  cho alarm bắt buộc không mất; `Stop()` vẫn join worker sau drain window.
- App FW vẫn compose/encode, nên CPU thấp không phải đơn giản do đã chuyển encode ra FW.
- Artifact config có embedding enabled nhưng README nói parsed/not consumed;
  không lấy một flag làm chứng cứ recognition hoạt động.
- README cảnh báo FastRPC debug logging qua `.farf` làm tăng overhead; chỉ là yếu tố cần
  kiểm tra trong benchmark, không có chứng cứ đây là nguyên nhân CPU LACAI hiện tại.
- Ring/profile/config trong checkout FW mới khác baseline đang pin của LACAI.
  Không sao chép ring layout/capacity/default endpoint rồi gọi là compatible.

## 10. Offload DSP nhưng giữ multiplatform

### 10.1. Hướng execution đề xuất

```text
RAW frame lease
  -> image_processor_port
     Qualcomm adapter: retained/imported mapping -> cDSP preprocess -> input tensor lease
  -> inference_graph_port
     Qualcomm adapter: QNN HTP on registered input/output buffers
  -> model decoder boundary
     Qualcomm adapter: cDSP decode/NMS -> compact neutral detections/keypoints
  -> neutral tracker / attribute / feature
  -> authorized bounded output
```

Face cascade dùng đúng retained source frame: DSP crop/align/normalize theo contract,
registered input cho embedding model, output embedding có kích thước nhỏ chuyển sang
matcher khi hợp lý. Không chạy mọi secondary model trên mọi ROI mỗi frame; quality,
cooldown, change detection, max ROI và fairness là policy cấu hình đã validate.

Không đưa SQL, Kafka, D-Bus hay toàn bộ feature state machine lên DSP. Những phần đó có
I/O/control/branching và semantics thay đổi; ARM vẫn phù hợp. Ưu tiên DSP cho pixel/tensor
loops đủ lớn để bù FastRPC overhead. Không gọi RPC mỗi pixel hoặc mỗi candidate.

### 10.2. Mở rộng contract memory đúng chỗ

Không tạo thêm framework song song khi các port hiện có có thể mở rộng. Nhưng contract
đang bắt buộc byte vector CPU sẽ cần revision để hỗ trợ leased tensor result. Các vai trò
dưới đây là đề xuất, chưa phải tên type/API đã đăng ký:

| Vai trò | Điều bắt buộc |
|---|---|
| Tensor buffer lease | shape/dtype/layout/strides/quantization, capacity, owner, memory domain, generation |
| Optional CPU mapping | access mode, cache-sync scope, bounded map/unmap; không buộc mọi buffer map được |
| Opaque backend memory | Token không lộ QNN/FastRPC/Gst type sang neutral layer |
| Completion | Job/ticket chứng minh reader/writer hardware đã xong; timeout chỉ hết thời gian chờ |
| Result owner | Giữ output lease đến khi decoder/result consumer xong; không reuse workspace sớm |
| Capability | Supported operations, tensor profiles, memory import/export và sync behavior thực có |

Một output buffer không thể vừa đưa view ra ngoài vừa tái sử dụng ngay cho inference kế
tiếp. Cần pool đủ số result in flight hoặc backpressure cho đến `take_result`/release.
Chuyển từ copy sang view phải review cùng
[application composition](../../architecture/application_composition.md), không chỉ sửa
`memcpy` thành pointer.

Pool/mapping:

- Allocate/register ở activation, capacity từ admission; không allocation vô hạn trong frame loop.
- Cache theo allocation identity + generation + layout + device/context, không theo số FD.
- Active job giữ lease; chỉ evict/unmap/deregister khi reader count và hardware completion
  cho phép. Stop/disconnect/close FD không là completion.
- Cache coherency/fence semantics phải có BSP/SDK evidence. “Cùng FD” không tự chứng minh
  zero-copy hay CPU/device nhìn dữ liệu mới nhất.
- DSP/HTP reset cần quiesce/quarantine handshake; không trả RAW ACK giả để unblock producer.

### 10.3. Biên adapter đa nền tảng

- Neutral layer mô tả **operation và semantics**, không có enum “phải dùng Qualcomm DSP”.
- Qualcomm adapter chứa FastRPC IDL/stub, skel loading, rpcmem/ION, QNN memhandle,
  FastCV/HVX và power votes. Không rải tên thư viện/property trong runtime/features.
- Nền tảng khác hiện thực bằng image accelerator/NPU/GPU/CPU tùy capability; không giả sử
  Rockchip, MediaTek, Novatek có cùng DSP hoặc cùng precision.
- Backend selector dùng capability + measured profile + policy. Nếu hardware bắt buộc
  mà không hỗ trợ thì reject/degraded có lý do; không silently CPU fallback gây quá tải.
- Reference CPU implementation dùng cho golden/logic fallback được cho phép; không
  đổi geometry, quantization, NMS ordering hoặc feature semantics giữa backend tùy tiện.
- DSP decoder là một implementation của semantic decoder contract, không một parser
  tùy model nằm ngoài catalog/versioning. Validate returned count, finite coordinates,
  labels và bounds dù result đến từ trusted backend.
- HTP và cDSP dùng tài nguyên/DDR/power có thể tranh chấp. Admission phải đo cả chuỗi
  concurrent với FW decode/encode; không cộng cơ học FPS tối đa của từng microbenchmark.

### 10.4. Thứ tự tối ưu

1. Đo allocation, copy bytes, CPU samples và queue age theo stage.
2. Reuse workspace, giảm logging nóng, loại copy thừa có thể bỏ mà không đổi ownership.
3. Một model detector: registered input/output + DSP preprocess/postprocess end-to-end.
4. Golden parity cho color conversion, letterbox, quantization, decoder/NMS và source transform.
5. DSP face ROI/align/normalize; batch ROI có giới hạn và fairness, đo với face count tăng.
6. Tối ưu scalar DSP thành fused/HVX kernels khi profile chứng minh cần, không chỉ chuyển
   vòng CPU sang DSP rồi coi đã tối ưu xong.
7. Demand-gate preview đúng semantics hoặc chuyển video owner sang FW theo mục 5.
8. Chỉ thêm concurrency/inference worker khi có lợi cho measured workload, giữ per-source
   serialization và exact-frame completion. Theo
   [ADR 0006](../../adr/0006_unwired_execution_infrastructure.md), worker vẫn reserved.

DSP skel cần toolchain Hexagon/SDK được duyệt và pin riêng; eSDK ARM không tự biên dịch
được mã Hexagon. ARM integration vẫn bắt buộc eSDK. Production PD/signing/library license
phải được BSP xác nhận; khả năng chạy unsigned test PD không phải điều kiện ship sản phẩm.

## 11. Benchmark và kiểm chứng trước khi chốt thiết kế

### 11.1. CPU phải đo cùng điều kiện

Chuẩn hóa: cùng board/BSP/QAIRT, governor/power profile, model hash, input resolution/color,
nguồn video và frame content, cadence từng model, số face/ROI, tracker, overlay/encode,
viewer/evidence demand, gallery size, logging, warmup và thermal state. Nếu không chạy được
cùng binary trên cùng BSP, ghi khác biệt và giới hạn suy luận, không ép tỷ lệ so sánh.

Các bài A/B:

| Bài | Mục tiêu |
|---|---|
| Một detector, cùng cadence, tắt preview/FR/storage | Tách preprocess + infer + decoder |
| Thay CPU path bằng DSP path, giữ model/semantics | Định lượng ARM saving, latency, DDR và độ chính xác |
| Bật preview với 0/1/n viewer và evidence consumer | Đo copy/compose/encode, demand semantics |
| FD + FR, sweep số face và refresh policy | Bắt chi phí cascade và control starvation |
| Nhiều source cùng một detector | Kiểm chứng ownership, fairness, admission và lỗi A04 |
| Bật local ingest/query/compaction và Kafka offline | Chứng minh dữ liệu không làm trễ AI/evidence |
| Full system sustained + thermal | Không coi cửa sổ startup mát là throughput bền vững |

Metrics tối thiểu: CPU process/thread theo một core, tổng CPU các process liên quan,
CPU ms/accepted inference và ms/ROI, stage wall p50/p95/p99, capture-to-result,
event-to-durable-ACK, event-to-FW-ACK, queue age, drop theo nguyên nhân, RSS/PSS, FD,
allocation count, copied bytes/s, hardware utilization nếu BSP hỗ trợ, nhiệt và power.
CPU thấp nhưng FPS giảm, queue age tăng hoặc tracker sai không phải tối ưu đạt yêu cầu.

Latency routing trong hồ sơ hiện tại không thay end-to-end capture→recognition/evidence.
Mốc timestamp phải đặt sau completion thực sự của stage cần đo. Khi DSP làm nhiều hơn,
đo wall time và utilization DSP/HTP chứ không chỉ CPU `top`.

Target CPU 15–20% nên là **mục tiêu thử nghiệm cho workload được ký**, chưa là cam kết.
Chốt SLO latency/FPS/accuracy/power và điều kiện workload cùng lúc, tránh tối ưu một con số.

### 11.2. Test storage/query

- Dataset tổng hợp có cardinality/retention thực tế cho cả security và traffic: ít/nhiều
  track, màu lệch phân bố, plate cardinality cao, relation/gap/signal/calibration revisions,
  cổng/lane đông, cold query và mixed concurrent ingest/query.
- Đo p50/p95/p99, bytes scanned, query plan, memory, write amplification, compaction time,
  DB/Parquet footprint. So SQLite-only với hybrid trước khi quyết định dependency.
- Dùng Q01–Q30 và ma trận S01–S18 mục 6: có positive/negative/unknown/unsupported/expired
  fixtures; kiểm trajectory, relations, scene-only fire/VLM, attendance, plate/lane/speed,
  aggregates và query hồi tố; không chỉ query áo đỏ qua cổng.
- Oracle xác minh không bỏ sót/nhân đôi giữa hot/cold, đúng interval/revision/zone/epoch.
- Crash injection trước/sau DB commit, file rename, manifest publish, compaction swap;
  disk full, corrupted segment, reboot và time jump.
- Không dùng cache-warm một query đơn để quảng cáo mọi truy vấn “nhanh”. Chốt retention,
  source count, query concurrency và latency budget bằng bảng acceptance.

### 11.3. Test delivery/evidence

- Mất mạng lâu, broker timeout, ambiguous ACK, exporter restart, schema không tương thích.
- FW restart giữa nhận và ACK; duplicate/reordered Start/Update/End; media ready sau timeout.
- Quyền bị revoke khi đang queue/retry; sink không được nhận attribute ngoài scope.
- Disk full/hot DB locked/Parquet compaction nặng không làm mất alarm âm thầm.
- Thiếu pre-roll khi cold start, no viewer nhưng cần evidence, profile change/GOP khác.
- Verifier kiểm actual clip interval, event ID và frame/clock correlation; không chỉ đếm file.

### 11.4. Test DSP/lifecycle

- Golden preprocessing với stride/padding, NV12/NV21, màu/range, crop biên, ROI lẻ,
  rotation/letterbox, quantization nhiều dtype; decoder parity cả tie/threshold/candidate cap.
- Callback/completion trễ, FD number reuse, context restart, ring/pool full, DSP fault,
  revoke/stop đang chạy, kết quả đến sau source epoch đổi.
- Không ACK/recycle RAW frame hay output buffer trước hardware completion.
- QEMU kiểm tra logic bằng eSDK; DMA/cache/fence và hiệu năng chỉ kết luận bằng board.
  Chỉ dùng board `192.168.138.98`, workspace `/opt/lacai`.
- Không bắt chước script deploy hay đường staging cá nhân của repository FW tham chiếu.

## 12. Lộ trình cải tiến và review tiếp

### 12.1. Các gói công việc

| Gói | Team chủ trì; bàn giao phụ thuộc | Đầu ra cụ thể | Điều kiện review xong |
|---|---|---|---|
| R0 — authority và hợp đồng | AI APP lead; BSP+FW và AI Model ký boundary liên quan | C01–C10; S01–S18/D01–D18/Q01–Q30; ADR scope video/data; trust/admission; SLO | Có người nhận và fixtures mỗi contract; cập nhật authority docs trước đổi boundary |
| R1 — đóng lỗ composition | AI APP | Sửa A01/A02/A06/A08; tách harness khỏi production; capability thật | Negative tests không tự cấp quyền/đổi feature; output thật đi qua gate |
| R2 — baseline CPU | AI APP; BSP+FW cấp target/resource profile, AI Model cấp workload | Reproducible manifest, per-stage profile, full-system A/B | Đủ giải thích CPU/copy budget, không nhầm FD với FR |
| R3 — một vertical DSP | AI APP; C02 từ BSP+FW, C03 từ AI Model | Detector leased tensor/pool và cDSP pre/post | eSDK build, golden parity, board completion tests, measured saving |
| R4 — metadata/event tối thiểu | AI APP; BSP+FW nhận C06/C07, AI Model cấp fixtures/kits | M1–M3 mục 6.15: common person/vehicle/scene schema, query baseline, outbox/UDS/receipts | Query oracle security+traffic, unsupported rõ, crash/retry không nhân đôi evidence |
| R5 — archive và cloud | AI APP; BSP+FW xác nhận storage/network/center deployment | M4 storage ADR, archive nếu cần, Kafka exporter và center ingest workstream tách biệt | Benchmark quyết định engine; hot/cold consistency, offline quota, broker và lake receipts riêng |
| R6 — chuyển video owner | BSP+FW; AI APP cấp C08/compatibility bridge | FW render/encode capability, legacy negotiation, rollback | Released-FW RTSP/UI/evidence conformance và tổng tài nguyên được đo |
| R7 — scale và vận hành | AI APP chủ trì runtime; BSP+FW bàn giao C10 và recovery primitive | Multi-source bindings, cascade fairness, bounded recovery, package/CI/soak | Nguồn lỗi độc lập, control responsive, không false completion; BSP+FW ký deployment |

R2 có thể tiến hành cùng R0/R1; R4 không phải chờ tối ưu DSP xong. R5 chỉ sau khi schema,
durability và identity R4 ổn định. Không làm Parquet/Kafka trước rồi mới xác định event/track
identity. Không wire worker/recovery reserved chỉ để tăng số module “đã hoàn thành”.

### 12.2. Các module/contract nên bổ sung hoặc mở rộng

Các tên sau là vai trò thiết kế, chưa tạo file hay đăng ký symbol:

- metadata projection: observations → segments, attribute/relation intervals, passages,
  measurements, scene events và aggregates theo D01–D18;
- authorized metadata sink và delivery receipt, giữ khác biệt live/durable;
- edge data writer, query facade, archive manifest/compactor;
- persistent outbox với state riêng cho FW và Kafka;
- FW event/evidence client adapter và receipt reconciler;
- Kafka exporter adapter;
- tensor buffer lease/result view và Qualcomm DSP implementation của image/decoder ports;
- annotation transport/video capability migration;
- production service runtime/generation owner, recognition owner, output owner thay logic
  lẫn trong `main`.

Không bắt buộc tạo một class/target cho từng dòng nếu không có lifecycle/dependency riêng.
Đăng ký naming trước source; public boundary revision cần lead và owner review. Giữ
dependency neutral → port, adapter → SDK; không cho perception include Kafka/SQLite/QNN.

### 12.3. Những quyết định cần người phụ trách chốt

1. Retention local bao lâu; số source/track tối đa; query latency và concurrency yêu cầu?
2. Stable usecase IDs cho đủ 18 mục; profile/query nào mở trước, track hay identity,
   temporal semantics và field nào được phép lưu/xuất?
3. Alarm nào bắt buộc durable/evidence; hành vi khi disk full/offline vượt quota?
4. Có bắt buộc burn-in vào evidence không; source/profile nào cung cấp prebuffer?
5. FW có nhận video ownership không; release nào, negotiation và rollback thế nào?
6. Cloud được nhận những field nào, embedding có bị cấm mặc định, ai quản lý credentials?
7. DSP SDK/signing/cache/completion nào BSP cam kết trên QCS6490 / Linux 1.8?
8. Mục tiêu CPU áp dụng workload nào, ở trạng thái nhiệt nào, có tính FW và data service
   thuộc AI APP không?
9. Traffic nào thuộc roadmap đầu: count/lane/ANPR hay calibration/signal-dependent analytics;
   BSP+FW giao được authoritative inputs nào?
10. Center application và infrastructure có nằm trong release này không; AI APP/BSP+FW
    bố trí người/ngân sách nào, hay chỉ nghiệm thu edge + broker contract trước?

### 12.4. Checklist cho lần review kế tiếp

- [ ] Có ADR/contract đã duyệt; proposal không bị báo thành capability đã ship.
- [ ] Entitlement/admission không được dựng `true` từ việc model đã load.
- [ ] Mỗi feature dùng đúng processor; không reference fallback trong production ngoài scope rõ.
- [ ] Thử hai nguồn dùng cùng model; state/transform/tracker không trộn.
- [ ] Có mapping đủ S01–S18 → data/query dependencies, capability và quyền; không mất mục
  cháy/khói, attendance, VLM vì schema chỉ nghĩ đến track.
- [ ] Q01–Q30 có semantics/limits và test theo profile; person/vehicle/scene dùng common core.
- [ ] Calibration/signal/clock invalid, unknown/partial/expired không bị trả thành zero/false.
- [ ] C01–C10 có owner/consumer/test gate; không đầu việc giao cho team không tồn tại.
- [ ] Local/Kafka/FW dùng ID/schema thống nhất nhưng delivery state độc lập.
- [ ] ACK level ghi rõ; crash/replay/dedup và disk-full test có kết quả.
- [ ] Evidence có actual interval, gap và correlation; no-viewer không phá pre-roll.
- [ ] Không expose vendor types; buffer lease/completion đúng qua DSP/QNN/result consumer.
- [ ] Có CPU/latency/copy/DDR/thermal A/B cùng workload, kèm accuracy regression.
- [ ] C++ build/tests dùng eSDK; DSP toolchain được duyệt; không dùng host pass thay target.
- [ ] Có acceptance released-FW thay vì chỉ harness; rollback và observability đủ.
- [ ] Chạy naming/layout/docs checks; docs phản ánh đúng source wiring và bằng chứng.

### 12.5. Các execution plan có thể giao ngay

Các file dưới đây là kế hoạch thực thi sau Plan 0. Agent phải hoàn thành Plan 0 trước; chỉ sau
khi P0-09 là UNBLOCKED và bốn gate P0-A…P0-D pass mới mở Plan 1. Plan 1A khóa package,
entitlement và install authority cho mô hình mỗi usecase là một application. Sau đó Plan 2/3/4
có thể bắt đầu theo dependency, còn Plan 5 tích hợp cuối cùng. Mỗi plan có owner, task ID,
output và acceptance riêng; status của một plan không tự nâng status các plan khác.

| Plan | Chủ trì | Bắt đầu khi | Kết quả chính |
|---|---|---|---|
| [Plan 0. Production composition foundation](../../development/production_composition_foundation_review.md) | AI APP lead | Technical foundation UNBLOCKED; board-smoke .98 | Scoped authority, async cascade, clean drain và single-source observation profile; product/owner acceptance chưa thay thế |
| [1. Contract và phạm vi team](contract_and_team_scope.md) | AI APP lead | **Accepted 2026-09-18**; BSP/FW và Model nộp receipts theo registry | C01–C10 machine registry, S01–S18 stable IDs, owner/conformance matrix |
| [1A. Phân phối ứng dụng usecase](usecase_app_distribution_plan.md) | AI APP lead | **Board-smoke first-install/control 2026-09-20**; update/rollback/journal còn mở | AI-owned D-Bus App Manager, app-as-SKU/shared-runtime, entitlement-gated install và installed-only control; backend là peer theo contract AI APP |
| [1B. Product slice khói/lửa và App Manager](fire_smoke_product_slice_plan.md) | AI APP lead | **Board-smoke 2026-09-20**; chưa accepted | Service bootstrap đã tách, S04 đi qua install/config/activation/metadata/evidence; model quality, cold-start và released-FW gates còn mở |
| [2. Metadata và query](metadata_query_plan.md) | AI APP | **Accepted 2026-09-19**; ADR 0009 accepted | Composed v1 service, receipt-safe retention, board fault gates, 142/142 eSDK và exact 5-minute candidate pass |
| [3. Event và evidence transport](event_evidence_transport_plan.md) | AI APP + BSP+FW | **AI-owned path board-smoke 2026-09-20**; released-FW receiver/receipt còn mở | UDS/outbox/ACK, FW evidence receipt và fault tests |
| [4. DSP đa nền tảng](dsp_multiplatform_optimization_plan.md) | AI APP + BSP+FW + AI Model | **AI APP scope accepted 2026-09-18**; external owner gates retained | Generic v1 cDSP preprocess/dense/overlay; 30.008 FPS, 13.50% CPU/5 phút |
| [5. Integration và rollout](integration_validation_rollout_plan.md) | AI APP lead | Plan 0 + Plans 1/1A–4 pass | Profiles, board/release acceptance |

Quy tắc giao agent: ghi plan/task ID trong issue; không sửa sibling repository; không đổi
contract hiện hành mà không ADR/review; commit chỉ chứa task-owned files; báo rõ test chưa
chạy và lý do. Các agent có thể làm fixture/mock trước source production, nhưng không
được mô tả mock là released-FW hoặc target acceptance.

## Giới hạn và công việc tiếp theo

- Plan 1, Plan 2 và scope Plan 4 đã accepted theo gate riêng. Plan 1A/1B và đường event/evidence
  phía AI APP đã đạt board-smoke ngày 2026-09-20, nhưng chưa accepted: update/rollback/journal,
  model-quality, cold-start QNN và released-FW receiver/receipt vẫn là gate bắt buộc.
- Plan 4 đã đo full workload hiện tại trong 5 phút; 18-usecase capacity và released-FW profile
  vẫn cần scenario/evidence riêng, không ngoại suy từ kết quả hiện tại.
- SQLite packed shards + materialized rollup đã qua workload `FULL` 5 phút và là lựa chọn edge v1;
  DuckDB/Parquet không vào edge baseline, librdkafka/receipt thuộc P3.
- Chưa có sizing/SLO sản phẩm cuối cùng. Mọi queue, timeout, cadence, quota và retention
  phải từ cấu hình được validate; số minh họa trong tài liệu không thành default runtime.
- Ưu tiên review R0/R1 và dựng benchmark R2; sau đó làm R3 và R4 thành hai vertical có
  nghiệm thu độc lập. Đây là đường cải tiến từng bước, không một đợt viết lại toàn hệ thống.

## Tài liệu liên quan

- [System architecture](../../architecture/system_architecture.md)
- [Architecture alignment review](../../development/architecture_alignment_review.md)
- [Implementation status](../../development/implementation_status.md)
- [Capability matrix](../../development/capability_matrix.md)
- [Feature event dispatch](../../architecture/feature_event_dispatch.md)
- [Feature event contract](../../architecture/feature_event_contract.md)
- [Attribute reader](../../architecture/attribute_reader.md)
- [Feature catalog hiện tại và khoảng trống so với danh sách 18 mục](../../architecture/feature_catalog.md)
- [Model integration kit](../../contracts/model_integration.md)
- [FW–AI APP boundary](../../contracts/fw_ai_app_contract.md)
- [Usecase control](../../contracts/fw_usecase_control.md)
- [Application composition](../../architecture/application_composition.md)
- [Artifact digest](../../architecture/artifact_digest.md)
- [FW release compatibility](../../contracts/fw_release_compatibility.md)
- [Owned QNN engine](../../adr/0003_owned_qnn_engine.md)
- [FR gallery/index ownership](../../adr/0004_fr_gallery_and_vector_index.md)
- [Reserved execution infrastructure](../../adr/0006_unwired_execution_infrastructure.md)
- [FR production validation](../../testing/face_recognition_production_validation.md)
- [eSDK emulation](../../testing/esdk_emulation.md)
- [Board workspace](../../testing/board_workspace.md)
