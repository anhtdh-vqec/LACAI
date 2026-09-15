# Kế hoạch hoàn thiện Face Recognition: SCRFD + EdgeFace + Zvec

Ngày lập: 2026-09-15. Cập nhật theo source hiện tại: 2026-09-15.
Trạng thái: kế hoạch triển khai và nghiệm thu. Các mục đã đánh dấu chỉ có nghĩa là source
và logic test tương ứng đã có; gate board/golden/owner review vẫn quyết định nghiệm thu.
Đích triển khai: QCS6490 / Qualcomm Linux 1.8. Board phát triển hiện được phân bổ là `.99`;
không dùng `.48` khi board đó đang có người sử dụng.

## 1. Kết quả phải bàn giao

Một usecase FR hoàn chỉnh phải đi từ ảnh camera thật đến kết quả nhận diện có thể kiểm chứng:

```text
FW RAW source → SCRFD → face bbox + 5 landmarks → association/quality gate
             → giữ đúng frame → alignment/crop → EdgeFace → embedding chuẩn hóa
             → Zvec search theo gallery revision → quyết định known/unknown/ambiguous
             → trạng thái nhận diện theo track → sự kiện điểm danh → FW/UI/persistence
```

Ngoài nhận diện trực tiếp, phải có enrollment, cập nhật/xóa người, đồng bộ index, phục hồi
sau restart, kiểm soát truy cập, cấu hình, đo hiệu năng và khả năng vận hành. Không coi
bbox khuôn mặt, model execute thành công hoặc top-1 search là usecase đã hoàn thành.

SCRFD và EdgeFace là hai package đầu tiên. Runtime không được phân nhánh theo tên hai
model này. Tensor names, dimensions, quantization, landmark template, thresholds và
cadence đến từ package/catalog/deployment đã validate. Thay model tương thích contract
không cần sửa orchestration. YOLO person không phải dependency bắt buộc của FR.

## 2. Baseline và giới hạn của bằng chứng hiện có

| Hạng mục | Đã có | Chưa chứng minh/chưa nối |
|---|---|---|
| SCRFD-500M-KPS W8A16 | QNN HTP probe; package/decoder/catalog binding; production primary selection; compatibility live cascade `.99` | Golden accuracy với ảnh thật và released-FW acceptance |
| EdgeFace-S gamma=0.5 W8A16 | QNN HTP probe; package/decoder/alignment contract; production secondary binding; compatibility live embeddings `.99` | Golden crop/input/embedding parity và post-fix multi-face rerun |
| Package registry | Binding riêng theo immutable model identity; role/dependency activation | Artifact authenticity và TOCTOU-safe trusted open |
| Anchor-distance decoder | Typed decode, inverse placement, NMS, landmarks; production chọn kind từ package | Golden tensors thật; output batch còn cấp phát; malformed metadata cần test rộng hơn |
| Frame store | Exact key, owner giữ frame, pump/session wiring, task tickets, byte budget, dependent drain | Completion phần cứng và age/epoch policy mở rộng |
| Cascade coordinator | Ticket-correlated alignment, synchronous secondary execute/decode, per-task isolation; compatibility live smoke `.99` | Async worker/fairness, post-fix multi-face và released-FW evidence |
| Image alignment port | Typed landmarks/template/transform/completion; Qualcomm FastCV adapter | Destination pool và golden alignment parity |
| Zvec | v0.7.0 public ARM64 SDK, mặc định build adapter; real-library tests pass trên `.48` | Chỉ tạo collection mới; revision/record IDs trong RAM; recovery/enrollment chưa có |
| FR/điểm danh | Production FD-to-embedding source composition; feature/event infrastructure | Matching, recognition state, gallery recovery, enrollment và attendance |

Bộ test eSDK QEMU hiện bao phủ graph lifecycle, runtime cascade invocation, retention/drain
và Zvec linking; số lượng chính xác nằm trong validation của từng commit. Test frame-store
và Zvec từng chạy native trên board. Đây là bằng chứng logic/index, không phải nghiệm thu
nhận diện hay DMA.

