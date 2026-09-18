# Kế hoạch DSP và tối ưu đa nền tảng

Plan này định nghĩa yêu cầu hệ thống, kiến trúc đích, backlog và gate nghiệm thu để giảm ARM
CPU cho workload camera hiện tại và mở rộng đến 18 usecase mà không hardcode model vào FastRPC.

**Status:** accepted — image transform, dense và overlay generic v1 cùng full workload đã đạt
30.008 FPS và CPU trung bình 13.50% trong gate 5 phút do AI APP lead chốt trên `.98`.
**Layer:** docs.
**Source:** `src/adapters/qualcomm`,
`manifests/models`, `docs/architecture/qualcomm_fastrpc_adapter.md`.

## Trách nhiệm

- AI APP lead sở hữu kiến trúc runtime, neutral contracts, adapter Qualcomm/reference,
  scheduling, memory lifetime, profiler, integration và báo cáo acceptance end-to-end.
- BSP+FW sở hữu Hexagon SDK/toolchain/signing, FastRPC domain, allocator/cache/fence/reset,
  power/thermal và bằng chứng completion trên target. BSP không sở hữu thuật toán model.
- AI Model sở hữu model kit, preprocess/decode semantics, tensor identity, quantization,
  ontology, golden fixtures và quality report. AI APP không tự đoán các giá trị này.
- Thay đổi ABI, ownership/completion, quantization hoặc model semantics phải có review của
  AI APP lead và owner tương ứng. Không dùng kết quả “có box trên màn hình” thay golden parity.

## 1. Yêu cầu hệ thống AI APP

### 1.1 Phạm vi chức năng

Danh sách sản phẩm hiện tại có 18 usecase, không phải 18 graph độc lập. Runtime phải chia sẻ
model result và tracker state theo dependency DAG; không chạy lại person detector cho mỗi bài.

| Nhóm graph dùng chung | Usecase tiêu thụ điển hình | Hệ quả thiết kế |
|---|---|---|
| Person/face detector, tracker, attributes | blacklist, điểm danh, tuổi/giới tính, heatmap, xâm nhập, đếm, theo dõi người, tụ tập, bất thường | Một inference result fan-out đến nhiều feature; thuộc tính chạy theo ROI/cadence |
| PPE/smoking/object detector | hút thuốc, vũ khí, PPE | Có thể là detector riêng hoặc attribute head; catalog quyết định, không branch theo model ID |
| Fire/smoke detector | cháy/khói | Hai class, tensor 320 và 2100 predictions; không vừa kernel person legacy |
| Object/association tracker | bỏ quên/biến mất, truy vết đồ, hành lý/xe đẩy | Cần relation và temporal state; không đưa toàn bộ nghiệp vụ vào DSP |
| VLM/event verifier | cảnh báo theo ngữ cảnh, hành vi bất thường | Cadence thấp, admission riêng; không được chặn pipeline realtime |
| Vehicle/plate graph tương lai | ANPR và traffic camera | Cần vehicle/plate/lane/signal/calibration contract; không giả định person-only |

Mục tiêu 18 usecase phải được benchmark bằng graph DAG thực tế, cadence và ROI cap thực tế.
Không được nhân CPU của một graph với 18 hoặc gọi 18 feature rules là 18 model full-FPS.

### 1.2 Chỉ tiêu hiệu năng

CPU được chuẩn hóa theo một logical core: `100%` nghĩa là dùng trọn một core, không phải
100% toàn SoC. Workload acceptance phải pin source, preview/encode, model hashes, cadence,
ROI/object density, viewer/evidence state, governor và thermal window.

| ID | Yêu cầu | Gate đo |
|---|---|---|
| SYS-PERF-01 | Workload hiện tại person + SCRFD + EdgeFace cascade + fire/smoke, preview 30 FPS: process CPU trung bình `< 15%` một core | `pidstat` mỗi giây trong 5 phút sau warmup; báo avg/max và usr/sys riêng |
| SYS-PERF-02 | Workload sản phẩm đủ 18 usecase: sustained process CPU `<= 80%` một core | Scenario catalog ký bởi ba team; tối thiểu 60 phút, không nhân bản graph dùng chung |
| SYS-PERF-03 | Không có busy-spin lúc cold start; peak 1 giây và thời gian tới result đầu tiên được ghi riêng | `pidstat` 100 ms/1 s từ process entry; flamegraph cold path; ngưỡng peak do product profile ký sau baseline |
| SYS-PERF-04 | Không giảm CPU bằng cách giảm ngầm cadence, FPS, accuracy hoặc bỏ result | So sánh accepted inference, output FPS, drop, queue age và golden/quality cùng workload |
| SYS-PERF-05 | ARM hot path không cấp phát theo frame ở preprocess/postprocess | Allocation trace sau warmup bằng 0 cho các stage này; pool có capacity hữu hạn |
| SYS-PERF-06 | Báo `CPU ms/accepted inference` và `CPU ms/ROI`, không chỉ `%CPU` | Counter theo model/operation, p50/p95/p99 |

