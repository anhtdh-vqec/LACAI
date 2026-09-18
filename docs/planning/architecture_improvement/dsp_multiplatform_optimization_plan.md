# Kế hoạch DSP và tối ưu đa nền tảng

Plan này định nghĩa yêu cầu hệ thống, kiến trúc đích, backlog và gate nghiệm thu để giảm ARM
CPU cho workload camera hiện tại và mở rộng đến 18 usecase mà không hardcode model vào FastRPC.

**Status:** planned — adapter legacy đã được harden và có baseline `.98`; ABI generic,
golden parity, soak và các mục tiêu CPU vẫn chưa được nghiệm thu. **Layer:** docs.
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
| SYS-PERF-01 | Workload hiện tại person + face + fire/smoke, preview 25 FPS: sustained process CPU `<= 12%` một core | `pidstat -u -t`, ít nhất 30 phút sau warmup; báo avg/p95/max và usr/sys riêng |
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
| SYS-MEM-04 | Không leak FD, map, rpcmem handle, QNN memhandle, graph hoặc scratch qua restart/reload | 100 start/stop/reload cycles và soak 8 giờ |
| SYS-MEM-05 | Sau warmup, PSS slope không vượt `1 MiB/giờ`; FD/map count không tăng đơn điệu | Linear slope + start/end/min/max; ngưỡng phải được xác nhận lại bằng 8 giờ evidence |
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

### 2.3 Defect và khoảng trống còn mở

1. ABI FastRPC production hiện có method theo tên person/SCRFD/fire-smoke; draft v1 và codec
   operation envelope đã có nhưng chưa được nối vào skeleton/runtime production.
2. Kernel dense v1 allocation-free đã nhận shape/class/quantization/transform/capacity bằng
   descriptor và có conformance cho person `8400/1` cùng fire/smoke `2100/2`. Tuy nhiên
   transport hiện vẫn là packed input, chưa có skeleton được BSP ký và fire/smoke production
   vẫn dùng portable CPU decoder.
3. SCRFD legacy cố định 640, ba level, hai anchor/cell và năm landmark.
4. DSP preprocess ABI không mang matrix/range/interpolation/normalization descriptor. Kernel
   dùng `ScaleDownMN` và một FastCV color operation cố định, trong khi manifest hiện khai báo
   bilinear + BT.709 limited. Chưa có golden chứng minh hai semantics tương đương.
5. QAIC 01.00.47 từ Hexagon SDK 5.5.7.0 đã sinh lại stub legacy từ IDL do LACAI sở hữu;
   stub chỉ khác bản cũ ở tên header include. `third_party/fastrpc_dsp` đã bỏ, nhưng
   license/owner của C compatibility và nguồn binary skeleton đang deploy chưa được ký.
6. Hexagon SDK 5.5.7.0 hiện có tại đường dẫn user cấp. `hexagon-clang` thiếu
   `libtinfo.so.5` trên host, nên build skeleton từ source và protocol v1 vẫn chưa có
   release receipt; eSDK vẫn là toolchain bắt buộc cho ARM C++/CMake/tests.
7. Neutral `tensor_blob` sở hữu vector bytes. QNN registered output vẫn phải memcpy về result
   owned mỗi frame; loại copy này cần owner/view + completion contract, không được xóa bằng cast.
8. Preview/source vẫn có memcpy đáng kể. Phải tách số CPU media path khỏi inference path trước
   khi quy lỗi cho decoder hoặc cDSP.
9. Bằng chứng startup peak và soak 8 giờ chưa có. Không được ghi “zero leak” từ một run ngắn.

Envelope v1 32 byte đã có codec C và negative tests cho version, length, operation,
capacity và domain generation. Dense payload 120 byte có canonical encoder, full bounds
validation, deterministic bounded NMS và cùng source đã build bằng eSDK/QEMU lẫn
`hexagon-clang -mv68`; person/fire-smoke shape là data, không có branch model. Đây vẫn chỉ là
D08/D10/D11 source evidence: chưa có registered-buffer transport, skeleton được ký hoặc
runtime activation. Compiler Hexagon 8.7.06 chạy trong probe với gói Ubuntu `libtinfo5`
giải nén riêng ở `/tmp`, không cài vào host; probe đó không phải build/signing receipt được
BSP phê duyệt.

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