Probe lịch sử: SCRFD trung bình 4.405 ms/20 lần; EdgeFace 2.918 ms/50 lần. Input zero
chỉ chứng minh execution/ABI. Không dùng tổng hai số này để suy ra FPS pipeline.
Person compatibility flow từng đạt 30 FPS với CPU khoảng 44.5%; không phải số đo FR.
Xem [cascade evidence](../architecture/cascade_inference.md) và
[preprocessing evidence](../architecture/qualcomm_preprocessing.md).

## 3. Các ràng buộc xuyên suốt

- Mọi C++ build/test/configure dùng `/home/a/Workspace/eSDK`; QEMU chỉ là logic evidence.
- Tận dụng converter/FastCV/QNN HTP, allocator và encoder Qualcomm đã có; đọc source/vendor
  capability trước khi chọn implementation. Không tự viết lại kernel vendor đã đáp ứng yêu cầu.
- Không mặc định converter có thể làm arbitrary landmark affine warp: phải kiểm chứng API,
  format, interpolation, destination ownership và khả năng chạy phần cứng trên BSP hiện tại.
- Vendor types ở adapters. Core/perception/runtime/features chỉ thấy neutral ports/contracts.
- Queue/pool/task/cache đều có budget và policy khi đầy. Không backlog vô hạn để giữ đủ FPS.
- Owner RAW/crop/tensor phải sống qua lần đọc phần cứng cuối. Timeout, FD close và stop không
  đồng nghĩa completion. Worker giữ frame copy ngay cả khi controller rời scope.
- No magic number/string/hardcode theo code_convention. Policy phải cấu hình và validate;
  protocol constants có owner. Không chỉ đổi literal thành constexpr.
- Không commit model binaries, biometric samples, embeddings thật, mật khẩu hoặc SDK private.
- Thay ABI/ownership/entitlement cần lead và owner review. Mỗi bước có tests/docs và commit
  riêng; người dùng tự push theo chỉ đạo hiện tại.

## 4. Thứ tự và dependency

| Mốc | Nội dung | Phụ thuộc | Đầu ra kiểm chứng |
|---|---|---|---|
| M0 | Chốt bundle, ABI, golden data và workload | — | Hai package và baseline có provenance |
| M1 | Hoàn thiện primary FD thật | M0 | Bbox/landmarks đúng trên ảnh và camera |
| M2 | Contracts cascade + ownership | M0 | Request/result/lifecycle rõ, tests lỗi |
| M3 | Nối retention vào pump | M1, M2 | Chính xác frame/epoch, drain không ACK sớm |
| M4 | Alignment Qualcomm và crop pool | M0, M2 | Golden crop/tensor parity |
| M5 | EdgeFace secondary + embedding decode | M3, M4 | Live FD → embeddings đúng correlation |
| M6 | Gallery durable + Zvec recovery | M0, M2 | Enrollment data sống qua restart, revision đúng |
| M7 | Enrollment/control API | M5, M6 | Add/update/delete idempotent, được authorize |
| M8 | Matching/track-level recognition | M5, M6 | Known/unknown/ambiguous có calibration |
| M9 | Điểm danh và FW integration | M7, M8 | Event ổn định, retry/restart không nhân bản |
| M10 | Performance, fault/soak, release | Đo từ M1; nghiệm thu sau M9 | Bundle release có evidence |

M6 có thể được triển khai độc lập bằng vectors tổng hợp trong lúc hoàn thiện M3–M5.
Không cần chờ tối ưu shared-QNN-context để có luồng đúng đầu tiên. Không bật secondary
full-frame chỉ để làm hai model cùng execute.

## 5. M0 — Chốt hai model package và golden baseline

- [x] Kiểm kê artifacts tại thư mục model được giao; ghi SHA-256, QNN export/runtime version,
  graph name, input/output ABI và preprocessing provenance. Kiểm tra đúng biến thể model.
- [x] Tạo package SCRFD: io_manifest, decoder.json, labels nếu cần, catalog binding và digest.
  Xác minh thứ tự output, score activation, anchor ordering/offset, stride, distance units,
  layout `[1,N,C]`, landmark ordering, quantization của từng tensor.