Ngưỡng cold-start chưa được tự đặt thành một con số tùy ý. D05 phải đo graph load, artifact
hash, `dlopen`, QNN compose/finalize, rpcmem page fault, DSP open/clock và first-frame allocation
riêng; sau đó AI APP lead ký peak/time-to-ready budget. Mục tiêu là loại busy loop, page-touch
trùng lặp và kích hoạt đồng thời không cần thiết, không che chi phí bắt buộc bằng warmup giả.

### 1.3 Bộ nhớ và lifetime

| ID | Yêu cầu | Gate đo |
|---|---|---|
| SYS-MEM-01 | Mọi queue, pool, mapping cache, candidate list và ROI batch đều bounded | Capacity từ validated configuration/catalog; test full/backpressure |
| SYS-MEM-02 | Không giữ hai output backing store nếu QNN registered output đã thành công | RSS/PSS và allocation trace khi activation; source inspection |
| SYS-MEM-03 | Không reuse/unmap/ACK khi cDSP/HTP còn có thể truy cập | Completion/quarantine test, FD reuse test, reset/disconnect test với BSP |
| SYS-MEM-04 | Không thấy tăng FD/thread hoặc tăng bộ nhớ không bounded trong acceptance hiện tại | So sánh đầu/cuối gate 5 phút; 100 cycle/8 giờ là qualification sản phẩm riêng |
| SYS-MEM-05 | Báo RSS/HWM đầu/cuối, không gọi run 5 phút là leak-free | FD/thread không tăng; mọi tăng RSS phải được nêu chính xác |
| SYS-MEM-06 | Dữ liệu tensor/biometric không được log hoặc dump mặc định | Security review và negative test |

Timeout, stop, source disconnect, FD close hay destructor không phải hardware completion. Nếu
BSP không chứng minh cancel/quiesce thì owner phải vào quarantine cho tới reset guarantee.

### 1.4 Đúng semantics và đa nền tảng

- Neutral contracts mô tả source planes, transform, dtype/layout/quantization, tensor roles,
  decoder semantics, source epoch và completion; không chứa QNN/FastRPC/FastCV/Gst types.
- Accelerator-required phải fail closed. CPU reference chỉ được chọn bằng policy rõ ràng,
  không tự chạy khi DSP open/RPC lỗi.
- Unknown layout, duplicate tensor role, shape/byte mismatch, non-finite scale/threshold và
  ABI revision không tương thích phải bị từ chối trước kernel access.
- Qualcomm adapter và reference backend cùng chạy golden fixtures. Backend khác có thể trả
  `unsupported` có reason; không được silently đổi precision, resize, range hoặc NMS.
- Tất cả claim hardware acceleration, zero-copy, model completion hoặc CPU target phải gắn
  với board/workload/date/artifact evidence.

## 2. Baseline và kết quả audit 2026-09-18

Baseline dưới đây đo binary đang chạy trên QCS6490 `.98` trước hai commit hardening mới;
đây là evidence chẩn đoán, không phải acceptance của source hiện tại.

| Mục | Kết quả |
|---|---|
| Workload | `yolov8n_person` + `scrfd_500m_bnkps` + `yolo11n_fire_smoke`, source 1920x1080@30, preview 25 FPS |
| Revision source gần deployment | `1cc2db2`; SHA-256 binary bắt đầu bằng `ac2c7b` |
| CPU 10 giây | `9.90% usr + 16.90% sys = 26.80%` một core |
| CPU 30.47 giây | `9.06% usr + 15.72% sys = 24.78%` một core |
| Memory ngắn hạn | RSS 430540–431432 KiB; PSS 406876→406768 KiB; FD 166–167 |
| Perf 10 giây | `__memcpy_generic` khoảng 8.75% main và 6.87% source; kernel symbols bị hạn chế |
| Kết luận | Chưa đạt `SYS-PERF-01`; cửa sổ 30 giây không chứng minh không leak |

