# Yêu cầu phối hợp FW – AI APP: frame transport, an toàn và tối ưu

Ngày: 2026-09-06. Người nhận: team FW BSP và FW software.
Người đề xuất: team AI APP. Trạng thái: đề xuất để review/sign-off, chưa phải cam kết của FW.

## 1. Quyết định tích hợp hiện tại

Companion baseline: [FW release compatibility](fw_release_compatibility.md) covers
AI-owned overlay/H264/ring, legacy AI control and backend/UI integration. Its final
section adds ring SDK, stale-consumer and deployment review requests. These are not
requirements to change the current raw transport before AI source development.

AI APP hiện nhận đúng nhánh `third` NV12/FD; contract đích gọi chung là FW RAW source.
AI Box phải normalize RTSP thành cùng RAW contract trước ranh giới AI APP.
Không yêu cầu FW đổi protocol hay bổ sung 4K trước khi AI viết receiver và adapter.
Không mở camera sensor/ISP trực tiếp, không lấy encoded main/sub để decode lại.
Một runtime AI nhận một kết nối mỗi logical RAW source, chia sẻ frame/compute giữa các feature.

Tách hai mốc: có thể phát triển/tích hợp happy path với giao thức hiện tại;
chỉ nghiệm thu production sau khi các điều kiện an toàn P0 bên dưới được xác nhận.
Không dùng “tương thích hiện tại” để miễn kiểm chứng ownership hoặc hardware completion.

Tài liệu này tự đủ để gửi cho FW. Các đường dẫn source bên dưới tính từ repository
`vqec_camera_service`, snapshot commit `139d335913e19e5a33a36fa8f8d706009892db44`.
Kết luận dựa trên đọc source, chưa phải kết quả đo trên board.
Target theo thông tin dự án: QSC6490 / Qualcomm Linux 1.8.

## 2. Baseline FW mà AI sẽ tuân theo

### 2.1 Data plane

```text
qtiqmmfsrc -> master NV12 -> tee
  -> queue (leaky downstream, max 2 buffers)
  -> videorate (drop-only)
  -> qtivtransform + caps theo third_profile
  -> appsink -> Unix SOCK_SEQPACKET + SCM_RIGHTS -> AI
```

- Đây là post-ISP NV12 chưa encode, không phải sensor RAW/Bayer.
- Producer chỉ chấp nhận một GstMemory, FD-backed, có GstVideoMeta.
  Kiểm tra fd-backed trong code chưa tự chứng minh mọi FD đều import được vào AI hardware.
- Một packet chứa packed FrameHeader 104 byte và một FD; ACK là buf_id 8 byte.
- Wire chưa có magic/version/endianness declaration; format là enum GstVideoFormat.
  AI sẽ cô lập compatibility này trong camera adapter và pin ABI của bản FW tích hợp.
- Socket camera 0: `/tmp/camera_ai/0_third_ai.sock`.
- Socket camera N khác 0: `/tmp/camera_ai/0_third_ai_camN.sock`.
- channel thực tế của producer là 0; stream route `third`, socket consumer route `ai`.
- Không dùng default `/run/camera_ai` của cấu hình transport chung cho producer này.
- Server fan-out tối đa 8 client; mỗi client giữ tối đa 4 buffer chưa ACK.
  Đầy 4 buffer thì bỏ frame mới; appsink max-buffers=1, drop=true.
- Gửi cho từng client tuần tự; send timeout hiện 2 giây/client.
  Đây là giới hạn trong code, không phải SLA latency đã nghiệm thu.

| Metadata hiện có | Cách AI sử dụng |
|---|---|
| buf_id | Token ACK trong đúng session, không coi là frame ID toàn cục |
| width, height, format, n_planes | Validate trước import; không hardcode 4K |
| offset[4], stride[4] | Giữ layout gốc, không giả định stride bằng width |
| size, mem_offset, mem_maxsize | Validate view và allocation bằng checked arithmetic |
| pts_ns, dts_ns, duration_ns | Giữ giá trị hợp lệ; xử lý riêng GST_CLOCK_TIME_NONE |