- [x] Tạo package EdgeFace: output tensor name/dtype/layout/dimensions, normalization,
  RGB/BGR, pixel range, mean/scale, quantize rounding/saturation và alignment template.
- [x] Không suy preprocessing chỉ từ shape hay tên model. So sánh training/export/reference
  pipeline. QNN offset và neutral zero_point phải chuyển đúng dấu.
- [ ] Tạo bộ golden được phép sử dụng, lưu ngoài Git: không mặt, một/nhiều mặt, nghiêng,
  biên ảnh, sáng/tối, occlusion, nhiều tỷ lệ source. Có expected intermediate tensors.
- [ ] Chốt workload nghiệm thu: độ phân giải/FPS, số camera, số mặt đồng thời, gallery size,
  embeddings/người, output bật/tắt, thời gian chạy và môi trường nhiệt.

**Gate:** cùng input, QNN execution/parity đã đối chiếu; input preprocessing có nguồn rõ.
Sai contract phải fail activation; artifact digest không được gọi là chữ ký xác thực.

**Tiến độ M0 (2026-09-15):** metadata package đã tạo từ ABI probe thật trên `.48` —
`manifests/models/scrfd_500m_bnkps/` (io_manifest, decoder.json anchor_distance,
preprocess, model_metadata + SHA-256) và `manifests/models/edgeface_s_gamma_05/`
(io_manifest, preprocess, model_metadata + SHA-256; decoder.json để M5). Catalog/registry
example: `manifests/models/model_catalog.face.example.json`,
`config/defaults/model_package_registry.face.example.json`. Cả hai decoder contract được
load và cross-validate; reference source đã xác nhận anchor offset, landmark ordering,
color/normalization và alignment template. **Còn thiếu:** golden được phép dùng,
threshold/accuracy calibration và pin QAIRT runtime. Không coi metadata là nghiệm thu model.

## 6. M1 — Primary FD chạy đúng thật

Vị trí chính: `src/app/vqec_vision_production_platform.cpp`,
`src/perception/detection/vqec_vision_anchor_distance_decoder.*`, package/schema.

- [ ] Kiểm tra parser và validator thống nhất: required/unknown fields, signed→unsigned,
  số quá lớn, duplicate/cross-stage tensor names, dtype, quantization, output bytes và rank.
- [ ] Đối chiếu tensor input với catalog; placement/geometry phải cùng nguồn authority
  với preprocess. Không lấy kích thước source đầu tiên cho mọi model.
- [ ] Giữ reject khác geometry khi owner còn dùng chung decoder; sau đó chuyển decoder
  thành owner theo source/model nếu cần hỗ trợ nhiều source khác kích thước.
- [ ] Golden test dequantization → anchor decode → NMS → inverse transform → landmarks.
  Bao gồm stretch/letterbox, padding, tọa độ âm, overflow, NaN/Inf và bbox ngoài ảnh.
- [ ] Định nghĩa overflow candidate policy: không vô tình fault cả stream vì cảnh đông;
  nếu bounded top-score selection được chọn, test determinism và accuracy của truncation.
- [ ] Chuyển output observations/landmarks sang pool/reuse đã đo; giữ atomic publication
  khi lỗi. Preallocated candidate buffer không đồng nghĩa toàn decoder hết allocation.
- [ ] Chạy FD camera thật, overlay bbox + landmarks phục vụ debug; đo stage timing và
  xác minh màu/stride/rotation với camera hiện tại.

**Gate:** đúng golden với tolerance ghi trong test manifest; camera frame và output mapping
đúng, không regression person pipeline. Chưa gọi là nhận diện danh tính.

## 7. M2 — Contracts cascade và quyền sở hữu

Vị trí chính: `vqec_vision_secondary_inference.hpp`, `vqec_vision_image_processor.hpp`,
`vqec_vision_embedding.hpp`, catalog/deployment schemas, scheduler.

- [x] Thêm role primary/secondary, dependency và preprocess/alignment capability vào catalog.
  Secondary models không nằm trong mask gửi full-frame của primary session.