Số user cung cấp cũng cho thấy fire standalone khoảng 12%, person standalone khoảng 16.9%
và full khoảng 26.6%. Hai nguồn đo phù hợp về thứ tự lớn. Phần `sys` và memcpy lớn cho thấy
không thể chỉ tối ưu NMS; cần xử lý result ownership/copy, source/preview path và cold allocation.

### 2.1 Candidate audit sau hardening

Candidate eSDK có SHA-256 bắt đầu bằng `9d78b5cd`, staged riêng trên `.98` ngày 2026-09-18.
Nguồn test là camera simulator 1920x1080@30 dùng memfd; workload và preview giống baseline.

| Mục | Kết quả |
|---|---|
| Production registration mặc định | FastRPC/kernel từ chối import memfd với `-22`; đây là đúng ranh giới capability, không phải bằng chứng DMA-BUF production |
| Fixture policy rõ ràng | `--allow-qaic-copy-input`; QAIC copy input rồi vẫn chạy remote cDSP kernel; kết quả CPU không đủ điều kiện nghiệm thu zero-copy/production |
| Cold start 1 giây | `90, 95, 95, 92, 92%`, sau đó `41%` rồi steady; phần lớn nằm trong QNN graph preparation |
| Steady CPU 30 giây | `6.66% usr + 13.63% sys = 20.29%` một core |
| Output ring | Ring v5 tăng 100 sequence trong 4 giây, tương ứng 25 FPS; slot 1920x1080 có payload H.264 |
| Memory smoke | Ở 138 giây: RSS 423464 KiB, PSS 399326 KiB, 150 FD; 30 giây sau RSS 422876–423004 KiB |
| Kết luận | Pipeline chạy end-to-end nhưng vẫn trượt `SYS-PERF-01`; startup spike, DMA-BUF A/B và soak còn mở |

PSS tăng trong phần warmup ngắn trước khi RSS phẳng trong cửa sổ 30 giây. Đây không phải bằng
chứng leak và cũng không chứng minh leak-free. D05–D07 vẫn cần đúng thời lượng và nguồn DMA-BUF
released-FW. Service chuẩn được khôi phục sau test; mẫu 8 giây sau warmup là 18.38% một core.

### 2.1.1 Registered-input fixture DMA-BUF

Ngày 2026-09-18, camera simulator được sửa pool theo ACK `buf_id` và thêm tùy chọn
`--dma-heap`; không còn quay vòng ghi đè slot khi frame còn in-flight. Trên `.98`, cả
`system` và `qcom,system` heap cấp phát/map/cache-sync được. Với `qcom,system`, candidate
FastRPC chạy policy mặc định (không bật QAIC copy input), ring tăng 100 sequence/4 giây
tức 25 FPS; không thấy lỗi map FD trong log. CPU process sau warmup đo 15 giây là
19.47% một core; RSS/HWM 390160 KiB và 163 FD tại một thời điểm. Đây chỉ là smoke
registered-input bằng fixture vẫn copy pixel từ QMMF, không phải released-FW DMA-BUF,
zero-copy, 30 phút CPU hay 8 giờ leak gate. Service/camera chuẩn đã được khôi phục và
ring chuẩn tăng 49 sequence/2 giây sau test. Chi tiết ở
[board evidence](../../testing/qsc6490_board.md).

### 2.1.2 Candidate cDSP overlay và direct encoder import

Ngày 2026-09-18, output adapter được chuyển từ `qtivoverlay` sang operation generic v1
`overlay_compose`. Kernel cDSP nhận descriptor NV12/box/label bounded, copy frame vào một trong
tám rpcmem surface, vẽ overlay rồi chuyển FD thẳng cho `v4l2h264enc` bằng GStreamer DMA-BUF
allocator chuẩn. Không còn QTI overlay plugin, QTI allocator hoặc cấp phát surface theo frame.

| Mục | Kết quả |
|---|---|
| ABI/kernel | Operation mask `18`: `dense_decode` + `overlay_compose`; system-client rpcmem input/output pass trên cDSP |
| eSDK logic | Full CTest **134/134** pass; overlay codec/kernel có copy, box, label và reserved-byte negative test |
| Registered DMA-BUF person | 20 giây: `10.04% usr + 3.95% sys = 13.99%` một core; RSS/HWM 246764 KiB, 21 threads |
| Memfd staging person | 20 giây: `11.10% usr + 5.75% sys = 16.85%` một core |
| Preview | 201 packet/8 giây = **25.125 FPS**, H.264 1920x1080; contact sheet đã review box/label/orientation/no-stale pass |
| So với person baseline | 13.99% so với 22.09%: giảm khoảng 36.7%; vẫn chưa đạt ngưỡng 12% |
| Perf registered path | Hot symbol còn lại chủ yếu là FastCV color conversion và horizontal/vertical scale của model preprocess; không còn `qtivoverlay` |