Nếu CPU map để diagnostic: plane bắt đầu tại mapped_base + mem_offset + offset[i].
Không fallback sang packed NV12 khi offset/stride không hợp lệ.
Không dùng số FD làm identity của allocation; FD có thể được tái sử dụng.
Chưa có color matrix/range, modifier, fence, source epoch hoặc UTC mapping trên wire.

### 2.2 Profile hiện có

| Camera | Default | Resolution / max FPS được code cho phép |
|---|---|---|
| 0 | 1920x1080 @25 | 1280x720 @25; 1920x1080 @25; 2560x1440 @20 |
| 1 | 854x480 @25 | 640x360 @25; 854x480 @25; 1280x720 @25 |

FPS chọn trong 10/15/20/25 và không vượt trần theo resolution.
Default không thay thế effective profile đang chạy; AI kiểm tra response và từng frame.
Raw 3840x2160 chưa được API third cho phép, dù source/master có thể lớn hơn.
AI không tự nâng third profile: đây là nhánh dùng chung, thay đổi ảnh hưởng consumer khác.

### 2.3 Control plane và lifecycle

D-Bus bus name `com.vnpt.camera.Camera`, object `/com/vnpt/camera/Camera`,
interface `com.vnpt.camera.Camera1`; implementation mặc định system bus,
có cấu hình session bus cho môi trường phù hợp. RpcClient port 9101 là khóa ánh xạ endpoint,
không phải yêu cầu AI kết nối TCP 9101.

1. StartStream(camera_id, channel_id=0, stream_id=third, transport=dmabuf,
   consumer_id riêng và ổn định cho acquisition hiện tại).
2. Giữ stream_handle; đọc effective profile, kết nối socket có bounded retry/backoff.
3. Nhận/validate frame; chỉ ACK frame đã bỏ trước submit hoặc đã hết mọi reader.
4. Khi dừng bình thường: chặn submit, drain readers, ACK, đóng socket rồi StopStream
   bằng đúng stream_handle và consumer_id đã acquire.
5. Khi reconnect: tăng session epoch nội bộ, reset temporal state phù hợp;
   không ACK token phiên cũ qua socket phiên mới. Source restart phải reacquire lease.

Lease consumer_id không phải chuỗi route `ai` trong tên socket, cũng không phải auth token.
StartStream có fast path idempotent theo camera/channel/stream/consumer hiện có;
không đổi transport âm thầm dưới cùng identity rồi coi đó là acquisition mới.
StartStream thành công không chứng minh socket/first frame đã sẵn sàng.
Không dùng third_ring_id hoặc trạng thái debug ring làm bằng chứng raw socket đang healthy.

## 3. Yêu cầu P0: điều kiện an toàn trước production

P0 có thể giải quyết bằng bằng chứng/contract BSP sẵn có hoặc thay đổi FW nếu thiếu;
không mặc định mọi mục đều cần thêm API mới.

### FW-AI-01 — Không recycle buffer khi hardware còn đọc

Owner: FW BSP + FW software; AI APP phối hợp fault test.

Hiện destructor client session unref toàn bộ pinned buffer dù chưa ACK.
Prune client/disconnect, stop producer hoặc đổi profile có thể đi vào đường này.
FD còn mở ở AI chỉ giữ allocation, không bảo đảm pool không ghi đè nội dung.

Yêu cầu:

- Bình thường chỉ trả pool sau ACK đại diện cho reader cuối hoàn tất.
- Với disconnect/crash/timeout: chứng minh hardware đã quiesce trước recycle,
  hoặc quarantine allocation/pool cũ với giới hạn bộ nhớ và escalation rõ ràng.
- Không timeout rồi tự xem như completion; không quarantine vô hạn không có recovery.
- Quy định bên nào thực hiện reset/quiesce, phạm vi reset ảnh hưởng camera/encoder/NPU,
  điều kiện được giải phóng và trạng thái báo supervisor.
- Profile change/stop cũng phải tuân cùng invariant, không chỉ consumer crash.

Nghiệm thu: fault injection ở lúc hardware đang đọc; trace chứng minh lần ghi/reuse kế tiếp
chỉ xảy ra sau completion/quiescence. Không chấp nhận chỉ “chạy không thấy crash”.

### FW-AI-02 — Xác nhận layout, màu và memory synchronization

Owner: FW BSP; AI APP kiểm chứng import/preprocess.

