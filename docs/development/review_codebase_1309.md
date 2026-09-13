Mình đã đi sâu vào các đường critical path của repo: `frame_source`, DMA-BUF/GStreamer bridge, Qualcomm plugin graph, owned-QNN engine, multi-model pump/session, multi-source supervisor, runtime composition, feature pipeline, output preparation, CMake, ADR và implementation status. Kết luận của mình là:

> **LACAI đang có nền kiến trúc tốt hơn khá nhiều so với một prototype AI camera thông thường, đặc biệt ở ownership/lifecycle/fail-closed. Nhưng hiện tại nó là một “production-oriented architecture skeleton” hơn là một production AI runtime.**
>
> Rủi ro lớn nhất bây giờ không phải code xấu, mà là **architecture đang đi trước hardware vertical slice quá xa**.

README của chính repo cũng khá trung thực về trạng thái này: camera adapter, scheduler, Qualcomm plugin và QNN engine đã có source; nhưng chưa có live FW/model integration, chưa có real decoder/tracker/feature/renderer/hardware encoder, QNN engine vẫn sync/copy và chưa có performance/thermal/hardware-completion qualification. ([GitHub][1])

## Đánh giá tổng thể

| Mảng                             | Điểm mình chấm | Nhận xét                                         |
| -------------------------------- | -------------: | ------------------------------------------------ |
| Architecture boundary            |       **9/10** | Tách FW / neutral core / vendor adapter rất đúng |
| Buffer ownership & lifecycle     |       **9/10** | Đây là phần mạnh nhất                            |
| Defensive programming            |       **9/10** | Fail-closed rất tốt                              |
| Testability                      |     **8.5/10** | Contract test khá bài bản                        |
| Qualcomm/QCS6490 integration     |     **6.5/10** | Có nền tốt, chưa thành vertical slice            |
| Scheduler / multi-camera         |     **6.5/10** | Logic đúng nhưng chưa đủ cho load thực           |
| Multi-model product architecture |       **6/10** | Frame fan-out tốt, cascade inference chưa đủ     |
| Performance readiness            |       **4/10** | Owned QNN hiện chưa phải hot-path production     |
| Output/video production path     |       **3/10** | Còn thiếu renderer + encoder thật                |
| Maintainability khi team lớn     |     **6.5/10** | Naming/CMake có dấu hiệu over-engineering        |
| Production readiness hiện tại    |    **~4.5/10** | Foundation tốt, nhưng chưa có E2E hardware proof |

Nếu chỉ đánh giá **foundation**, mình cho khoảng **8/10**. Nếu hỏi “đem lên QCS6490 làm camera box production ngay chưa?” thì chưa.

---

# 1. Phần mình thích nhất: boundary của bạn đang đúng

Architecture này:

```text
FW
 │ RAW NV12 + FD lease
 ▼
raw_source_port
 │
 ▼
multi_source_supervisor
 │
 ▼
multi_model_session
 │
 ├── image_processor_port
 │
 └── inference_graph_port
       │
       ▼
 perception / tracking
       │
       ▼
 features
       │
       ▼
 output_gate
       │
       ▼
 overlay / H264 / FW ring
```

là direction mình đồng ý.

Bạn giữ vendor SDK, QNN và GStreamer dưới `src/adapters/`, trong khi application orchestration chỉ nhìn neutral port. Đây là boundary rất có giá trị nếu sau Qualcomm còn có Rockchip/MediaTek/Novatek. ([GitHub][1])

Đặc biệt mình **không khuyên bỏ abstraction này để viết thẳng QNN/GStreamer khắp application**.

Bạn cũng đã làm đúng một việc mà rất nhiều camera codebase làm sai: contract không chỉ chứa pixel dimensions mà còn explicit:

* ownership;
* epoch;
* buffer identity;
* clock;
* tensor layout;
* dtype/quantization;
* resource/inflight limits.

README còn quy định rõ không được release buffer khi hardware có thể vẫn đang đọc. ([GitHub][1])

Đây là mindset production đúng.

---

# 2. `frame_source` là đoạn code mình đánh giá cao nhất

Camera IPC implementation khá chắc.

Bạn đang dùng:

```cpp
SOCK_SEQPACKET
SCM_RIGHTS
MSG_CMSG_CLOEXEC
SO_PEERCRED
```