- [x] Request typed chứa source slot + full frame key, model slot/revision, track identity
  cùng epoch, landmark schema/count/points, ROI, transform provenance, deadline monotonic,
  priority và ticket retention. Không nhét landmarks vào opaque string/JSON.
- [x] Chốt port alignment riêng; contract mô tả source coordinates, destination tensor,
  transform, capability và completion.
- [x] Result embedding typed giữ frame/track correlation, model/version và normalization;
  không dùng payload bytes thiếu layout/meaning cho matching.
- [ ] Chuẩn hóa task status/error typed cho delivery bất đồng bộ; hiện lỗi task được tổng hợp
  trong cascade report và không tạo embedding giả.
- [ ] State machine: queued → submitted → completed → delivered; queued có thể hủy ngay,
  submitted phải drain; stale epoch ngăn publish nhưng vẫn phải hoàn tất cleanup.
- [x] Chốt quyền gọi complete(ticket), ownership thread, ticket domain lifetime và ngăn
  stale completion của store cũ đi vào store mới có số ticket trùng.
- [x] Test bad schema, mismatch epoch/model, wrong ticket, duplicated completion, stopped owner.

**Gate:** lead/owner review contracts và failure semantics trước khi nối hardware.

## 8. M3 — Retention và fan-out thực tế

Vị trí chính: `vqec_vision_multi_model_pump.*`, `vqec_vision_multi_model_session.*`,
`vqec_vision_cascade_frame_store.hpp`, source session/supervisor.

- [x] Retain frame trước primary submit cho model có dependency. Primary submit bị reject
  phải rollback đúng; không để slot giữ mãi khi không có kết quả.
- [x] Khi FD result về, lookup đúng source key từ submission ticket; tuyệt đối không lấy
  latest preview frame. Match descriptor buffer ID/epoch/PTS và nguồn camera/channel.
- [ ] Quality/admission tạo tối đa số secondary task cấu hình; mỗi task nhận owner và ticket.
  Giữ slot đến khi primary đã đóng admission và tất cả secondary task đã completion.
- [ ] Nếu scheduler enqueue thất bại, trả ticket ngay vì chưa submit phần cứng. Nếu submit
  không rõ đã nhận hay chưa, giữ owner và đi vào drain/recovery, không coi là reject sạch.
- [ ] Thiết lập timeout queued work, age/drop policy, fairness theo source/track và counters.
  Dùng monotonic arrival clock cho deadline; không trừ PTS khác clock domain.
- [ ] Source restart: đóng admission epoch cũ, hủy queued, drain submitted; source StopStream
  vẫn chờ toàn bộ frame owners, kể cả cascade/crop. Test partial-start rollback.
- [ ] Validate cấu hình store: frame/task ceilings, tổng bytes kể cả RAW + crops + tensors,
  overflow arithmetic, zero budgets, allocation failure và peak occupancy.
- [ ] Ngăn frame-key reuse/stale result được match với allocation mới; không dùng FD số
  hoặc buffer_id riêng lẻ làm identity. Chốt reset/reconstruction lifecycle rõ ràng.

**Gate:** fault tests chứng minh không ACK sớm, không leak, không trộn frame/epoch, ngân sách
không được trả trước real completion. Native tests với workers giả chưa đủ để chứng minh DMA.

## 9. M4 — Alignment và preprocessing Qualcomm

- [x] Xác minh 5-point order/template từ EdgeFace reference và cấu hình template/coordinate
  convention trong package.
- [x] Tính similarity transform trong neutral code và thực hiện pixel warp/color qua
  adapter FastCV Qualcomm có capability đã kiểm chứng.
- [ ] Kiểm chứng golden cho transform và crop trên dữ liệu model được phép dùng; reject
  điểm trùng, suy biến, NaN, mặt quá nhỏ và transform quá mức theo policy đã duyệt.
- [x] Đánh giá QTI converter/FastCV cho affine warp/crop; ghi API, supported format, execution
  backend và giới hạn. Adapter FastCV được chọn vì QTI plugin không có arbitrary affine.