- Project-owned IDL, wire structs, host adapter và custom kernels nằm dưới project-owned
  `src/adapters/qualcomm` hoặc một module DSP có naming/provenance rõ; không giả là vendor code.
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
| `yolov8n_person` | QNN HTP; legacy DSP preprocess + one-class postprocess | Golden NV12→tensor cho color/range/resize/quant; v1 dense descriptor; parity boxes/scores/NMS; current workload benchmark |
| `scrfd_500m_bnkps` | QNN HTP; legacy DSP preprocess + fixed anchor postprocess | Golden ba level/kps; inverse top-left transform; quant zero-point parity; ROI density benchmark |
| `edgeface_s_gamma_05` | QNN HTP; FastCV alignment/cascade | Golden align/template/normalize; ROI batch cap; embedding quality/privacy; no full-frame DSP substitution |
| `yolo11n_fire_smoke` | QNN HTP; DSP legacy preprocess; portable multi-class decoder | v1 dense multi-class DSP decoder cho 320/2100/2; class-aware NMS; smoke/fire golden/hard negatives |

Mỗi model chỉ “hoàn thiện” khi có model kit M0–M4: provenance/load, exact IO, preprocess
golden, decode golden và quality report. AI APP có thể hoàn thiện adapter source nhưng không tự
đóng quality gate nếu AI Model chưa bàn giao golden.

## 5. Kế hoạch thực hiện có thể giao agent

### P0 — Đóng correctness và provenance legacy

| ID | Owner | Công việc | Tiêu chí nghiệm thu |
|---|---|---|---|
| D00 | AI APP | Giữ các fix lease, fail-closed, exact tensor validation, quant sign và workspace | Full eSDK suite pass; negative tests shape/name/bytes/nonfinite/closed-DSP/FD reuse |
| D01 | AI APP+BSP | IDL legacy đã chuyển vào `src/adapters/qualcomm` và QAIC sinh stub khi build; kiểm toán C compatibility và binary skeleton còn mở | Có IDL, generator/version/hash/license cho từng file; file chưa rõ provenance không vào release |
| D02 | BSP+FW | Cấp approved Hexagon SDK 5.5.7, build container và signing/deploy procedure | Clean build skeleton từ IDL/source; digest reproducible; deploy `.98` không dùng binary copy tay |
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
| D06 | AI APP+BSP | Sustained/per-stage baseline | 30 phút current + 60 phút full; usr/sys, CPU ms, copies, FPS/drop/latency/DDR/thermal |
| D07 | AI APP | Memory baseline | 100 lifecycle cycles + 8 giờ; PSS slope, FD/maps/handles, sanitizer/reference tests |

### P2 — Contract và FastRPC v1 generic

| ID | Owner | Công việc | Tiêu chí nghiệm thu |
|---|---|---|---|
| D08 | AI APP | ADR + neutral descriptor/capability/completion contracts | Không vendor type; bounded fields; version/error fixtures; lead+BSP+Model review |
| D09 | AI APP+BSP | IDL v1, handshake, domain generation và error mapping | Old/new ABI không gọi nhầm ordinal; fuzz/negative length tests; reset generation test |
| D10 | AI APP | Reference operation backend | Chạy toàn bộ golden, deterministic tie/candidate caps; backend-independent result semantics |
| D11 | AI APP+BSP | cDSP kernels descriptor-driven | Approved DSP build; unknown enum/shape rejected; per-session bounded scratch |

### P3 — Current model verticals

| ID | Owner | Công việc | Tiêu chí nghiệm thu |
|---|---|---|---|
| D12 | AI APP+Model | Person dense vertical | Pre/post golden parity; exact 640/8400/1 descriptor là data, không code branch |
| D13 | AI APP+Model | SCRFD anchor vertical | Score/box/kps parity, edge clamp, top-left inverse transform và dense-face stress |
| D14 | AI APP+Model | Fire/smoke multi-class vertical | 320/2100/2 chạy cDSP v1; class-aware NMS parity; no CPU dense scan |
| D15 | AI APP+Model | Face align/embedding vertical | Bounded ROI batch, golden affine/normalize, embedding quality và privacy gates |

### P4 — Copy, startup và memory

| ID | Owner | Công việc | Tiêu chí nghiệm thu |
|---|---|---|---|
| D16 | AI APP | Neutral borrowed tensor result lease | Owner/view/completion contract; no per-frame result allocation/copy; decoder lifetime tests |
| D17 | AI APP+BSP | Registered input/output end-to-end | Cache/fence/completion proven; no early reuse; copied-bytes counter giảm đúng |
| D18 | AI APP | Staged activation và pre-sized bounded pools | Không cold stampede/busy-spin; phase counters; rollback trên partial start |
| D19 | AI APP+BSP | Source/preview memcpy investigation | Mỗi copy có owner/reason/bytes; loại copy chỉ khi FW DMA contract chứng minh an toàn |
| D20 | AI APP | Leak/reload hardening | SYS-MEM-01..06 đạt qua cycle+soak; fault injection alloc/RPC/reset |

### P5 — Capacity và acceptance