Kết quả này chứng minh hướng offload overlay và direct encoder import có lợi, nhưng không được
suy rộng thành acceptance full workload. Fire/smoke trong scene không có detection không tạo
preview output nên mẫu CPU fire không phải end-to-end encode evidence. Camera fixture DMA-heap
vẫn không thay thế released-FW cache/fence/completion acceptance.

### 2.1.3 Acceptance cuối: preprocess + dense + overlay generic v1

Candidate cuối ngày 2026-09-18 chuyển image transform sang FastRPC v1 descriptor-driven,
không dispatch theo model ID. cDSP resolve FastCV từ thư viện board, chỉ advertise operation khi
đủ symbol, reuse scratch và rpcmem output. Production bỏ hoàn toàn dependency legacy; SCRFD dùng
portable anchor-distance decoder qua neutral contract và EdgeFace giữ bounded ROI alignment.

| Mục | Kết quả |
|---|---|
| ABI/kernel smoke | Operation mask `19`; `image_transform`, `dense_decode`, `overlay_compose` pass |
| Build/test | Hexagon v68 clean build; eSDK/QEMU CTest **135/135** pass |
| Full workload | Person + SCRFD + fire/smoke primary; EdgeFace cascade được cấu hình |
| Preview | H.264 1920x1080; 241 packet/8 giây = **30.125 FPS** |
| Gate 5 phút | 9006 frame/300.119829 giây = **30.008 FPS** |
| CPU 5 phút | `6.53% usr + 6.96% sys = 13.50%` trung bình một core; max mẫu 1 giây 16.00% |
| Lifetime ngắn | FD `122 -> 122`; thread `48 -> 48`; RSS/HWM `+508 KiB` |
| Output | model slot 0/1/2 có result; `cascade_failed=0`; render failure 0; visual overlay pass |

Kết quả đạt đúng gate mới do AI APP lead chốt: full workload, 30 FPS, CPU trung bình dưới 15%
trong 5 phút. Mức tăng RSS nhỏ không phải bằng chứng zero leak. BSP signing, released-FW
DMA/cache/fence/reset, independent model quality và soak sản phẩm dài hơn vẫn là gate của owner
tương ứng, nhưng không chặn đóng plan AI APP này.

### 2.2 Những gì source hiện đã sửa

| Hạng mục | Trạng thái |
|---|---|
| Mapping lease | Cache bounded, identity không theo FD number, active lease chặn unmap/evict, clear có retirement |
| DSP failure | Production yêu cầu DSP session mở thành công; không silently chạy host C reference |
| Tensor validation | Exact role/count/shape/dtype/layout/byte count và finite quantization cho kernel legacy |
| Quantization | Neutral `(q - zero_point) * scale` được đổi đúng sang legacy `offset = -zero_point` |
| Workspace | Compact decoder result được reuse có khóa; CPU scratch chỉ cấp phát ở reference-test mode |
| QNN cold allocation | Registered output và heap fallback loại trừ nhau; bỏ zero-fill rpcmem trước execute |
| FastRPC input | QAIC pointer ABI dùng `remote_register_buf_attr2`; memfd tách khỏi DMA-BUF và chỉ được copy khi bật fixture policy rõ ràng |

Các mục trên là source/logic evidence. Chúng chưa chứng minh DSP binary provenance, golden
numeric parity, CPU target, released-FW DMA completion hay leak-free soak.

### 2.3 Defect đã đóng và khoảng trống ngoài plan

1. Production không còn mở ABI legacy theo tên model; board runtime cuối không chứa legacy
   skeleton. FastRPC v1 handshake, operation mask, domain generation và completion được dùng
   chung cho preprocess, dense và overlay.
2. Dense v1 allocation-free nhận shape/class/quantization/transform/capacity bằng descriptor;
   person `8400/1` và fire/smoke `2100/2` dùng cùng implementation. Packed multi-tensor vẫn có
   một ARM copy bounded và là tối ưu tiếp theo, không phải hardcode model.
3. SCRFD không còn buộc production vào kernel legacy; portable anchor-distance decoder được tạo
   từ package stage qua neutral contract. cDSP anchor-distance có thể thêm sau khi có cost/golden.
