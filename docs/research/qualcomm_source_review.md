# Qualcomm source review

Review date: 2026-09-09.
Local repository: `/home/a/Workspace/gst-plugins-qti-oss`
HEAD: `0cdf24a99c625fa616564ebf82fd8813c744ed82`.
git status --short tại lúc review: sạch. Không thay đổi repository tham khảo.

Hướng dẫn tích hợp hiện hành được chuẩn hóa tại
[qualcomm plugin adapter reference](../architecture/qualcomm_plugin_adapter_reference.md).
Tài liệu này giữ các quan sát source và giới hạn bằng chứng để làm cơ sở review.

## Phạm vi và mức độ bằng chứng

Đã đọc tập trung implementation liên quan đường AI: QNN loader/execute/cleanup,
FastCV conversion/sync/mode, converter normalization, C2D completion/import,
DMA allocator, ML converter transform/pools, object-detection sample wiring,
postprocess module và tracker dependency. Đã khảo sát danh mục sample multistream.
Không phải audit toàn bộ repository, codec/camera stack, mọi model decoder hoặc
SDK proprietary. Chưa compile/run trên board, chưa đo throughput/accuracy.
Source local là bằng chứng hành vi của bản plugin này, không là cam kết mọi SDK.

Mọi build/test/CMake của LACAI dùng eSDK tại `/home/a/Workspace/eSDK`; việc đọc
source plugin không thay thế kiểm tra runtime trên image QSC6490.

Các đường dẫn dưới đây tương đối với repository trên; line number là snapshot.

## Evidence -> quyết định thiết kế

| ID | Source / symbol | Quan sát trực tiếp | Áp dụng cho AI APP |
|---|---|---|---|
| Q01 | gst-plugin-mlqnn/ml-qnn-engine.cc:520 setup_backend | dlopen backend, getProviders, chọn providers[0], tạo backend/device/profiler | loader chọn provider tương thích ABI có kiểm tra; không lấy phần tử đầu mù quáng |
| Q02 | cùng file:689 setup_cached_graphs | QNN System đọc binary metadata, contextCreateFromBinary, graphRetrieve | ưu tiên context binary đã pin SDK/target; validate metadata trước run |
| Q03 | cùng file:804 setup_uncached_graphs | model .so, composeGraphs, contextCreate, graphFinalize | đường compatibility cho .so riêng; không giả định bin và so cùng quy trình |
| Q04 | cùng file:1131 execute | gán clientBuf pointer, graphExecute đồng bộ, graph_infos[0] | async ở executor không có nghĩa SDK async; job giữ input/output; graph_name explicit |
| Q05 | cùng file:982–1000 và 1190–1210 | workaround output float32 và convert native->float | giữ dtype mỗi tensor; dequantize chỉ phần decoder cần |
| Q06 | gst-plugin-base/gst/video/fcv-video-converter.c:2146 compose | cảnh báo async unsupported; compose trực tiếp | backend công bố synchronous; worker wrapper cung cấp completion thật |
| Q07 | cùng file:2266 wait_fence, flush | warning Not implemented; wait trả true | không dùng stub làm fence/quiesce bảo đảm |
| Q08 | cùng file:653 stage buffer, 846 conversion, 2388 mode | có staging/copy; op modes low-power/performance/CPU offload/CPU performance | FastCV không đồng nghĩa zero-copy hoặc mọi op trên DSP |
| Q09 | gst-plugin-base/gst/video/video-converter-engine.c:260 normalize_ip | CPU loop cho normalization; index dựa width/bpp | dùng golden + stride-aware code; không copy giả định packed vào frame arbitrary |
| Q10 | cùng file:309 default_backend | ưu tiên GLES nếu build có, rồi C2D, rồi OCV; FCV khởi tạo mặc định | chọn FastCV explicit, không kế thừa backend auto selection |
| Q11 | gst-plugin-base/gst/video/c2d-video-converter.c:471,1189 | map device address; fence nội bộ GArray requests; Finish trước normalize | phân biệt completion token và Linux sync_file FD; C2D là option có gate |
| Q12 | gst-plugin-base/gst/allocators/gstqtiallocator.c:104 | DMA heap hoặc ION theo build; pool reuse và map | allocator phụ thuộc BSP; không hardcode allocator cho mọi Qualcomm |
| Q13 | gst-plugin-mlvconverter/mlvconverter.c:2320–2390 | output DMA sync START/END, compose gọi fence=NULL | cache sync không phải thay thế device dependency; audit input/output đầy đủ |
| Q14 | gst-sample-apps/gst-ai-object-detection/main.c:537,550,781,1169 | converter -> QNN -> postprocess; QNN backend HTP path | dùng làm vertical-slice benchmark tham khảo, không dùng qmmfsrc trong production AI |
| Q15 | gst-plugin-mlpostprocess/modules/object-detection/ml-postprocess-yolov8.cc | decoder/NMS C++ và tensor caps cụ thể | tên YOLO chưa đủ xác định output; decoder theo model manifest, không ở BSP |
| Q16 | gst-plugin-objtracker/algorithm/bytetrack/CMakeLists.txt + dataType.h | tracker code dùng Eigen | tracking là portable perception; không phải vendor hardware inference |
| Q17 | gst-plugin-base/gst/video/CMakeLists.txt | FCV/C2D/GLES/OCV compile conditional | không link nguyên base library mà vô tình mang OpenCV vào sản phẩm |
| Q18 | gst-plugin-mlqnn/README | cần QNN SDK riêng và tích hợp distro | OSS checkout không đủ để build đầy đủ adapter target |