- Chốt FD type/allocator thực tế, linear NV12 hay modifier khác, plane/stride/alignment.
- Chốt NV12 color matrix/range/chroma siting. Nếu wire chưa truyền, cung cấp profile
  cố định có version; không đổi âm thầm giữa firmware release.
- Chốt khi send frame thì producer write đã hoàn tất bằng cơ chế nào; implicit sync
  nếu có phải chỉ rõ producer/consumer API nào thực hiện wait/import và bằng chứng test.
- Chốt cơ chế CPU cache synchronization khi debug map; không nhầm cache sync với device fence.
- Nêu cách xác nhận last device read completion trước ACK.

Nghiệm thu: padded-stride/nonzero-offset fixtures, color bars và golden tensor;
không lệch plane/màu, không đọc vượt view; stress producer-write/AI-read không torn frame.

### FW-AI-03 — Bảo vệ raw socket và làm chặt parser

Owner: FW software; BSP hỗ trợ policy trên image.

- Socket directory/file có owner/group/mode được quản lý, không mở raw frame cho user tùy ý.
  Kiểm tra quyền peer (ví dụ SO_PEERCRED) và liên kết quyền stream với client theo thiết kế FW.
- consumer_id tự khai báo không được coi là chứng thực.
- recvmsg kiểm tra MSG_TRUNC/MSG_CTRUNC, payload length, ancillary type/length và đúng số FD;
  đóng mọi FD thừa/lỗi, dùng CLOEXEC, validate dimensions/planes/ranges có overflow checks.
- ACK parsing kiểm tra packet length đầy đủ; duplicate/stale/unknown ACK không giải phóng sai slot.
- Không unlink/bind socket trong directory không đáng tin mà thiếu kiểm soát ownership.

AI sẽ làm chặt parser phía mình; FW chịu trách nhiệm endpoint và ACK receiver phía producer.
Nghiệm thu: unauthorized peer bị từ chối; malformed/truncated/extra-FD packets không leak FD,
không crash và không release nhầm buffer.

## 4. Yêu cầu P1: ổn định và tối ưu bản tích hợp

| ID | Owner chính | Yêu cầu / đầu ra nghiệm thu |
|---|---|---|
| FW-AI-04 | FW software | Cô lập client chậm: bounded/nonblocking send hoặc worker bounded; một client không giữ đường gửi client khác tới timeout 2 giây. Đo tác động khi encode/record cùng chạy. |
| FW-AI-05 | FW software + BSP | Công bố budget pool tổng và per-client, max hold time, drop policy. Không chỉ tăng max_inflight để che bottleneck; báo pinned high-water/drop reason. |
| FW-AI-06 | FW software | GetStatus phản ánh riêng raw branch, socket readiness, connected clients, effective profile, producer instance và lỗi; phân biệt với third debug ring. |
| FW-AI-07 | FW software + AI APP | Reconfigure có drain boundary và thông báo discontinuity; tránh nhiều client tự đổi profile nhau. Chốt owner/arbitration và recovery khi Start/Stop response mất. |
| FW-AI-08 | FW software | Lease cleanup khi app chết: policy có owner, thời hạn và reconcile; cleanup control lease không được dẫn đến recycle DMA thiếu quiescence. |
| FW-AI-09 | FW BSP | Bàn giao bảng tương thích image/GStreamer/plugins/allocator/SDK; package runtime dependencies và quyền device để AI IPK độc lập. Không buộc AI link source từ repo FW. |
| FW-AI-10 | FW software + AI APP | Counters và timing có clock domain: sent/acked/dropped, pinned, send latency, hold time, reconnect, pool/FD usage; bounded logging không chứa ảnh/biometrics. |

Ưu tiên tối ưu data path dựa trên profiling: giữ FD + metadata, tránh full-frame CPU copy;
không bắt buộc FastCV cho mọi thao tác FW nếu đường qtivtransform hiện tại đáp ứng correctness/KPI.
AI có thể ACK sau preprocess khi đã chứng minh output độc lập và không còn reader input;
không cần giữ camera frame suốt inference/temporal window nếu không có dependency.

## 5. Yêu cầu P2: mở rộng có version, không chặn baseline