- [x] Giữ thứ tự warp/resize, RGB conversion, normalization và quantization đúng reference.
  Padding/border/interpolation được khai báo tại package/adapter boundary.
- [ ] Pool crop/tensor theo số in-flight đã admit; cache/import theo allocation identity và
  generation. Không create/destroy pipeline/allocator cho từng khuôn mặt.
- [ ] Kiểm tra DMA-BUF modifier, stride, cache sync/fence và completion; không gọi là zero-copy
  khi vẫn có memfd copy, tensor pack hoặc QNN client-buffer copy.
- [ ] Golden so sánh ảnh crop, input tensor và embedding downstream; ghi tolerance và nguyên
  nhân sai số lượng tử. Không chỉ nhìn crop đẹp bằng mắt.

**Gate:** crop/tensor parity và đo latency/CPU/copies trên board; không reuse destination
khi HTP còn đọc. Input 112×112 chỉ thuộc package EdgeFace hiện tại.

**Tiến độ M4 capability (2026-09-15):** đã kiểm chứng trên `.48` + eSDK sysroot —
`qtivtransform` chỉ có crop/destination/resize/flip/rotate 90° (không affine tuỳ ý); FastCV
`libfastcvopt.so.1.8.0` export affine warp (`fcvTransformAffineu8_v2`,
`fcv3ChannelTransformAffineClippedBCu8`, `fcvGeomAffineFitf32`, `fcvGetPerspectiveTransformf32`).
Kết luận: adapter alignment nên là owned FastCV (không dùng QTI plugin affine). Đây là
capability evidence, chưa phải runtime/golden parity hay DSP offload.

## 10. M5 — EdgeFace secondary và embedding decoder

- [x] Tạo secondary graph owner riêng; load một lần tại activation, không mỗi mặt.
  Giữ đường đồng bộ đúng trước, sau đó chỉ dùng async khi runtime capability được kiểm chứng.
- [ ] Scheduler không gọi alignment/QNN/search blocking trên camera receive/output thread.
  Bounded worker + completion queue, fair admission và explicit busy handling.
- [x] Decode output theo typed tensor reader, dequantize đúng, kiểm finite/dimension, từ chối
  norm gần zero; L2 normalize và gắn model/version, task, frame/track identity.
- [ ] Giữ crop/input/output owners qua completion và delivery; reject kết quả stale khi track
  mất, epoch đổi, model version thay hoặc feature bị revoke.
- [ ] Chạy camera thật: FD → crop → EdgeFace, xuất diagnostic không chứa embedding ra log.
- [ ] Golden embedding parity với model reference; thử cùng người/khác người chỉ là sanity,
  chưa phải calibration threshold hay tiêu chí nhận diện.

**Gate:** mỗi result truy được về đúng face/frame và không trộn slot; overload không làm
preview/primary bị kẹt. Đo crop jobs/s riêng với primary FPS.

## 11. M6 — Gallery durable và Zvec recovery

Vị trí chính: `src/adapters/zvec/`, `embedding_index_port`; bổ sung storage port và FR owner.
Theo [ADR 0004](../adr/0004_fr_gallery_and_vector_index.md), authoritative gallery nằm sau
storage boundary; Zvec là index dẫn xuất, không phải nguồn duy nhất của identity data.

- [ ] Schema lưu subject ID opaque, record ID, embedding model/version/preprocess revision,
  vector dimension/metric, template quality, gallery revision, timestamps và deletion state.
- [ ] Chốt protected/encrypted storage với FW: key provisioning, permissions, quota, backup,
  retention/purge và schema migration. Không tự nhúng khóa hay đường dẫn deployment.
- [ ] Transaction/journal: validate + CAS → durable authoritative commit → cập nhật Zvec →
  publish indexed revision. Search chỉ chạy revision đã đồng bộ. Crash giữa từng bước phải
  replay idempotent hoặc rebuild; không tự gắn revision mới cho collection cũ.
- [ ] Metadata durable cho index: schema/model fingerprint, revision, build status; open
  collection chỉ sau handshake. Thiếu/sai/corrupt thì unavailable/rebuild, không stale match.