| ID | Owner | Công việc | Tiêu chí nghiệm thu |
|---|---|---|---|
| D21 | AI APP | Current workload A/B | `SYS-PERF-01`, golden, FPS/drop/latency/memory/thermal cùng đạt |
| D22 | Cả ba | 18-usecase scenario A/B | `SYS-PERF-02`, quality/coverage/admission reason và no hidden graph duplication |
| D23 | AI APP | Multiplatform conformance | Qualcomm + reference pass cùng fixtures; unsupported backend trả reason rõ |
| D24 | AI APP lead | Acceptance report và close review | Evidence paths, revisions, open deviations, owner sign-off; không còn blocker P0–P5 |

Mỗi agent nhận đúng một ID hoặc một vertical độc lập. Agent phải đọc contract liên quan,
không sửa sibling repo, dùng eSDK cho toàn bộ C++ build/test, chạy layout/docs gates và tạo
focused commit chỉ chứa task-owned changes. Board artifact nằm dưới `/opt/lacai`; không ghi
credential, private SDK hay model binary vào Git.

## 6. Lệnh và evidence bắt buộc

Mọi report phải lưu raw output cùng manifest, commit, binary/model/skeleton digest và timestamp.
Các lệnh dưới là khung; script chính thức phải validate PID/workload thay vì hardcode.

```bash
source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux
cmake --build build-esdk-full -j4
ctest --test-dir build-esdk-full --output-on-failure -j4

pidstat -u -r -d -w -p <pid> 1 1800
pidstat -u -t -p <pid> 1 1800
```

Board report phải có:

1. cold start từ process entry đến first accepted result, chia phase;
2. steady current workload ít nhất 30 phút;
3. 8 giờ memory soak và 100 start/stop/reload cycles;
4. perf/flamegraph hoặc trace tương đương cho usr và sys;
5. output FPS, accepted/dropped inference, queue age, latency p50/p95/p99;
6. HTP/cDSP/DDR/power/thermal nếu BSP tool hỗ trợ;
7. golden/quality report cùng artifact, không dùng visual-only parity;
8. teardown counts và reset/fault-injection disposition.

## 7. Gate đóng plan

- [ ] P0 provenance/toolchain/golden không còn blocker.
- [ ] FastRPC v1 dispatch theo operation descriptor, không theo model ID hoặc method model-name.
- [ ] Current four-model verticals có golden parity và explicit unsupported semantics.
- [ ] Unknown shape/layout/quantization/version fail trước kernel; accelerator failure không
  chạy CPU reference ngoài policy.
- [ ] Không per-frame allocation ở preprocess/postprocess; output copy có counter và owner.
- [ ] Completion/cache/reset evidence chứng minh không early reuse/unmap/ACK.
- [ ] `SYS-PERF-01` và `SYS-PERF-02` đạt cùng FPS/latency/quality/thermal gates.
- [ ] `SYS-MEM-01..06` đạt 8 giờ + lifecycle cycles; không gọi run ngắn là leak-free.
- [ ] Qualcomm và reference conformance pass; neutral layer không có vendor type.
- [ ] AI APP lead, BSP+FW lead và AI Model lead ký đúng phần ownership của mình.

Plan chưa được đóng ở revision hiện tại: candidate fixture gần nhất còn 19.47% CPU và startup đạt
90–95% trong khoảng năm giây; DMA-BUF production A/B chưa có. Hexagon toolchain/provenance và
model golden chưa đủ, fire/smoke postprocess vẫn ở ARM, preprocessing semantics chưa được
chứng minh, startup/soak chưa có acceptance evidence.

## Giới hạn và công việc tiếp theo

- Candidate hardening đã có board fixture smoke; vẫn cần released-FW DMA-BUF A/B vì QAIC-copy
  memfd không thay board acceptance của registered input.
- Mục tiêu `<=12%` và `<=80%` là yêu cầu sản phẩm do AI APP lead đặt; D04 phải đóng workload
  trước khi so số.
- SDK 5.5.7.0 đã được cấp và QAIC sinh được legacy/v1 draft. P2/P3 cDSP binary vẫn phụ
  thuộc runtime `libtinfo.so.5` được BSP phê duyệt cho host compiler, ABI/ownership review,
  kernel source/build/signing và oracle semantics/quality của AI Model. Đây là dependency
  có owner, không phải lý do để copy thêm code legacy.

## See also

- [Architecture improvement master plan](README.md)
- [Contract and team scope](contract_and_team_scope.md)
- [Qualcomm FastRPC adapter](../../architecture/qualcomm_fastrpc_adapter.md)
- [Qualcomm QNN ION memory](../../architecture/qualcomm_qnn_ion_memory.md)
- [Model integration](../../contracts/model_integration.md)
- [QCS6490 board evidence](../../testing/qsc6490_board.md)