và verify UID của producer theo deployment policy. Code còn reject malformed ancillary data, yêu cầu đúng một FD, reject duplicate/non-increasing buffer token và giới hạn số camera frame lease đang sống. ([GitHub][2])

Ví dụ đoạn:

```cpp
getsockopt(... SO_PEERCRED ...)
...
credentials.uid != _config.producer_uid_
```

là một chi tiết security/robustness tốt mà prototype thường bỏ qua. ([GitHub][2])

Tương tự:

```cpp
reader_count_->outstanding_.load() >= g_max_live_frames
```

cho thấy bạn đang thực sự kiểm soát bounded resource thay vì để camera FD pile up. ([GitHub][2])

Điểm này mình cho **9/10**.

---

# 3. Nhưng ACK buffer trong destructor là thứ mình muốn sửa sớm

Hiện tại khi owner cuối cùng của frame chết:

```cpp
const auto sent = ::send(
    session_->socket_fd_,
    &token,
    sizeof(token),
    MSG_NOSIGNAL | MSG_DONTWAIT);

if (sent != sizeof(token))
    session_->healthy_ = false;
```

rồi close FD và giảm outstanding counter. ([GitHub][2])

Ý tưởng:

> destructor của final owner = lease completion boundary

là **đúng**.

Nhưng transport ACK ngay trong destructor bằng nonblocking `send()` có vấn đề production.

Giả sử socket send buffer đang đầy:

```text
final frame owner destroyed
       ↓
send ACK
       ↓
EAGAIN
       ↓
session unhealthy
       ↓
local FD closed
```

Nhưng FW không nhất thiết đã biết buffer đó được release.

Nếu protocol phía FW chỉ recycle buffer sau khi nhận ACK, một lần EAGAIN có thể biến thành:

```text
FW buffer stuck
 → available RAW buffers giảm
 → camera pipeline starvation
 → reconnect/recovery
```

Destructor không phải chỗ thích hợp để retry I/O.

Mình sẽ chuyển thành:

```text
received_frame::~received_frame()
        │
        └─ enqueue(buffer_id)
                 ↓
        release_dispatcher
                 │
            nonblocking send
                 │
       ┌─────────┴─────────┐
       │                   │
     success             EAGAIN
       │                   │
     done              retry later
                           │
                    bounded deadline
                           │
                       fault source
```

Destructor chỉ **handoff ownership**, còn một `release_dispatcher` chịu trách nhiệm gửi release token.

Đây là thay đổi P0/P1 của mình.

---

# 4. DMA-BUF bridge rất đúng hướng, nhưng chưa được gọi là zero-copy production

Đoạn bridge của bạn:

```cpp
fcntl(fd, F_DUPFD_CLOEXEC)
       ↓
gst_dmabuf_allocator_alloc()
       ↓
GstMemory
       ↓
GstBuffer
```

sau đó giữ `shared_ptr` owner bằng qdata trên `GstMemory` là giải pháp khá đẹp. Khi GstMemory cuối cùng chết, owner camera mới được release. ([GitHub][3])

Đây là một pattern tốt:

```text
FW DMA-BUF
    │
    ▼
raw_frame owner
    │
    ▼
GstMemory ─── holds raw_frame owner
    │
    ▼
GStreamer/QIM
```

Tức **không copy pixel chỉ để đưa FD vào GStreamer**.

Nhưng chính documentation của repo cũng đúng khi chưa gọi nó là proven zero-copy: lifetime của `GstMemory` không tự động chứng minh accelerator đã hoàn tất DMA đọc buffer. Repo cũng ghi hardware-completion qualification vẫn chưa có. ([GitHub][1])

Có thêm một micro-optimization đáng làm sau này:

```cpp
allocator_owner allocator{gst_dmabuf_allocator_new()};
```

đang tạo allocator trong mỗi `wrap_frame()`. ([GitHub][3])

Ở 1 camera không đáng kể.

Ở:

```text
16 camera × 15 fps = 240 frames/s
```

thì không cần thiết tạo/destroy allocator object mỗi frame.

Mình sẽ giữ một:

```cpp
dmabuf_bridge_context
{
    GstAllocator* allocator;
    ...
};
```

theo lifecycle của adapter.

