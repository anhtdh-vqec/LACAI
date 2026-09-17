# Kế hoạch làm sạch base — 2026-09-16

Trạng thái 2026-09-17: **tất cả workstream hoàn thành** — CB-01, CB-02, CB-03, CB-B, CB-C,
CB-D, CB-E1/E2/E3, CB-F, CB-G/CB-T — mỗi bước có eSDK/QEMU + board `.98`. `service_main.cpp`
còn 2017 dòng và `run_generation` 1005 dòng; phần run loop, platform-owner và
recognition/feature setup còn lại gắn chặt vòng đời owner (phải sống qua bundle) nên cần
redesign "service runtime owner struct" nếu muốn tách tiếp, không phải dọn dẹp. Xem
`implementation_status.md` để biết bằng chứng từng bước.

Mục tiêu: dọn toàn bộ nợ cấu trúc/đúng đắn còn lại để có một **base sạch** trước khi
phát triển feature/tracker/production usecase. Phạm vi **không** gồm: 14 feature package,
tracker production, attribute producer, pose/OCR, attendance, liveness/PAD, TEE/hardware
key, signed provisioning, async/shared QNN — đây là các bước sau.

Kế hoạch này bổ sung [base_completion_plan](base_completion_plan.md) và
[architecture alignment review](../development/architecture_alignment_review.md); không
thay thế release gate. Mỗi bước là một commit focused có source + test + evidence. Build
chỉ bằng eSDK tại `/home/a/Workspace/eSDK`; test board native trên `.98`. Không push.

## Baseline đã chốt (B0)

Ngày 2026-09-16, toolchain eSDK `armv8-2a-qcom-linux`, cấu hình mở rộng
(Qualcomm/FastCV/QNN/Camera/Camera-DBUS/GST/FR-DBUS/usecase-DBUS/Zvec):

- eSDK cross-build: 100% target built.
- QEMU CTest: **120/120 passed** (`build-esdk-full`).
- Board `.98` native qua SSH key `lacai-qsc6490` (BatchMode, không dùng mật khẩu):
  **112/114** pass vô điều kiện.
  - `decoder_package_test`: pass khi cấp `manifests/models` (fixture target-side; ✅).
  - `zvec_embedding_index_test`: còn **2 check fail** cần triage; test hiện chỉ in tổng số
    lỗi nên phải bổ sung chẩn đoán theo từng check (CB-T, mục G).
- ABI ring thực tế (từ reader FW `vqec_vision_ring_rtsp.py`): **version 5, 16 slot,
  payload 1 MiB, header 4096, slot header 1232** — khớp `qtiv_renderer`, không khớp
  `ring_sink` (v4/2 MiB). Xem CB-01.

## Quy tắc thực thi mỗi bước

1. Viết test chứng minh lỗi/requirement trước khi sửa.
2. Đổi external boundary → cập nhật contract/ADR trước source.
3. Build eSDK + QEMU CTest; test native trên `.98`; production smoke khi chạm DMA/SDK/FW.
4. Rà literal/dependency; cập nhật `implementation_status` + docs; focused commit.
5. Ghi rõ test chưa chạy và lý do.

## Workstream A — Correctness integration (P0)

### CB-01: Chuẩn hoá một FW ring ABI (writer + reader contract)

Hiện trạng: hai implementation không đồng bộ. `src/adapters/qualcomm/vqec_vision_qtiv_renderer.cpp:99-201`
tự viết ring **v5/16/1 MiB/4096/1232** (khớp reader FW thật); `src/adapters/fw_output/vqec_vision_ring_sink.cpp:13`
assert `camera_ai::kRingVersion == 4` và `preview_limits::g_max_encoded_payload_bytes = 2 MiB`
(stale theo FW commit cũ 139d335). `fw_ring_sink.md:63` quy định "AI must not duplicate the
shared ABI" — nhưng adapter đang duplicate.

Hướng tối ưu đã chốt (áp cho FW):

1. **ABI canonical = `camera_ai::SharedMemoryFrameRingBuffer` version 5**, đúng như reader
   đang triển khai: `header_size=4096`, `slot_header_size=1232`, `slot_count=16`,
   `payload_size=1 MiB`; offset header `magic=0, version=4, header_size=8,
   slot_header_size=12, slot_count=16, payload_size=20, write_sequence=32, ring_id=64`;
   offset slot `seqlock=0, data_size=8, width=20, height=24, stride=28, is_keyframe=40,
   sps_size=44, pps_size=48, frame_id=56, timestamp_ns=64, media_pts_ns=72, sequence=104,
   codec=176, sps=208, pps=720`, parameter set tối đa 512 B.
2. **Một owner ABI**: FW phát hành header/version constant (`camera_ai_common`) trong eSDK
   sysroot; AI include header FW, không mirror inline. Khi chưa có, AI giữ **một** header
   layout duy nhất ở contracts với `static_assert(g_version == 5)` và contract test; xoá
   bản sao trong `qtiv_renderer`.
3. **Memory ordering**: writer dùng seqlock có release semantics
   (`std::atomic_thread_fence(std::memory_order_release)` trước/sau publish); FW reader
   phải acquire. Ghi rõ yêu cầu này trong contract.
4. **Open/create protocol**: không `unlink` vô điều kiện. AI `O_CREAT|O_EXCL`; nếu đã tồn
   tại thì attach + validate version/layout/identity; mismatch → fail-closed, không clobber.
   Thay thế chỉ khi FW reader đã dừng (coordinated).
5. **Path/sanitize** `/dev/shm/camera_ai_<ring_id>` là hằng contract; `ring_id` từ config.
6. **Trường bắt buộc điền**: `frame_id`, `timestamp_ns` (hiện chưa set), và bảo đảm keyframe
   chứa in-band SPS+PPS (reader quét payload).