- [ ] Rebuild sang collection tạm, kiểm số records/parity rồi chuyển generation an toàn;
  queries đang chạy giữ generation cũ đến completion. Không xóa gallery người dùng tự động.
- [ ] Đưa query/upsert/delete vào owner worker có queue bounded. Hiện adapter giữ mutex khi
  gọi vendor và allocate query/doc mỗi search: cần đo, tránh block camera, giới hạn concurrency.
- [ ] Backend hiện dùng FLAT. Benchmark FLAT với index khác nếu gallery cần; lựa chọn index,
  build/search parameters là config có validation. ANN phải đo recall và tác động FR accuracy.
- [ ] Validate returned IDs, finite/range/order của similarity, number of results, stable ties;
  không publish batch một phần khi backend lỗi. Cosine similarity = 1 − Zvec cosine distance.
- [ ] Fault injection: disk full, permission denied, torn write, corrupt metadata, writer crash,
  repeated mutation, incompatible model, concurrent search/delete và failed index mutation.
- [ ] Review redistribution licenses/dependency notices của public SDK; pin/hash và khả năng
  thay artifact theo vendor. Không tuyên bố Zvec GPU/Adreno acceleration từ test hiện có.

**Gate:** enroll/update/delete tồn tại đúng sau restart; crash không tạo match stale hoặc
identity mồ côi; không search khi revision mismatch. Có backup/restore/rebuild procedure.

## 12. M7 — Enrollment và control API

- [ ] Define FW API request/result cho create subject, enroll sample, replace/remove template,
  delete subject, query status và rebuild; schema version, authorization, idempotency key,
  expected revision, timeout và error taxonomy.
- [ ] Enrollment online dùng cùng alignment/embedding pipeline như recognition. Với ảnh
  upload, thêm input adapter và validation riêng; không tự coi upload là camera frame.
- [ ] Chọn đúng subject/session được authorize; reject nhiều mặt không xác định, chất lượng
  kém hoặc sample không đạt policy. Không tự enroll unknown visitor.
- [ ] Số mẫu/người, diversity, duplicate sample/duplicate subject handling và quality threshold
  phải cấu hình/calibrate. Tổng hợp centroid chỉ khi đã đánh giá accuracy so với multi-template.
- [ ] Bound template count, gallery size, request size và rate; công bố progress/error rõ.
- [ ] Delete phải invalidate index, caches và pending results, đồng thời thực thi purge theo
  storage policy. Test retry enrollment sau mất mạng không tạo bản ghi lặp.

**Gate:** quản trị gallery được end-to-end qua FW API, không cần sửa file tay trên board.

## 13. M8 — Matching và trạng thái nhận diện

- [ ] Search pinned model/version/gallery revision qua neutral port; Zvec là candidate retrieval.
- [ ] Aggregate nhiều template thành subject candidate; top-1/top-2 margin so giữa hai subject
  khác nhau, không giữa hai template của cùng người. Lấy đủ candidates để không bỏ sót runner-up.
- [ ] Định nghĩa known/unknown/ambiguous/low_quality/unavailable; backend lỗi không thành unknown.
- [ ] Calibrate threshold/margin/quality theo dataset đại diện camera và enrollment policy;
  tách tập calibration và evaluation. Không chọn một threshold phổ biến từ Internet.
- [ ] Đo false accept/reject, identification recall và unknown rejection theo gallery/workload;
  công bố sample size, điều kiện và hạn chế. Calibration thay đổi thì revision hóa policy.
- [ ] Recognition state theo source epoch + track generation; temporal confirmation, cooldown,
  refresh interval và maximum age cấu hình. Không reuse identity sau tracker ID switch/reuse.
- [ ] Invalidate match cache khi gallery/model/policy revision đổi, subject bị xóa hoặc entitlement
  bị revoke; không gắn tên từ kết quả cũ vào bbox hiện tại của người khác.
- [ ] Liveness: SCRFD + EdgeFace không tự cung cấp anti-spoof. Nếu yêu cầu điểm danh chống
  ảnh/video giả, cần PAD/liveness riêng hoặc cơ chế sản phẩm đã duyệt, có test và budget riêng.