## Những điều không được suy diễn

1. DMA-BUF camera có thể không import được trực tiếp vào QNN. Chưa thấy đường
   memRegister/memDeRegister trong ml-qnn-engine.cc đã khảo sát; phải kiểm SDK BSP.
2. QNN HTP không chứng minh preprocess, postprocess, tracker đều chạy HTP.
3. API fence gpointer của C2D wrapper không phải FD có thể gửi thẳng qua IPC.
4. Plugin output float32 là lựa chọn wrapper, không là yêu cầu universal của QNN.
5. Sample multistream có nhiều engine branch không chứng minh shared model scheduler.
6. Không đồng nhất qmmfsrc raw Bayer với video/x-raw NV12 post-ISP yêu cầu AI.
7. Metadata dimension/caps sample không thay thế Model Integration Package.

## Những điểm phải kiểm kỹ nếu tái sử dụng logic

- Chọn provider/version union bằng SDK API chính thức; không sao chép pointer
  arithmetic đọc interface metadata.
- Validate số tensor và output-name mapping độc lập input count/output count;
  plugin có shared graphindices cần regression cho subset/reordered outputs.
- Cleanup partial-init và buffer đọc binary: audit RAII trên tất cả error exits,
  không suy ra an toàn chỉ vì mẫu có free().
- Cache device surfaces không chỉ dựa integer FD tái sử dụng.
- CPU normalization của sample cần xác minh stride/range/quant rounding cho model.
- FastCV operation mode/cleanup lifetime có thể ảnh hưởng nhiều instance: phải hỏi
  SDK về thread/global semantics trước parallel sessions.
- Per-file BSD-3-Clause-Clear và tracker có nguồn/license riêng: giữ notice khi
  reuse, rà soát quyền binary SDK riêng. Chưa nhập third-party code vào workspace.

## Hướng sử dụng

Updated by ADR 0002: first implement a private plugin-backed adapter on the
user-confirmed QSC6490 / Qualcomm Linux 1.8 baseline; direct SDK is optional later.
The original direct-SDK preference below is historical, not a blocker for coding.
Benchmark riêng: dùng vendor sample trên cùng model/input/board để đối chiếu.
Không fork nguyên plugin stack thành core AI; không thêm pipeline encode/display
chỉ để thực hiện inference. Các backend C2D/GLES là tối ưu tùy capability, không
mặc định thay FastCV đã chọn mà không có review.