Nhưng đây chưa phải P0. Đo trước rồi sửa.

---

# 5. Qualcomm plugin backend của bạn: direction rất hợp QCS6490

Bạn đang build private pipeline:

```text
appsrc
  ↓
qtimlvconverter
  ↓
capsfilter
  ↓
qtimlqnn
  ↓
appsink
```

và `qtimlvconverter` đang chọn `engine=fcv`. ([GitHub][4])

**Mình thích backend này. Đừng xem nó là backend “tạm” rồi vội bỏ.**

Qualcomm chính thức cũng dùng đúng pattern preprocess → ML framework → post-process trong QIM SDK; IM SDK còn có các plugin cho object tracking, overlay, encoder/RTSP và sample multi-stream tới 32 streams. ([GitHub][5])

Nó nên là **performance/reference baseline** của bạn.

Tức trước khi owned-QNN được tuyên bố tốt hơn:

```text
Plugin backend
qtimlvconverter → qtimlqnn
```

phải được benchmark cạnh:

```text
Owned backend
your preprocess → QNN API
```

trên **cùng model/cùng input/cùng FPS**.

Đừng assume raw QNN API nhanh hơn QIM plugin.

---

# 6. Vấn đề lớn nhất hiện tại: owned QNN backend chưa phải production hot path

Đây là phần mình quan tâm nhất.

Code tự khai báo capability rất trung thực:

```cpp
max_inflight_jobs_ = 1;
supports_async_ = false;
supports_shared_memory_ = false;
supports_artifact_update_ = false;
supports_multi_model_domain_ = false;
```

và comment nói rõ chỉ hỗ trợ synchronous client-buffer execution. ([GitHub][6])

Đây là **thiết kế fail-closed rất tốt**.

Nhưng performance thì hiện tại:

```text
input std::vector<uint8_t>
           ↓
QNN_TENSORMEMTYPE_RAW
           ↓
graphExecute()       ← blocking
           ↓
std::vector output
```

Mỗi execute còn tạo:

```cpp
std::vector<tensor_blob> outputs;

for (...)
    blob.bytes_.resize(bytes);
```

trước khi gọi synchronous `graphExecute()`. ([GitHub][6])

Với 1 model, 1 camera: fine.

Với:

```text
8 camera
×
2–3 models
```

nó sẽ rất khác.

Đặc biệt `submit_tensors()` gọi `engine.execute()` ngay:

```cpp
const auto executed =
    engine_.vqec_vision_ai_qcom_qneng_execute(...)
```

nên abstraction bên ngoài:

```text
arm
submit
poll
outstanding
```

trông asynchronous, nhưng backend hiện thực tế vẫn thực hiện inference **ngay trong submit call**. ([GitHub][7])

Đây là vấn đề architecture/performance quan trọng nhất.

---

# 7. Hệ quả: một QNN execute có thể block scheduling của những source khác

`multi_source_supervisor::step()` hiện pick **một source**, gọi:

```cpp
selected->...step(...)
```

xong mới chuyển source lần sau. ([GitHub][8])

Nếu bên trong:

```text
source session
 ↓
multi model pump
 ↓
submit_tensors
 ↓
QNN graphExecute()
```

mất 25 ms, thì thread orchestrator đó mất 25 ms.

Có nghĩa:

```text
Camera 0
  ↓
QNN 25ms
  ↓
Camera 1
  ↓
QNN 25ms
  ↓
Camera 2
...
```

Đây không phải concurrency mà hardware pipeline cần.

Về lâu dài mình muốn:

```text
                  ┌─ QNN job worker/context
source scheduler ─┼─ QNN job worker/context
                  └─ ...
       │
       └── không block
```

hoặc tốt hơn sử dụng QNN async API khi qualification xong.

ADR của bạn đã dự kiến async + registered/shared memory + shared HTP execution domain, nên hướng đã đúng; chỉ là implementation hiện mới dừng trước phase đó. ([GitHub][9])

---

# 8. Direct QNN path còn thiếu một mảnh rất lớn: production preprocess

`qnn_inference_graph::submit_frame()` hiện trả:

> QNN graph requires preprocessed tensor submission from a neutral preprocessing stage. ([GitHub][7])

Pump vì vậy phải:

```cpp
processor_->preprocess(frame, ..., blobs);
graph.submit_tensors(blobs);
```

và intermediate chính là:

```cpp
std::vector<tensor_blob> blobs;
```

([GitHub][10])

Nhưng CMake của QNN backend hiện chỉ compile:

```text
vqec_vision_sdk_loader.cpp
vqec_vision_qnn_engine.cpp
vqec_vision_qnn_inference_graph.cpp
vqec_vision_backend_factory.cpp
```

Không có `vqec_vision_buffer_manager.cpp`, không có Qualcomm `fastcv_processor`, trong khi ADR đã reserve `buffer_manager` cho registered/shared-memory phase. ([GitHub][11])

Đây là lý do mình nói **docs đang đi trước source**.

Ở QCS6490, mình sẽ coi:

```text
NV12 DMA-BUF
 → accelerated preprocess
 → registered/persistent QNN input
 → HTP
```

là milestone rất quan trọng.

Nếu cuối cùng thành:

```text
NV12 DMA-BUF
 → mmap
 → CPU resize
 → CPU color convert
 → std::vector
 → QNN
```

thì việc viết raw QNN backend gần như mất phần lớn ý nghĩa.

---

# 9. Multi-model pump làm ownership rất tốt

Có một đoạn mình đặc biệt thích.

Sau khi nhận frame, bạn tính cadence, rồi **arm/reserve tất cả graph due trước khi submit graph đầu tiên**, với comment rõ rằng shared backend retention domain phải fail trước khi bất kỳ graph nào đọc frame. ([GitHub][10])

Đây là cách suy nghĩ đúng cho:

```text
                   ┌ Model A
one RAW owner ─────┼ Model B
                   └ Model C
```

không để kiểu:

```text
A accepted frame
B failed reservation
→ ownership state nửa sống nửa chết
```

Ngoài ra nếu graph còn outstanding thì đánh `busy_model_mask` và không submit thêm. ([GitHub][10])

Mình cho phần ownership/scheduling safety này điểm cao.

---

# 10. Nhưng cadence scheduler hiện mới phù hợp “live detection”, chưa đủ QoS cho feature pipeline

Khi model tới cadence nhưng graph đang busy:

```cpp
busy_model_mask |= bit;
continue;
```

frame đó được skip. ([GitHub][10])

Với:

```text
person detection @ 5 FPS
```

rất hợp lý. Camera AI thường ưu tiên newest frame, không queue 20 frame cũ.

Nhưng với:

```text
face recognition
ANPR OCR
barcode
event snapshot classifier
```

semantics có thể khác.

Ví dụ một license plate chỉ xuất hiện 200 ms:

```text
frame due
   ↓
OCR model busy
   ↓
skip
   ↓
plate gone
```

Bạn cần QoS ở model/job level, kiểu:

```text
drop_if_busy
latest_wins
replace_pending
must_process_once
event_triggered
```

Không nên tất cả model dùng cùng một semantics cadence.

---

# 11. Mình thấy một vấn đề architecture lớn hơn: “parallel models” ≠ “AI application graph”

Feature pipeline hiện map result theo **một immutable model slot** rồi gọi fanout của slot đó. ([GitHub][12])

Điều này rất đẹp cho:

```text
model A detection
 ├ feature A
 ├ feature B
 └ feature C
```

Nhưng nhiều use case camera thật là:

```text
person detector
     ↓ ROI
face detector
     ↓ ROI
face embedding
     ↓
gallery match
```

hoặc:

```text
vehicle detector
     ↓
plate detector
     ↓
OCR
```

hay:

```text
person detector
     ↓
tracking
     ↓
pose model on selected tracks
```

Đây không phải:

```text
one frame → N independent models
```

mà là:

```text
primary inference
      ↓
ROI/task generation
      ↓
secondary inference
      ↓
join with track/object identity
```

Mình nghĩ LACAI hiện **thiếu explicit secondary-inference/task graph contract**.

Nếu không thêm, sau này feature developer sẽ bắt đầu bypass architecture:

```cpp
feature_processor {
    call QNN manually;
}
```

và lúc đó neutral architecture bị thủng.

Mình sẽ bổ sung concept dạng:

```text
secondary_inference_request
{
    source_epoch
    source_frame_id
    track_id
    model_id
    ROI
    deadline
    priority
}
```