### FW-AI-11 — Raw 4K và traffic

Owner: FW BSP + FW software; AI APP cung cấp workload.

- Đánh giá bổ sung third NV12 3840x2160; 25 FPS là target đề xuất, chưa là cam kết.
- Xác nhận sensor mode, ISP/transform/DDR/pool budget cùng main/sub encode, record,
  số camera và thermal steady state. Không chỉ thêm resolution vào validation table.
- Đánh giá bypass scale khi geometry trùng master, chỉ nếu memory lifetime/isolation và
  starvation của camera pool được chứng minh; không mặc định export master tốt hơn.
- Traffic cần thêm khả năng FPS/shutter/timestamp/orientation/calibration phù hợp model;
  chốt theo từng workload, không coi 4K là đủ cho mọi bài traffic.

NV12 4K packed = 12,441,600 byte/frame, khoảng 311 MB/s tại 25 FPS cho payload một chiều;
chưa gồm padding và lượt đọc/ghi/copy. Đây là phép tính dung lượng, không phải benchmark.

### FW-AI-12 — Protocol kế tiếp

Owner: FW software; BSP và AI APP review schema.

Đề xuất thêm magic/version/header_size/message_type, endian rõ ràng, producer/session/source
epoch, profile revision, portable pixel format/modifier/color information, allocation generation,
clock domain và synchronization description; sync handle chỉ có khi cơ chế yêu cầu.
Capability API trả effective profile, limits và transport endpoint.

Giữ legacy endpoint/protocol trong thời gian migration hoặc thêm endpoint version riêng.
Không chèn trường vào packed header 104 byte trên endpoint cũ làm vỡ AI đang triển khai.
Version mới phải có serializer/decoder fixtures, compatibility matrix và rollback test.

### FW-AI-13 — Unified multi-source RAW registry

Owner: FW software + BSP; AI APP cung cấp workload/admission fields.

- Công bố 1..16 logical source IDs, effective profile/rate của từng source và giới hạn
  tổ hợp đồng thời; `16` là ceiling cấu hình, không phải mặc định hay cam kết SoC.
- Cung cấp `raw_source_ref` ổn định và cùng RAW descriptor/lease/FD/ACK/epoch semantics
  cho cả AI Camera và AI Box; một feature mới không tạo thêm producer nếu dùng chung source.
- Trên AI Box, FW tự resolve RTSP, quản lý credential, demux/decode và chỉ export RAW.
  Không đưa RTSP URI/codec/password hoặc decoder lifecycle qua AI APP contract.
- Công bố RAW profile/format/modifier, reconnect/discontinuity, timestamp/clock,
  allocation/synchronization và ngân sách toàn hệ thống khi chạy cùng record/live stream.

### FW-AI-14 — Preview output registry cho nhiều source

Owner: FW software; backend/UI/RTSP service cùng review.

Release hiện chỉ có `detect0`/`detect1` cố định cho cam0/ch0. Để expose preview độc lập
cho nhiều source, FW cần contract registry có version, unique writer ownership, mapping
source -> ring/mount/UI, lifecycle/generation, first-viewer keyframe, stale-reader cleanup
và backward compatibility. AI APP sẽ reject `preview_output_ref` không resolve được;
source inference-only đặt preview surface count bằng 0.

### FW-AI-15 — Memory/synchronization evidence per source

Owner: FW BSP + FW software.

Capability/admission response cần cho biết allocator/modifier, allocation size có stride,
pool bytes/high-water, acquire/completion synchronization và quarantine/reset behavior.
Nghiệm thu phải trace từng boundary để phân biệt import cùng allocation với copy ngầm;
không gọi toàn pipeline là zero-copy chỉ vì IPC gửi FD.

## 6. Phần AI APP tự chịu trách nhiệm ngay