4. Image transform v1 đã mang plane/geometry/matrix/range/interpolation/placement/channel/
   normalization/dtype/quantization/capacity. Unsupported semantics bị reject trước RPC.
5. `third_party/fastrpc_dsp` đã bỏ. Project-owned v1 source được QAIC/Hexagon build tái lập;
   external FastCV được link/resolve, không copy source vendor vào LACAI.
6. Hexagon SDK 5.5.7.0 và `libtinfo.so.5` host-compat bền vững đã dùng để build v68. BSP signing
   và release receipt vẫn thuộc BSP, không bị mô tả nhầm là AI APP acceptance.
7. QNN registered output còn copy về neutral owned result. Muốn bỏ copy phải có borrowed-owner
   và hardware-completion contract; không cast để lách lifetime.
8. `qtivoverlay` và plugin preprocess đã rời production hot path. cDSP image transform và overlay
   dùng pool/scratch bounded; direct encoder import không cấp phát surface theo frame.
9. Startup vẫn có QNN preparation peak. Plan này chấp nhận steady-state theo gate 5 phút mới;
   cold-phase budget/staged activation là backlog riêng. Run ngắn không được gọi là leak-free.
10. Board đã chuẩn hóa schema v1 và layout tối thiểu `/opt/lacai`; candidate/test/legacy/scratch
    dư thừa đã xóa sau khi giữ evidence cần thiết.

Envelope v1 32 byte đã có codec C và negative tests cho version, length, operation,
capacity và domain generation. Dense payload 120 byte có canonical encoder, full bounds
validation, deterministic bounded NMS và cùng source đã build bằng eSDK/QEMU lẫn
`hexagon-clang -mv68`; person/fire-smoke shape là data, không có branch model. Đây vẫn chỉ là
D08/D10/D11 source evidence: chưa có registered-buffer transport, skeleton được ký hoặc
runtime activation. Compiler Hexagon 8.7.06 và hai clean build chạy với `libtinfo5` trong
user-local host-compat prefix bền vững. Kết quả này là source/reproducibility evidence, chưa
phải build/signing receipt được BSP phê duyệt.

## 3. Kiến trúc đích

```text
model kit + deployment
          |
          v
validated operation descriptors -----> capability/admission/cost model
          |                                      |
RAW lease + source ticket                        v
          |                         shared dependency DAG / cadence
          v                                      |
image_transform_port ----------------------------+
  Qualcomm: descriptor -> FastRPC v1 -> cDSP     |
  Reference: exact golden implementation         |
          |                                      v
registered tensor lease -> QNN HTP -> result tensor lease
          |                                      |
          +------------> detection_decode_port <-+
                           Qualcomm: cDSP compact decode/NMS
                           Reference: CPU golden
                                      |
                           neutral observations
                                      |
                       tracker / relation / feature fan-out
```

### 3.1 FastRPC v1 model-independent

Không sửa ordinal legacy. Tạo protocol v1 có `query_capabilities` và ABI revision trước mọi
submit. Wire dùng operation family và descriptor, không dùng model ID:

| Operation | Descriptor tối thiểu |
|---|---|
| `image_transform` | NV12 offsets/strides/view, crop, matrix/range/chroma, resize/interpolation, placement/pad, channel order, normalization, dtype/quantization, output shape |
| `dense_decode` | role bindings, prediction/class counts, box encoding, score activation/layout, thresholds, class-aware NMS, tie order, candidate/output caps, inverse transform |
| `anchor_distance_decode` | level count, grid/stride/anchors, score/box/kps roles, landmark ontology, quantization, inverse transform |
| `roi_align` | bounded ROI batch, source frame ticket, affine/template, interpolation, output normalization/quantization |

Handshake trả limits, supported enum values, max bytes/count/in-flight, domain generation,
cache/completion mode và operation version. Adapter reject descriptor ngoài capability trước
khi nhận RAW source. Device không đọc JSON/string; host validate package rồi marshal fixed-size
wire structs có length/version rõ ràng.

### 3.2 Source layout và provenance

- Project-owned IDL, wire structs, host adapter và custom kernels nằm dưới
  `src/adapters/qualcomm/dsp/{host,v1,legacy}` theo đúng ownership; không giả là vendor code.
- Chỉ QAIC-generated artifacts có thể ở generated/vendor area. Mỗi file ghi input IDL hash,
  QAIC/Hexagon SDK version, command, owner, license và regeneration check.