rồi đưa nó quay lại inference scheduler.

Đây là thứ mình đánh giá **quan trọng hơn việc support 16 independent models/source**.

---

# 12. Multi-source supervisor: fairness tốt, nhưng fault propagation hơi nguy hiểm

Bạn round-robin source:

```cpp
next_source_index_ = selected_index + 1;
```

nên không để một source độc chiếm loop. Good. ([GitHub][8])

Nhưng đoạn:

```cpp
if (source_code != ok && source_code != pending) {
    return pending;
}
```

có nghĩa lỗi của source được giữ trong `_report.source_status_`, còn return value của supervisor lại thành `pending`. ([GitHub][8])

Mình hiểu ý đồ: **một camera chết không kéo toàn bộ 16 camera chết**.

Ý đó đúng.

Nhưng production cần chắc chắn upper layer không chỉ check return code:

```cpp
if (supervisor.step().ok())
```

rồi bỏ qua report.

Mình sẽ có một health/event channel tách riêng:

```text
source0: healthy
source1: reconnecting
source2: faulted / camera disconnected
source3: healthy
```

và metrics:

```text
source_disconnect_total
source_reconnect_total
source_fault_duration
source_last_frame_age
```

Tức fault isolation giữ nguyên, nhưng **error không được biến thành invisible pending**.

---

# 13. Stop/drain semantics nhìn chung rất tốt

`multi_model_session` không release source ngay khi stop.

Nó:

```text
begin stop
 ↓
drain graphs
 ↓
poll outstanding
 ↓
unload/reconcile
 ↓
release source
```

([GitHub][13])

Đúng thứ tự.

Đây lại là một bằng chứng codebase của bạn suy nghĩ khá kỹ về ownership.

Một chi tiết product cần quyết định sau này: khi drain đang có completed tensor result, code hiện lấy vào một biến `discarded`. ([GitHub][13])

Tức stop semantics hiện gần:

```text
drain ownership, discard business result
```

Có thể đúng cho shutdown.

Nhưng nếu stop là:

```text
model hot-swap
configuration update
source restart
```

thì cần quyết định explicit:

```text
drain_and_deliver
```

hay:

```text
drain_and_discard
```

Đừng để implicit.

---

# 14. Điểm mình không thích: naming convention đang làm C++ khó đọc hơn

Ví dụ một method:

```cpp
multi_model_pump::
vqec_vision_ai_appl_mmump_resolve_targets()
```

hoặc:

```cpp
graph.vqec_vision_ai_ports_infgr_get_outstanding()
```

Convention bắt **mọi method public/private/static/helper** phải dùng:

```text
vqec_vision_ai_<dir_id>_<file_id>_<verb_object>
```

([GitHub][14])

Theo mình đây là over-engineering.

Trong C thuần:

```c
vqec_vision_ai_qcom_qneng_execute()
```

còn có lý.

Nhưng C++ đã có:

```cpp
namespace vqec::vision::ai

class qnn_engine {
public:
    status execute(...);
};
```

thì:

```cpp
engine.execute(...)
```

đã đủ scope.

So sánh:

```cpp
graph.vqec_vision_ai_ports_infgr_get_outstanding()
pump.vqec_vision_ai_appl_mmump_resolve_targets()
session.vqec_vision_ai_appl_mmses_request_stop()
```

với:

```cpp
graph.outstanding()
pump.resolve_targets()
session.request_stop()
```

Bản sau dễ review hơn rất nhiều.

Khi team từ 2 người thành 8–10 người, naming hiện tại tạo cognitive load rất lớn.

Mình sẽ giữ long-prefix cho:

```text
public C ABI
exported free functions
wire/plugin symbols nếu cần
```

nhưng **C++ class method nên dùng tên local bình thường**.

Mình không khuyên rename toàn repo ngay hôm nay vì churn rất lớn. Nhưng mình sẽ sửa convention trước khi codebase phình thêm 3×.

---

# 15. CMake cũng bắt đầu có dấu hiệu “too granular”

Root `CMakeLists.txt` hiện hơn 3.600 dòng render trên GitHub và khai báo rất nhiều static library nhỏ:

```text
camera_wire
camera_control
camera
gst_frame_bridge
qualcomm
qnn_engine
camera_graph_pump
source_supervisor
scheduler
multi_model_pump
multi_model_session
...
```

([GitHub][11])

Module boundary là tốt.

Nhưng build definition không cần phản chiếu architecture diagram ở độ granular 1:1.

Mình sẽ chuyển dần:

```text
src/core/CMakeLists.txt
src/adapters/camera/CMakeLists.txt
src/adapters/qualcomm/CMakeLists.txt
src/perception/CMakeLists.txt
src/runtime/CMakeLists.txt
src/app/CMakeLists.txt
tests/CMakeLists.txt
```

và chỉ giữ top-level làm:

```cmake
add_subdirectory(...)
```

Không ảnh hưởng architecture nhưng giảm friction rất nhiều.

---

# 16. Một sự thật quan trọng: hiện chưa có production executable

Executable:

```text
vqec_ai_vision_applications
```

chỉ được build với `vqec_vision_ai_reference`; CMake comment thẳng đây là **device-free reference harness**. ([GitHub][11])

Trong khi README cũng xác nhận:

* chưa live FW/model;
* chưa real decoder/tracker;
* chưa renderer;
* chưa hardware encoder;
* QNN engine chưa board-qualified. ([GitHub][1])

Đây là chỗ mình sẽ **dừng mở rộng architecture**.

Bạn đã có đủ contract rồi.

Từ giờ, value lớn nhất là:

```text
real frame
 → real model
 → real QCS6490 HTP
 → real detection
 → real tracker
 → real overlay
 → real H264
 → real FW ring
```

---

# 17. QIM SDK có thể giúp bạn không phải tự viết quá nhiều output/video plumbing

Qualcomm IM SDK hiện đã có sẵn plugin cho:

```text
mlvconverter
mlqnn
mlpostprocess
objtracker
overlay
qmmfsrc
rtspbin
smartvencbin
...
```

và sample Qualcomm dùng hardware/media pipeline cho multi-stream, composition, inference và encode/stream. ([GitHub][5])

Không có nghĩa là business perception/tracking của LACAI phải giao hết cho Qualcomm.

Mình sẽ phân chia:

```text
           Qualcomm/QIM                 LACAI
────────────────────────────────────────────────────
DMA-BUF bridge              │
preprocess                   │
QNN execution                │
HW composition/overlay*      │
HW encode*                   │
                             │ tensor
                             ▼
                      model decoder
                      object semantics
                      tracking policy
                      feature engine
                      rules
                      metadata
                      product behavior
```

`*` nếu QIM plugin đáp ứng output contract của FW.

Tức **đừng tự viết low-level hardware encoder adapter nếu QIM đã giải quyết đúng requirement**.

---

# 18. Thứ tự mình sẽ làm tiếp nếu mình lead repo này

Mình sẽ khóa feature expansion trong ngắn hạn và chạy đúng một roadmap sau:

1. **Làm vertical slice thật trên QCS6490:** một RAW NV12 FD từ FW → `qtimlvconverter → qtimlqnn` → một decoder thật như SCRFD/YOLO → tracker đơn giản → overlay → HW H264 → FW ring. Chưa cần 16 camera.

2. **Dùng plugin backend làm golden baseline.** Đo input-to-result latency, preprocess, inference, CPU, memory và FPS ở 1/2/4/8 camera. Owned-QNN chỉ được promote khi có số đo tốt hơn hoặc cung cấp capability plugin không có.

3. **Implement production `image_processor_port` cho owned QNN.** Sau đó làm persistent tensor pool → registered/shared memory → async QNN. Không `std::vector::resize()` output trên mỗi inference ở hot path.

4. **Sửa camera release semantics:** destructor enqueue release; dispatcher flush ACK có bounded retry/deadline. Sau đó test socket backpressure/disconnect/reconnect/stop trong khi còn lease.

5. **Thêm inference QoS + secondary inference scheduling.** Tách continuous models như detector khỏi event/ROI models như face embedding/OCR. Đây sẽ là nền cho các feature thực tế hơn “N model chạy song song trên full frame”.

