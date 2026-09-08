# Qualcomm adapter — implementation blueprint

Status: original direct-SDK blueprint; see ADR 0002 for current plugin-backed implementation.
Baseline user-confirmed: QSC6490, Qualcomm Linux 1.8. Sysroot is not required to write code.
Historical direct-SDK blueprint below is not current implementation status.
Current plugin graph includes submission/result/drain source; runnable service and
preview output are missing. See implementation_status.md and fw_release_compatibility.md.
Preview renderer/encoder must be separate from the inference graph and preserve the
released H264/ring contract; selecting Codec2 requires board evidence, not just factory availability.
Read docs/research/qualcomm_plugins_reference.md for all plugin usage details.
Đọc kèm source review Q01–Q18.
Mục tiêu: FastCV image ops + QNN inference, neutral interfaces, safe ownership,
performance measured trên target. Không dùng OpenCV hoặc GStreamer trong core.

## 1. Module và ownership

| Planned file trong src/adapters/qualcomm | Trách nhiệm |
|---|---|
| vqec_vision_sdk_loader.cpp | allowed paths, symbols, versions, provider compatibility, RAII libs |
| vqec_vision_buffer_manager.cpp | AI-owned allocation/import/map/cache scope, pools, completion ownership |
| vqec_vision_fastcv_processor.cpp | crop/resize/color/rotate/normalize qua capability thực có; geometry metadata |
| vqec_vision_qnn_engine.cpp | backend/device/context/graph/tensor bind/execute/completion/profiling |
| vqec_vision_backend_factory.cpp | capability probe + construct backend theo board manifest |
| vqec_vision_c2d_processor.cpp | optional hardware blit/import path sau benchmark + ADR |

Private vendor headers ở thư mục này, không ở include/vqec/vision/ai/contracts.
Tên public override theo interface owner; helper dùng qcom + file_id registry.
Semantic postprocess ở perception/detection hoặc module decoder, không nhúng
YOLO/face rules trong qnn_engine. Vendor postprocess offload là implementation
tùy chọn có golden equivalence, không là điều kiện để mọi bài dùng adapter.

## 2. Capability descriptor bắt buộc

board_id, soc_id, bsp_build_id, adapter_version, sdk_build_ids;
accepted source pixel formats/modifiers/planes/alignments/max dimensions;
image ops + output dtype/layout + actual execution path;
memory import/export/map support + cache/sync requirements;
QNN backend id + accepted artifact/compiler/runtime versions;
graph/tensor restrictions, max contexts/inflight, thread safety;
sync/async/cancel/quiesce support, timeout recovery;
measured workload profiles và chưa-được-đo flags.

Không hardcode supported=true theo tên vendor. Thiếu library/symbol/version ->
unsupported với reason, không fallback CPU im lặng. Product policy cho phép
fallback phải explicit, có metric, budget và effective degraded state.

## 3. RAW4K -> model input

1. Validate descriptor, epoch, memory handles, bounds và negotiated profile.
2. Giữ frame lease; wait acquire synchronization theo camera/BSP contract.
3. Import hoặc map input qua BSP memory path đã xác nhận.
4. Tính ROI/alignment/letterbox; clamp đúng policy, không tự đổi model semantics.
5. Preprocess vào tensor/surface AI-owned trong bounded pool.
6. Hoàn tất toàn bộ source reads -> ACK Camera; release mapped/imported views
   theo lifetime được SDK bảo đảm.
7. Bind input tensor -> QNN execute -> completion -> decoder.
8. Release output sau mọi decoder consumer xong; recycle input khi inference xong.

Nếu nhiều model đọc cùng Camera frame, ACK sau consumer đọc cuối, hoặc tạo
intermediate chung có lifetime AI-owned. Một branch xong không được ACK cả frame.
Nếu passthrough trực tiếp Camera memory vào SDK, lease kéo dài đến SDK completion.

## 4. FastCV implementation

- Probe API thực có theo pinned headers/libs, không khai báo function pointer
  bằng chữ ký tự đoán từ một SDK khác.
- Bắt đầu với synchronous execute trong bounded worker; completion tạo sau return.
- fcv SetOperationMode dùng policy cố định; kiểm tra SDK global state và CleanUp
  trước cho phép nhiều processors cùng sống.
- Preallocate staging buffers theo model plans; ghi số lượng/bytes/copy metrics.
- NV12/NV21 plane order, stride và chroma ROI alignment kiểm tra riêng.
- Mapping CPU: cache begin/end theo BSP; device fence dependency là vấn đề khác.
- Không gọi normalize ở cả converter và tensor packing làm normalize hai lần.
- Dtype INT8/UINT8/FP16/FP32, layout NHWC/NCHW chỉ advertise sau golden pass.
- Resize-before-color-convert có thể tiết kiệm bandwidth nhưng khác numerical
  ordering: chỉ đổi nếu model team chấp nhận và golden/accuracy pass.