- Link QNN/FastCV/FastRPC runtime theo BSP package; không copy vendor library/private SDK.
- Skeleton `.so` là deployment artifact có digest/signing/compatibility receipt; receipt không
  thay signature verification hay path authorization.

### 3.3 Scheduling cho 18 usecase

- Catalog ánh xạ usecase → model dependencies → feature processors; activation manager tạo
  mỗi model/source association một lần rồi fan-out result.
- Cadence là rational phase có max in-flight, deadline, priority và busy-skip policy.
- Cascade face/attribute/plate chạy theo bounded ROI batch và refresh interval, không full-frame
  full-FPS mặc định.
- Admission dùng measured cost vector: ARM CPU, HTP, cDSP, DDR, memory, thermal và latency.
- Startup thực hiện validate/hash/compose theo dependency order; chỉ bật source sau khi resource
  pools sẵn sàng. Graph độc lập có thể staged activation theo policy để tránh cold stampede.

## 4. Hoàn thiện các model hiện tại

| Model | Hiện trạng | Việc phải làm trước production acceptance |
|---|---|---|
| `yolov8n_person` | Generic v1 cDSP preprocess + dense decode; QNN HTP; board accepted trong workload hiện tại | AI Model ký semantics/quality độc lập; BSP ký skeleton/released-FW completion |
| `scrfd_500m_bnkps` | Generic v1 cDSP preprocess; QNN HTP; portable neutral anchor-distance decode | AI Model ký golden ba level/kps/quality; cDSP decoder chỉ thêm nếu benchmark cần |
| `edgeface_s_gamma_05` | QNN HTP; bounded FastCV alignment/cascade; không chạy full-frame | AI Model ký align/embedding quality/privacy và ROI-density profile |
| `yolo11n_fire_smoke` | Generic v1 cDSP preprocess + dense multi-class decode `2100/2`; QNN HTP | AI Model ký smoke/fire golden, hard-negative và quality report |

Mỗi model chỉ “hoàn thiện” khi có model kit M0–M4: provenance/load, exact IO, preprocess
golden, decode golden và quality report. AI APP có thể hoàn thiện adapter source nhưng không tự
đóng quality gate nếu AI Model chưa bàn giao golden.

## 5. Kế hoạch thực hiện có thể giao agent

### P0 — Đóng correctness và provenance legacy

| ID | Owner | Công việc | Tiêu chí nghiệm thu |
|---|---|---|---|
| D00 | AI APP | Giữ các fix lease, fail-closed, exact tensor validation, quant sign và workspace | Full eSDK suite pass; negative tests shape/name/bytes/nonfinite/closed-DSP/FD reuse |
| D01 | AI APP+BSP | IDL legacy đã chuyển vào `src/adapters/qualcomm` và QAIC sinh stub khi build; kiểm toán C compatibility và binary skeleton còn mở | Có IDL, generator/version/hash/license cho từng file; file chưa rõ provenance không vào release |
| D02 | BSP+FW | SDK 5.5.7, persistent user-local host compatibility và clean reproducible source build đã có; còn approve build host, signing/deploy procedure | Digest reproducible đã đạt ở source gate; cần BSP receipt và deploy `.98` không dùng binary copy tay |
| D03 | AI Model | Bàn giao golden M0–M4 cho bốn model | Fixtures hợp lệ/lỗi; tolerances và quality owner ký |

Theo xác nhận của AI APP lead, đường đang chạy là baseline hồi quy ban đầu. AI APP có thể
capture input cố định và output hiện tại, ghi hash artifact/binary, source epoch, config,
tensor bytes/observations và tolerance để khóa regression. Điều đó **không** tự xác nhận
semantics BT.709/bilinear/quantization hoặc độ chính xác nghiệp vụ; AI Model vẫn phải review
M2–M4 và quality/hard-negative. D03 được chia thành capture regression (AI APP) và ký oracle
semantics/quality (AI Model), không trì hoãn capture chỉ vì chưa có bộ dữ liệu mới.

### P1 — Baseline có thể lặp lại

| ID | Owner | Công việc | Tiêu chí nghiệm thu |
|---|---|---|---|
| D04 | AI APP | Tạo workload manifest current và 18-usecase DAG | Pin hashes/cadence/source/preview/ROI/viewer/governor/thermal; schema review |
| D05 | AI APP | Cold-start trace theo phase | 100 ms/1 s CPU, faults, alloc/copy, time-to-first-result; không chỉ `top` screenshot |
| D06 | AI APP+BSP | Sustained current baseline | 5 phút full current theo gate lead; usr/sys, FPS và output health |
| D07 | AI APP | Memory smoke hiện tại | Đầu/cuối 5 phút có RSS/HWM, FD, thread; không claim leak-free |