6. **Thêm production telemetry trước khi tối ưu:** `frame_age`, `lease_count`, `frames_dropped_busy`, `frames_dropped_cadence`, preprocess latency, QNN latency, end-to-end latency P50/P95/P99, queue depth, source disconnects, HTP/CPU/thermal. Sau đó có thể kéo `libqcperf` của Qualcomm vào telemetry.

7. **Sau khi E2E ổn mới cleanup engineering debt:** simplify C++ method naming, split root CMake, thêm sanitizer/static-analysis/fuzzing cho wire decoder và lifecycle state machines.

8. **Acceptance cuối cùng:** 1/4/8/16 source soak test, camera disconnect/reconnect, model failure, QNN failure, FW restart, stop while hardware busy, thermal throttling và 24h+ soak. Chỉ sau đó mới gọi path là production/zero-copy. ([GitHub][1])

---

# Một architecture mình nghĩ sẽ đưa LACAI từ hiện tại lên production tốt hơn

```text
                         LACAI
┌────────────────────────────────────────────────────────────┐
│                                                            │
│ FW RAW NV12 DMA-BUF                                        │
│        │                                                   │
│        ▼                                                   │
│ frame_source                                               │
│        │                                                   │
│        ├── lease owner ────────────────┐                    │
│        │                               │                    │
│        ▼                               │                    │
│ source scheduler                       │                    │
│        │                               │                    │
│        ├──────────── Primary inference │                    │
│        │             detector          │                    │
│        │                 │             │                    │
│        │                 ▼             │                    │
│        │              tracker          │                    │
│        │                 │             │                    │
│        │                 ▼             │                    │
│        │          ROI/task scheduler   │                    │
│        │             │      │          │                    │
│        │             │      └──────── OCR                  │
│        │             └────────────── face embedding        │
│        │                                                   │
│        ▼                                                   │
│ feature / rule engine                                      │
│        │                                                   │
│        ├──────────── metadata/events                       │
│        │                                                   │
│        ▼                                                   │
│ overlay preparation                                        │
│        │                                                   │
│        ▼                                                   │
│ QIM composition / HW encoder                               │
│        │                                                   │
│        ▼                                                   │
│ H264 + metadata → FW ring                                  │
│                                                            │
│ release dispatcher ◄──────────────── final DMA owner ──────┘
│        │
│        ▼
│ FW buffer ACK
└────────────────────────────────────────────────────────────┘
```

Ở dưới scheduler:

```text
                   inference_graph_port
                           │
              ┌────────────┴─────────────┐
              │                          │
        QIM plugin graph            Owned QNN
        production baseline         optimized path
              │                          │
      qtimlvconverter              buffer pool
           qtimlqnn                memRegister
                                      async
                                      HTP
```

Architecture như vậy giữ nguyên phần tốt nhất bạn đang có, nhưng thêm đúng thứ còn thiếu cho camera AI product: **cascaded inference, real async execution và deterministic release path**.

---

## Một vài chi tiết nhỏ mình cũng sẽ sửa

Repo có vài chỗ dùng `QSC6490`/`qsc6490` trong naming/docs trong khi target là **QCS6490**. Không ảnh hưởng runtime nhưng nên cleanup sớm để grep/tooling/onboarding không bị nhiễu. README chính hiện đã viết đúng `QCS6490`. ([GitHub][1])

Mình cũng thích việc repo không cố phóng đại trạng thái: implementation status phân biệt rất rõ “QEMU test”, “board lifecycle smoke”, “hardware completion”, “zero-copy”, “performance acceptance”. Cách viết này nên giữ nguyên; nó ngăn team tự đánh lừa mình bằng test pass nhưng chưa có hardware evidence. ([GitHub][15])

### Kết luận quan trọng nhất

**Mình sẽ không rewrite LACAI.** Boundary hiện tại đáng giữ.

Nhưng mình cũng **không thêm thêm nhiều abstraction/contract nữa trong vài sprint tới**.

Hiện repo đã có đủ “xương”. Việc quan trọng nhất là buộc nó đi xuyên qua silicon:

```text
FW FD
  ↓
QCS6490 DMA-BUF
  ↓
preprocess
  ↓
HTP
  ↓
real model
  ↓
real tracking/feature
  ↓
HW encode
  ↓
FW ring
```

rồi đo.