- Camera adapter legacy-wire riêng; không expose Gst/vendor type trong neutral core.
- Validate descriptor, đúng FD ownership, original strides/offsets, session-bound ACK.
- Queue/inflight bounded trong giới hạn 4 buffer của FW; admission cho shared models/ROI.
- Frame chưa submit có thể drop + ACK; frame đã submit giữ lease đến last-reader completion.
- Điều khiển Start/Stop và xử lý reconnect; không tự sửa profile toàn nhánh dùng chung.
- Không đoán color/fence guarantee bị thiếu; dùng cấu hình tích hợp đã được BSP xác nhận.
- Không nhận UBWC/multi-FD hay protocol mới bằng heuristics khi chưa được hỗ trợ rõ ràng.
- Thu metrics receive/preprocess/inference/hold/release và xây mock/fault/board tests.
- Không dùng một socket riêng cho mỗi bài trong 13 feature; xử lý entitlement và chia sẻ compute
  trong runtime AI. FR/attributes/traffic không thay đổi ownership contract của frame.

## 7. Ma trận nghiệm thu và bàn giao

| Nhóm test | Tình huống | Tiêu chí |
|---|---|---|
| Correctness | Default profile, padding, nonzero mem_offset, timestamps invalid | Không đọc sai/vượt view; tensor so golden trong tolerance model |
| Backpressure | AI chậm, queue đầy, client không đọc/không ACK | Memory/FD/pinned bounded; drop có lý do; đo ảnh hưởng camera/record |
| Lifecycle | Start/Stop lặp, app crash, socket mất, FW restart | Không stale ACK; không recycle trước completion; phục hồi theo policy |
| Reconfigure | Đổi resolution/FPS lúc AI đang đọc | Drain/recovery rõ ràng; không trộn layout hoặc temporal epoch |
| Security | Sai peer, packet/ancillary lỗi, duplicate ACK | Reject có lý do; không crash/leak/release sai |
| Coexistence | AI + main/sub + record, thermal soak | Đạt KPI được các team ký theo workload |

Đề xuất soak 24 giờ cho release candidate; thời lượng và workload chính thức cần thống nhất.
Report phải có image/commit, plugin/model version, profile, camera count, encode/record settings,
CPU/RSS/FD/pool/thermal, latency p50/p95/p99, drops, và fault recovery traces.
KPI numeric chưa được đo không được ghi thành “đạt”: cần chốt FPS tối thiểu, p99 frame age,
max hold time, memory cap, recovery deadline và mức ảnh hưởng recording cho từng workload.

### Đề nghị FW phản hồi

Với mỗi FW-AI-01..15, trả: người phụ trách, đã hỗ trợ/cần sửa/không hỗ trợ,
bằng chứng source hoặc test, phương án, target release/ngày bàn giao và hạn chế còn lại.

Thứ tự phối hợp đề xuất:

1. Review P0 và xác nhận baseline/profile/memory facts trước buổi tích hợp hardware đầu tiên.
2. AI triển khai legacy receiver song song; FW bổ sung test/bằng chứng hoặc sửa P0.
3. Đo baseline cùng workload thực, chọn thay đổi P1 theo bottleneck.
4. Nghiệm thu P0 + KPI release; lên lịch riêng cho 4K/protocol mới, không đổi legacy âm thầm.

## 8. Source dùng đối chiếu

- `shared/raw_frame_transport/src/raw_frame_wire_protocol.hpp`: header và ACK.
- `shared/raw_frame_transport/src/custom_dmabuf_raw_frame_transport.cpp`:
  BuildSocketPath, SendSample, ReturnAckLoop, destructor, AcceptLoop, ReceiveFrame/ReleaseFrame.
- `shared/raw_frame_transport/include/camera_ai/raw_frame_transport/raw_frame_transport.hpp`:
  metadata và public producer/consumer interface.
- `hal/capture/src/gstreamer_camera_adapter.cpp`: AppendRateThenTransform,
  TryAttachThirdAiBranch và DetachThirdAiBranch.
- `hal/transform/src/transform_adapter.cpp`: hardware transform và output caps.
- `shared/common/include/camera_ai/common/const.hpp`: socket path và third capabilities.
- `platform/camera_service/src/camera_service.cpp`: Start/Stop, profile validation/reconcile.
- `shared/common/src/rpc.cpp`, `rpc_endpoint_catalog.cpp` và
  `platform/ipc/dbus/com.vnpt.camera.Camera1.xml`: control transport/schema.
- `application/ai_app/src/video/DmabufFrameSource.cpp`: consumer hiện có để đối chiếu,
  không phải implementation AI mới và không mang OpenCV path sang AI mới.