**Gate:** recognition policy có số đo accuracy và uncertainty states; UI không hiển thị
confident identity khi dữ liệu chưa đủ hoặc gallery unavailable.

## 14. M9 — Điểm danh, output và FW

- [ ] Processor FR/attendance vào feature registry, activation/dependency/entitlement gates;
  không bypass bằng flag luôn true. Search và identity outputs đều được authorize.
- [ ] Chốt semantics sản phẩm: presence/check-in/check-out, camera/zone, timezone, ca làm,
  re-entry/cooldown và cách xử lý nhiều camera. Không suy check-out chỉ từ mất bbox một frame.
- [ ] Stable event ID + idempotency, durable outbox/ack/retry; FW reconnect/restart không làm
  nhân đôi điểm danh. Chốt at-least-once delivery và dedup ownership, không hứa exactly-once.
- [ ] Dùng monotonic cho interval; UTC cho event time với clock quality. Test NTP step,
  mất đồng bộ và timezone changes theo policy đã chọn.
- [ ] Preview gắn label với track/frame freshness, lọc dữ liệu được phép hiển thị; tên chỉ
  format ở output boundary. FW chịu RTSP/UI/recording; AI chịu overlay/encode/ring.
- [ ] UI/admin phản ánh loading/ready/gallery_rebuilding/degraded/faulted, không chỉ bật/tắt.

**Gate:** enrollment → camera match → attendance → FW persistence qua retry/restart chạy
đúng. Acceptance điểm danh tách khỏi recognition accuracy và video FPS.

## 15. M10 — Đo hiệu năng và tối ưu có bằng chứng

Bắt đầu instrumentation từ M1, không chờ xong feature mới đo.

- [ ] Timestamp/counters ở RAW receive, primary queue/preprocess/HTP/decode, cascade wait,
  alignment, FR HTP, embedding normalize, search, temporal decision, overlay/encode/ring.
- [ ] Báo p50/p95/p99, queue age/high-water, drop reasons, in-flight owners, pool bytes,
  allocations/frame, copied bytes/frame, CPU theo thread, RSS, HTP load và thermal state.
- [ ] Phân biệt video FPS, FD results/s, FR crop jobs/s, identity refresh/track và latency từ
  capture đến AI result/ring. Một RTSP stream 30 FPS không chứng minh bbox/identity cập nhật 30 Hz.
- [ ] Mục tiêu người dùng: realtime 25–30 FPS, CPU 15–25%. Chốt số mặt/camera/gallery tương
  ứng và chuẩn CPU (% một core hay toàn SoC) trước nghiệm thu; chưa có số đo FR đạt mục tiêu.
- [ ] Frame period 30 FPS là khoảng 33.3 ms; đây là nhịp input, không tự là budget latency
  cho toàn pipeline async. Chốt latency budget và allowed drops riêng theo workload.
- [ ] Đo ngân sách theo số mặt: primary cost + N×(alignment+FR+search), cộng scheduling/output;
  overlap chỉ tính khi có trace. N chưa giới hạn thì không thể cam kết realtime/CPU.
- [ ] Tối ưu theo hotspot: pooling, reuse graphs/crops, QTI/FastCV execution, QNN reusable
  registered memory, loại client-buffer copy khi đủ ownership/sync evidence; rồi mới xét
  shared context/async/batching theo capability và latency tradeoff.
- [ ] Temporal scheduling FR theo track là policy explicit, có max result age. Không giảm
  xuống cập nhật 1 giây/lần chỉ để đạt số CPU đẹp. Đo latency/accuracy sau mọi thay đổi cadence.
- [ ] Zvec query benchmark theo kích thước gallery, templates/person và mutation load; tối ưu
  worker/index trước khi viết SIMD/vector search mới.
- [ ] Soak và thermal test với thời lượng/workload đã duyệt; báo worst-case throttling,
  source reconnect, disk pressure và mixed person+FR workload nếu sản phẩm yêu cầu.

**Gate:** report tái lập được, có bundle hash/config/BSP/runtime và methodology. Nếu không
đạt CPU/latency, ghi hotspot và tradeoff cần quyết định; không tuyên bố đạt bằng throughput alone.