7. Version 5 là canonical; sửa `fw_ring_sink.md`, `preview_limits` và `ring_sink` cho khớp
   hoặc deprecate/loại bỏ `ring_sink`.

Nguồn: `src/adapters/qualcomm/vqec_vision_qtiv_renderer.cpp`, `src/adapters/fw_output/`,
`docs/architecture/fw_ring_sink.md`, `docs/architecture/preview_contract.md`,
reader FW `/opt/anhtdh/tools/vqec_vision_ring_rtsp.py`.
Gate: live `.98` 30 FPS, reader replacement/restart không clobber, payload không vượt 1 MiB,
không còn hai định nghĩa ABI.

### CB-02: Vòng đời QNN

- `unload → load` hiện fail (`qnn_engine.cpp:398-400`, `qnn_inference_graph.cpp:216-229`):
  hoặc hỗ trợ reload đúng, hoặc cấm tường minh + test.
- `static_assert` layout ABI mirror `qnn_model_graph_config_info`/`qnn_model_graph_info`.
- Verify `tensor.version == QNN_TENSOR_VERSION_2` trước khi đọc `.v2` (`:507-508`).
- Làm rõ/bỏ claim zero-copy output (`:611-614` vẫn memcpy mỗi frame).

Gate: eSDK unit + board execute parity SCRFD/YOLOv8n byte-identical.

### CB-03: Clock domain cho metric

Map pipeline PTS → steady clock, hoặc bỏ `e2e_*` khỏi mọi claim latency và đánh dấu
`unsupported`. Nguồn: `docs/testing/qsc6490_board.md` (nhiều lần ghi metric vô nghĩa).

## Workstream B — Loại hardcode (P0/P1)

Đưa vào validated startup config (thiếu/sai → reject), xoá literal trùng:
`/opt/vqec/models/`, `/usr/lib/libQnnHtp.so`, `/usr/lib/libQnnSystem.so`, `/run/camera_ai`,
`nv12_format_value=23`, `"fw.dmabuf.v1"`/`"qcom.dmabuf.v1"` (literal lặp ở
`service_main.cpp:1215-1216`), revisions và step interval mặc định; default trong
`production_platform.hpp:41-50`.
Gate: production khởi động từ config; missing/invalid policy reject; board flow không đổi.

## Workstream C — Gộp validation dùng chung (P1)

- SHA-256 hex: `model_catalog.cpp:21-24`, `model_package.cpp:8-13`, `inference_execution.cpp:253-255`.
- Identifier: `model_package.cpp:15-29` tự viết, đã lệch `identifier.hpp`.
- Even-dimension (~10 nơi) và `packed_nv12` (~4 nơi).
Gate: mỗi validator một owner; unit test missing/invalid/boundary.

## Workstream D — Hạ tầng chết (P1) — đã xử lý (CB-D)

`inference_worker`, `secondary_inference_scheduler`, `recovery_controller` build nhưng
không được wire; tồn tại 2 thiết kế secondary và 3 implementation bounded-concurrency.
Quyết định (ADR 0006): xoá `secondary_inference_scheduler` + contract `secondary_inference`
đã bị `cascade_coordinator` thay thế; giữ `inference_worker` và `recovery_controller` như
reserved có điểm wire tường minh. Gate: còn đúng một đường thực thi mỗi concern.

## Workstream E — Tách monolith (P1)

`service_main.cpp::run_generation` (đã tách còn ~1005 dòng) tách tiếp thành builder nhỏ;
PIMPL đầy đủ cho production platform. Không đổi behavior.
Gate: 120/120 giữ nguyên; board production flow không đổi.

## Workstream F — Lỗi nhỏ/security (P1/P2)

- `contracts/vqec_vision_recognition.hpp:8` include header ports (đảo layering).
- Zvec helpers `noexcept` + `std::string` → OOM `std::terminate`.
- Storage: AAD chưa bind identity; thêm `src/adapters/storage/README.md`.
- `model_io_manifest.cpp:54` tham số `_label` bị bỏ.
- Bound kích thước reply D-Bus (`usecase_control_dbus.cpp:143-155`).
- Check `gst_bin_add_many` (`face_enrollment_image_source.cpp:295-301`).

Gate: unit test tương ứng.

## Workstream G — Test/CI/docs trung thực (P1/P2)

- **CB-T**: thêm chẩn đoán theo từng check cho test Zvec và triage 2 fail trên `.98`.
- CI: `host-sanitizers` tắt Zvec hoặc bootstrap SDK (hiện clean runner fail ở
  `CMakeLists.txt:82`); eSDK jobs phải **fail** khi được yêu cầu mà thiếu `ESDK_ROOT`
  thay vì skip im lặng; thêm job board smoke tùy chọn.
- Scaffold chạy được cho `tests/{integration,replay,board,golden}`.
- Đồng bộ docs: `tests/unit/README` (53/71 cũ vs thực 121), `implementation_status`, README.

## Definition of done cho base sạch

1. CB-01..CB-03 và workstream A–F xong, mỗi bước có test + evidence eSDK/QEMU + board `.98`.
2. Không còn hai định nghĩa ABI ring; không còn hardcode policy trong production executable.
3. Không còn target/hạ tầng chết không rõ trạng thái; mỗi validator một owner.
4. CI phản ánh đúng: job thiếu điều kiện fail chứ không skip thành "green".
5. Docs khớp source; baseline và gate được ghi lại trong `implementation_status`.

## Việc để bước sau (không thuộc base sạch)

Feature package thật, tracker production, attribute producer, pose/OCR, attendance,
liveness/PAD, TEE/hardware key, signed provisioning, async/shared QNN, multi-vendor.