### P2 — Contract và FastRPC v1 generic

| ID | Owner | Công việc | Tiêu chí nghiệm thu |
|---|---|---|---|
| D08 | AI APP | ADR + neutral descriptor/capability/completion contracts | Không vendor type; bounded fields; version/error fixtures; lead+BSP+Model review |
| D09 | AI APP+BSP | IDL v1, host handshake, domain generation và error/completion mapping đã source-delivered; unsigned open/execute/reopen `.98` đạt, còn BSP signing và in-flight reset | Old/new ABI không gọi nhầm ordinal; fuzz/negative length tests; reset generation test |
| D10 | AI APP | Reference operation backend | Chạy toàn bộ golden, deterministic tie/candidate caps; backend-independent result semantics |
| D11 | AI APP+BSP | cDSP kernels descriptor-driven | Approved DSP build; unknown enum/shape rejected; per-session bounded scratch |

### P3 — Current model verticals

| ID | Owner | Công việc | Tiêu chí nghiệm thu |
|---|---|---|---|
| D12 | AI APP+Model | Person dense production adapter đã source-delivered; board/golden còn mở | Pre/post golden parity; exact 640/8400/1 descriptor là data, không code branch |
| D13 | AI APP+Model | SCRFD anchor vertical: neutral portable production path đã chạy | AI Model còn ký score/box/kps quality và dense-face stress |
| D14 | AI APP+Model | Fire/smoke dùng chung dense production adapter; board/golden còn mở | 320/2100/2 chạy cDSP v1; class-aware NMS parity; no CPU dense scan |
| D15 | AI APP+Model | Face align/embedding vertical | Bounded ROI batch, golden affine/normalize, embedding quality và privacy gates |

### P4 — Copy, startup và memory

| ID | Owner | Công việc | Tiêu chí nghiệm thu |
|---|---|---|---|
| D16 | AI APP | Neutral borrowed tensor result lease | Owner/view/completion contract; no per-frame result allocation/copy; decoder lifetime tests |
| D17 | AI APP+BSP | Registered input/output end-to-end | Cache/fence/completion proven; no early reuse; copied-bytes counter giảm đúng |
| D18 | AI APP | Pre-sized bounded pools đã đạt steady-state; staged activation còn backlog | Không per-frame surface/scratch growth; cold phase được báo riêng |
| D19 | AI APP+BSP | Source/preview memcpy investigation — cDSP overlay/direct encoder import đã board-smoke; released-FW completion còn mở | Mỗi copy có owner/reason/bytes; loại copy chỉ khi FW DMA contract chứng minh an toàn |
| D20 | AI APP | Lifetime hardening trong scope plan | 5 phút FD/thread ổn định; long soak/reset fault là release qualification |

### P5 — Capacity và acceptance

| ID | Owner | Công việc | Tiêu chí nghiệm thu |
|---|---|---|---|
| D21 | AI APP | Current workload A/B | `SYS-PERF-01`, 30 FPS, output health và memory smoke cùng đạt |
| D22 | Cả ba | 18-usecase scenario A/B | `SYS-PERF-02`, quality/coverage/admission reason và no hidden graph duplication |
| D23 | AI APP | Multiplatform conformance | Qualcomm + reference pass cùng fixtures; unsupported backend trả reason rõ |
| D24 | AI APP lead | Acceptance report và close review | Evidence paths, revisions, open deviations; đóng AI APP scope, tách external release gates |

Mỗi agent nhận đúng một ID hoặc một vertical độc lập. Agent phải đọc contract liên quan,
không sửa sibling repo, dùng eSDK cho toàn bộ C++ build/test, chạy layout/docs gates và tạo
focused commit chỉ chứa task-owned changes. Board artifact nằm dưới `/opt/lacai`; không ghi
credential, private SDK hay model binary vào Git.

## 6. Lệnh và evidence bắt buộc

Mọi report phải lưu raw output cùng manifest, commit, binary/model/skeleton digest và timestamp.
Các lệnh dưới là khung; script chính thức phải validate PID/workload thay vì hardcode.