## 16. Ma trận kiểm thử tối thiểu

| Nhóm | Case bắt buộc |
|---|---|
| Package | Missing/unknown kind, contract mismatch, negative/overflow integer, tensor ABI mismatch, digest fail |
| FD | No face/multiple faces, border, rotated/scaled source, quantization, degenerate/NaN output, NMS overflow |
| Retention | Queue full, byte full, stale epoch, duplicate ticket, late completion, stop/restart, failed submit |
| Alignment/FR | Golden crop/input/embedding, wrong point order, singular transform, low quality, zero norm |
| Gallery | Empty/full, duplicate/update/delete, model isolation, crash at each commit stage, rebuild under queries |
| Recognition | Unknown/ambiguous, similar subjects, multi-template aggregation, track reuse, revision/cache invalidation |
| Attendance | Duplicate delivery, offline/reconnect, process restart, clock jump, multi-camera dedup policy |
| Performance | Faces/gallery sweep, output on/off, sustained load/thermal, concurrent mutation, bounded memory |

Dùng vectors tổng hợp cho index/unit tests. Dataset biometric được phép dùng phải có
storage/access/retention riêng ngoài Git; logs và CI artifacts không được chứa embeddings thật.

## 17. Definition of Done và bàn giao

- [ ] M0–M10 có evidence, owner review và các ngoại lệ được quyết định rõ.
- [ ] Chạy FR từ cấu hình/package; không sửa source theo tên model, endpoint hay threshold.
- [ ] Enrollment, live recognition, delete và restart recovery chạy trên board với FW API.
- [ ] Accuracy/calibration, latency/FPS/CPU, thermal/soak, ownership/drain đều có report riêng.
- [ ] Bộ test eSDK pass; native hardware tests được ghi đúng phạm vi, không nhầm với QEMU.
- [ ] Tài liệu kiến trúc, schema, ADR, module README, capability/status và runbook khớp source.
- [ ] Release bundle có version/digests, dependency licenses, health checks, rollback/migration,
  storage recovery và hướng dẫn vận hành; không kèm secrets/biometric dữ liệu phát triển.
- [ ] Có demo tái lập: enroll subject được phép → nhận diện → ghi event → restart → nhận diện
  tiếp → delete → không còn trả identity đã xóa.

## 18. Bước làm ngay và quản lý tiến độ

1. Tạo golden được phép dùng và chạy camera thật qua FD → retained alignment → EdgeFace;
   lưu tensor/latency evidence mà không log embedding sinh trắc.
2. Tách cascade execution khỏi service thread bằng bounded worker/completion state machine,
   rồi đo destination/input/output pooling và DMA/QNN copy hotspots trên board.
3. Hoàn thiện M6 gallery durable/recovery bằng synthetic vectors, sau đó nối recognition
   state M8 vào typed embedding results.
4. Nối enrollment M7 và attendance M9; benchmark từ đầu và đóng M10 sau khi workload cùng
   acceptance policy được chốt.

Mỗi work item ghi: owner, dependency, commit, tests, board evidence, giới hạn và trạng thái
`not_started / implementing / source_verified / board_verified / accepted`.
Không chuyển sang accepted chỉ vì compile hoặc demo một trường hợp thành công.
Không gán ngày hoàn thành chắc chắn trước khi chốt golden artifacts, FW storage/control
boundary, alignment capability và workload. Đây là dependency cần xử lý, không phải lý do
ngừng các phần việc độc lập đã được cho phép.

### Tài liệu tham chiếu

- [Cascade design và evidence](../architecture/cascade_inference.md)
- [Gallery/Zvec ADR](../adr/0004_fr_gallery_and_vector_index.md)
- [Qualcomm preprocessing](../architecture/qualcomm_preprocessing.md)
- [QNN board validation](../testing/qnn_board_validation.md)
- [Qualcomm plugin reference](../research/qualcomm_plugins_reference.md)
- [Source inventory](../development/implementation_status.md)
- [Code convention](../development/code_convention.md)