- Nếu phải xử lý input UBWC: yêu cầu BSP path decompression/import đã hỗ trợ
  hoặc negotiate linear NV12. Không map rồi coi UBWC là linear.
- C2D/GLES chỉ thêm nếu FastCV path không đạt target và SDK hỗ trợ; source review
  cho thấy có lựa chọn này, chưa chứng minh tối ưu trên board của sản phẩm.

## 5. QNN implementation

Init: verify artifact -> load backend/System libs -> enumerate compatible
providers -> create backend/device -> inspect binary metadata -> create context ->
select graph by manifest name -> validate all I/O -> allocate/bind -> warmup.

- Context binary là default candidate; .so model đường riêng nếu product cần.
- Kiểm version-tagged metadata/tensor unions trước đọc member; deep-copy metadata
  nếu memory nguồn sẽ được SDK release.
- Không assume first graph hoặc one input. Reject graph/name/dtype/shape mismatch.
- Quantization map chính xác theo SDK definition và Model contract; không tự coi
  SDK offset là cùng convention zero_point. Native dtype giữ riêng từng output.
- Execute sync ở v1 với bounded executor, serialize context trừ khi SDK xác nhận
  concurrent safe. Deadline là scheduler contract, không giả QNN hủy được.
- Shared memory registration là optimization phase sau client-buffer baseline.
  Chỉ implement nếu BSP SDK hỗ trợ allocator/import đó; register lifetime theo
  context + allocation; deregister sau completion trước free context/buffer.
- Client-buffer baseline có thể copy trong SDK: log path, đo, không gọi zero-copy.
- Không recreate backend/context hoặc dlopen model mỗi frame.
- Teardown reverse dependency sau drain; metadata/profiling callbacks không
  được gọi vào object/library đã hủy; kiểm partial-init mọi bước.

## 6. Completion và recovery

Job states: queued -> submitted -> completed/failed; cancel_requested là intent,
không là terminal state cho device đang dùng memory.

Stop source: stop submit -> bỏ queued -> await submitted completion -> release
lease -> release source. SDK stall: control thread vẫn phản hồi health;
quarantine bounded resources, đề nghị FW/BSP recovery. Recovery contract phải
xác nhận device access stopped trước tái sử dụng/free camera pool.
Process kill/restart chỉ được coi an toàn nếu BSP bảo đảm teardown DMA; cần test.
Không giữ mutex khi graphExecute/Finish/wait hoặc RPC.

## 7. Test matrix

| Gate | Test | Evidence |
|---|---|---|
| A0 SDK inventory | missing lib/symbol, ABI mismatch, invalid artifact | deterministic error + no partial-init leak |
| A1 buffer | padded stride, multiple planes, FD reuse, epoch change | no OOB, exact release, mapping counters |
| A2 preprocess | crop borders, rotate, letterbox, RGB/BGR, quantized data | input tensor golden tolerance |
| A3 inference | bin/so nếu support, graph select, mixed dtype, multi I/O, reordered outputs | tensor + decoded golden |
| A4 lifetime | slow execute, timeout, disable, disconnect, shutdown | no early ACK/use-after-free |
| A5 workload | RAW4K + live stream/record + feature combinations | latency/fps/CPU/RSS/thermal/copy report |
| A6 stability | repeated load/unload, 24–72h agreed soak, fault injection | no growing FD/RSS/pool leak, recovery trace |

24–72h là mục tiêu nghiệm thu đề xuất, không phải test đã chạy.
A0–A4 đạt trước optimization; A5 profile trước khi tăng thread/batch/context.
Hardware memory tests không thay thế bằng host sanitizer.

## 8. BSP inputs còn thiếu — blocker cho code production

SoC + board revision; BSP image; Linux/toolchain/sysroot; FastCV headers/libs;
QNN SDK and device libs including required accelerator dependencies; known-good
model/context + SDK demo; allocator/import/cache/fence samples; supported NV12
4K source mode; permissions/device nodes; thread/concurrency/reset documentation;
redistribution policy; thermal/load budgets. Owner BSP, deadline end week1/2.

For the direct-SDK path these inputs remain required. ADR 0002 allows writing the
plugin-backed adapter now without sysroot/build. Do not fabricate SDK APIs or
describe unbuilt source as a board-qualified streaming adapter.