```bash
source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux
lacai_dsp_build="$(mktemp -d /tmp/lacai-esdk-dsp.XXXXXX)"
cmake -S . -B "$lacai_dsp_build" -DBUILD_TESTING=ON \
  -DVQEC_VISION_AI_ENABLE_CAMERA=ON -DVQEC_VISION_AI_ENABLE_CAMERA_DBUS=ON \
  -DVQEC_VISION_AI_ENABLE_GST_FRAME_BRIDGE=ON \
  -DVQEC_VISION_AI_ENABLE_QUALCOMM=ON -DVQEC_VISION_AI_ENABLE_FASTCV=ON \
  -DVQEC_VISION_AI_ENABLE_QNN_ENGINE=ON \
  -DVQEC_VISION_AI_ENABLE_MODEL_MANIFEST=ON \
  -DVQEC_VISION_AI_ENABLE_MODEL_CATALOG=ON \
  -DVQEC_VISION_AI_ENABLE_DEPLOYMENT_CONFIG=ON \
  -DVQEC_VISION_AI_ENABLE_FEATURE_CATALOG=ON \
  -DVQEC_VISION_AI_ENABLE_ARTIFACT_DIGEST=ON
cmake --build "$lacai_dsp_build" -j4
ctest --test-dir "$lacai_dsp_build" --output-on-failure -j4

pidstat -u -r -d -w -p <pid> 1 300
pidstat -u -t -p <pid> 1 300
```

Board report phải có:

1. cold start được ghi riêng, không trộn vào steady-state average;
2. full current workload chạy liên tục ít nhất 5 phút;
3. CPU usr/sys trung bình, max mẫu, output FPS, model/output error counts;
4. RSS/HWM, FD và thread đầu/cuối; không suy diễn leak-free;
5. codec/resolution/FPS và contact sheet overlay được review;
6. exact service/skeleton/script digest và raw evidence path;
7. các gate chưa chạy được gắn đúng owner, không bị che bằng visual parity.

## 7. Gate đóng plan

- [x] Toolchain, source provenance và baseline ABI v1 đủ cho scope AI APP; BSP signing và AI
  Model quality được tách thành release gate có owner.
- [x] FastRPC v1 dispatch theo operation descriptor, không theo model ID hoặc method model-name.
- [x] Current four-model runtime verticals chạy end-to-end; unsupported semantics fail closed.
- [x] Unknown shape/layout/quantization/version fail trước kernel; accelerator failure không
  chạy CPU reference ngoài policy.
- [x] Preprocess/overlay scratch và surface pool bounded, reuse sau warmup; copy còn lại có owner.
- [x] Synchronous completion giữ mapping/source/output owner; uncertain completion quarantine.
- [x] `SYS-PERF-01` đạt 30.008 FPS và 13.50% CPU trung bình trong 5 phút.
- [x] Memory smoke có FD/thread ổn định và RSS/HWM được báo đúng; không claim leak-free.
- [x] Qualcomm và reference logic conformance pass; neutral layer không có vendor type.
- [x] AI APP lead đã định nghĩa và chấp nhận gate hiện tại; BSP+FW và AI Model giữ release gate
  của họ, không bị AI APP tự ký thay.

Plan được đóng cho scope AI APP hiện tại. `SYS-PERF-02` (18-usecase product scenario), BSP-signed
skeleton, released-FW cache/fence/reset, independent model quality và long soak là công việc của
capacity/release qualification tiếp theo; chúng không được mô tả là đã hoàn tất.

## Giới hạn và công việc tiếp theo

- Generic cDSP preprocess/dense/overlay đã có board fixture registered-DMA acceptance; vẫn cần
  released-FW DMA-BUF completion vì fixture không thay acceptance của FW/BSP.
- Mục tiêu `<=80%` cho đủ 18 usecase vẫn cần scenario DAG/cadence/ROI chính thức; không ngoại suy
  trực tiếp từ workload bốn model hiện tại.
- SDK 5.5.7.0 đã được cấp, QAIC sinh được legacy/v1 draft và host compiler có user-local
  `libtinfo.so.5` bền vững. P2/P3 cDSP binary vẫn phụ thuộc BSP phê duyệt build host,
  signing/release receipt và oracle semantics/quality của AI Model. Đây là dependency có owner,
  không phải lý do để copy thêm code legacy hoặc mở lại plan AI APP đã đạt gate.

## See also

- [Architecture improvement master plan](README.md)
- [Contract and team scope](contract_and_team_scope.md)
- [Qualcomm FastRPC adapter](../../architecture/qualcomm_fastrpc_adapter.md)
- [Qualcomm QNN ION memory](../../architecture/qualcomm_qnn_ion_memory.md)
- [Model integration](../../contracts/model_integration.md)
- [QCS6490 board evidence](../../testing/qsc6490_board.md)