Nếu vertical slice này chạy tốt, LACAI có khả năng trở thành một **application runtime rất sạch cho camera/AI box đa SoC**. Nếu tiếp tục mở rộng contract/model/registry trước vertical slice, repo có nguy cơ rơi vào trạng thái **architecture-complete nhưng product-incomplete** — và theo code hiện tại, đó là rủi ro lớn nhất mình thấy. ([GitHub][1])

[1]: https://github.com/anhtdh-vqec/LACAI "GitHub - anhtdh-vqec/LACAI · GitHub"
[2]: https://github.com/anhtdh-vqec/LACAI/blob/main/src/adapters/camera/vqec_vision_frame_source.cpp "LACAI/src/adapters/camera/vqec_vision_frame_source.cpp at main · anhtdh-vqec/LACAI · GitHub"
[3]: https://github.com/anhtdh-vqec/LACAI/blob/main/src/adapters/qualcomm/vqec_vision_dmabuf_bridge.cpp "LACAI/src/adapters/qualcomm/vqec_vision_dmabuf_bridge.cpp at main · anhtdh-vqec/LACAI · GitHub"
[4]: https://github.com/anhtdh-vqec/LACAI/blob/main/src/adapters/qualcomm/vqec_vision_plugin_graph.cpp "LACAI/src/adapters/qualcomm/vqec_vision_plugin_graph.cpp at main · anhtdh-vqec/LACAI · GitHub"
[5]: https://github.com/qualcomm/gst-plugins-imsdk?utm_source=chatgpt.com "GitHub - qualcomm/gst-plugins-imsdk: Qualcomm® IM SDK provides hardware-accelerated GStreamer plugins and reference apps for multimedia development, along with AI SDK integrations like Neural Processing SDK, AI Engine Direct, and Lite Runtime. · GitHub"
[6]: https://github.com/anhtdh-vqec/LACAI/blob/main/src/adapters/qualcomm/vqec_vision_qnn_engine.cpp "LACAI/src/adapters/qualcomm/vqec_vision_qnn_engine.cpp at main · anhtdh-vqec/LACAI · GitHub"
[7]: https://github.com/anhtdh-vqec/LACAI/blob/main/src/adapters/qualcomm/vqec_vision_qnn_inference_graph.cpp "LACAI/src/adapters/qualcomm/vqec_vision_qnn_inference_graph.cpp at main · anhtdh-vqec/LACAI · GitHub"
[8]: https://github.com/anhtdh-vqec/LACAI/blob/main/src/app/vqec_vision_multi_source_supervisor.cpp "LACAI/src/app/vqec_vision_multi_source_supervisor.cpp at main · anhtdh-vqec/LACAI · GitHub"
[9]: https://github.com/anhtdh-vqec/LACAI/blob/main/docs/adr/0003_owned_qnn_engine.md "LACAI/docs/adr/0003_owned_qnn_engine.md at main · anhtdh-vqec/LACAI · GitHub"
[10]: https://github.com/anhtdh-vqec/LACAI/blob/main/src/app/vqec_vision_multi_model_pump.cpp "LACAI/src/app/vqec_vision_multi_model_pump.cpp at main · anhtdh-vqec/LACAI · GitHub"
[11]: https://github.com/anhtdh-vqec/LACAI/blob/main/CMakeLists.txt "LACAI/CMakeLists.txt at main · anhtdh-vqec/LACAI · GitHub"
[12]: https://github.com/anhtdh-vqec/LACAI/blob/main/src/app/vqec_vision_multi_model_feature_pipeline.cpp "LACAI/src/app/vqec_vision_multi_model_feature_pipeline.cpp at main · anhtdh-vqec/LACAI · GitHub"
[13]: https://github.com/anhtdh-vqec/LACAI/blob/main/src/app/vqec_vision_multi_model_session.cpp "LACAI/src/app/vqec_vision_multi_model_session.cpp at main · anhtdh-vqec/LACAI · GitHub"
[14]: https://github.com/anhtdh-vqec/LACAI/blob/main/docs/development/code_convention.md "LACAI/docs/development/code_convention.md at main · anhtdh-vqec/LACAI · GitHub"
[15]: https://github.com/anhtdh-vqec/LACAI/blob/main/docs/development/implementation_status.md "LACAI/docs/development/implementation_status.md at main · anhtdh-vqec/LACAI · GitHub"
