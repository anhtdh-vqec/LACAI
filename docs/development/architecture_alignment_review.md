# Architecture alignment review và danh sách vấn đề còn lại

2026-09-16 follow-up: same-process desired-plan drain/rebuild/publication and private
volatile Zvec lifecycle are now delivered and compatibility-tested on `.98`. The initial
persistent plaintext index was removed after encrypted-gallery recovery passed. Current
measured cases, resource sample and remaining release gates are consolidated in
[FR validation](../testing/face_recognition_production_validation.md). Historical findings
below retain their review context; signed provisioning, durable receipts, asynchronous
control, hardware-key/swap policy and workload qualification remain open.

Ngày: 2026-09-15. Cập nhật theo source cascade/runtime hiện tại.
Phạm vi: Markdown trong docs, root rules/README và module READMEs, đối chiếu source/CMake
ở các boundary liên quan. Đây là architecture/documentation review, không phải audit mọi
nhánh code hay một lần benchmark/board qualification mới.

**Status:** current — open architecture issue backlog A01–A25.

## 1. Kết luận về định hướng

**Chưa lệch mục tiêu cốt lõi, nhưng có khoảng cách đáng kể giữa kiến trúc mong muốn,
production composition và các tài liệu được cập nhật không đồng bộ.**

Giữ nguyên: AI nhận RAW từ FW, một acquisition mỗi source, multi-model qua neutral ports,
AI sở hữu usecase và preview output, Qualcomm tối ưu ở adapters, portable qua vendor ports,
không OpenCV production và không viết lại kernel vendor chỉ để tự sở hữu implementation.

Hai thay đổi có chủ đích cần phân biệt với sai lệch:

| Quyết định | Ban đầu | Hiện hành | Đánh giá |
|---|---|---|---|
| Qualcomm | Plugin-first ADR 0002 | Thêm owned QNN adapter ADR 0003; production dùng FastCV/QNN | Hợp hướng nếu có capability/evidence; generic selection chưa xong |
| FR gallery/search | Đề xuất FW service | AI sở hữu matching, encrypted gallery/key và Zvec theo ADR 0004 | Đúng scope mới; hardware key/power-cut/performance còn mở |
| Push | Tự commit + push | Commit theo bước, người dùng tự push | Rule đã đồng bộ theo chỉ đạo mới nhất |

Portable không có nghĩa mọi model/vendor đã hỗ trợ. Default build có Zvec không đồng
nghĩa mọi luồng dùng FR; Zvec là dependency FR, các neutral targets vẫn cần độc lập.
13 feature entries và 16 source ceiling không phải bằng chứng 13 feature/16 camera chạy thật.

## 2. Những mâu thuẫn tài liệu đã sửa trong lần này

- System architecture còn nói service, tracker, renderer và QNN direct chưa có.
- README báo base-ready, số test cũ và default all-options-OFF dù Zvec mặc định ON.
- ADR 0002 nói direct SDK chưa implement; ADR 0004 giữ đoạn thiếu lib đã được giải quyết.
- FW control/system plan còn giao matching/search cho FW trái quyết định FR mới.
- Cascade design nói chưa có package binding/decoder dù đã có; tách primitive và integration.
- Rules còn yêu cầu push tự động, convention/delivery plan còn nhắc host build.
- Capability matrix còn báo FastCV/render chưa có và gọi smoke là qualification toàn phần.
- Tài liệu lịch sử và tài liệu hiện hành chưa có bản đồ, thứ tự authority và thuật ngữ evidence.

Các báo cáo test cũ giữ nguyên ngày/phạm vi, không đổi số cũ thành 97. Tài liệu research
mô tả upstream API không tự là capability của adapter LACAI. Contract proposal không tự
là FW implementation đã released. Index tài liệu giúp phân biệt các loại này.

## 3. Backlog kiến trúc/code còn mở

P0: correctness/ownership hoặc chặn usecase; P1: production generality/performance;
P2: maturity/tooling. Source references chỉ ra nơi cần review, không khẳng định mọi case
đã tái hiện bằng test. Owner là vai trò đề xuất; chưa gán cá nhân hoặc ngày nghiệm thu.

| ID | Mức | Vấn đề / evidence | Việc phải làm | Tiêu chí đóng / owner |
|---|---|---|---|---|
| A01 | P0 | FD→exact-frame alignment→EdgeFace đã nối ở source nhưng chưa chạy live/golden end-to-end | Chạy camera với approved golden capture; đối chiếu crop/input/embedding và correlation | Camera → embedding đúng frame/epoch, drain và parity; AI runtime/BSP/model |
| A02 | P1 | FastCV aligner còn map/copy/allocate ROI và tensor cho từng mặt; QTI color khác neutral golden chưa được quyết định | Golden hóa color/border; pool destination/input/output; profile và chọn offload theo evidence | Input/crop/tensor chuẩn, real completion và copy/CPU budget; AI/BSP/model |
| A03 | P0 | Cascade execute đồng bộ trên service progress thread; epoch đổi yêu cầu restart graph | Bounded worker/completion state machine, fair admission, stale-result cleanup và graph epoch reconciliation | Multi-face không block camera/output; stop/restart không ACK sớm; runtime |
| A04 | P0 | **Đã xử lý**: protected encrypted gallery + revision CAS + reopen/rebuild; Zvec derived index private tmpfs, destroyed on close | Còn lại: hardware-bound key, capacity/load benchmark | Restart/delete/replay đúng revision; AI/FW |
| A05 | P0 | **Đã xử lý (loader)**: strict decoder_package loader rejects unknown/duplicate/out-of-range và cross-check catalog | Còn lại: golden thật của model | Reject invalid trước activation; model/app |
| A06 | P0 | DMA-BUF retention/reference count không chứng minh hardware completion | Trace input/crop/tensor owners, fence/cache/import and drain protocol | No early ACK/reuse trong native fault tests; BSP/adapter |
| A07 | P0 | Compatibility camera/RTSP != released FW acceptance | Validate released wire/ring/control, disconnect, demand, ACK, permissions | FW end-to-end conformance report; FW/AI |
| A08 | P0 | **Đã xử lý (source+board)**: protected gallery, multi-template enrollment/search, matching, clean-restart recovery | Còn lại: calibration, liveness/PAD, attendance output | Delete/restart đúng; AI/FW/model |
| A09 | P1 | QNN build qua `qnn_backend_bundle` factory; FastCV vẫn constructed trực tiếp | Generic capability/policy selection cho mọi backend | Thay backend không sửa orchestration; platform |
| A10 | P1 | Production còn dùng reference tracker/zone factory | Tách fixture/production registration; qualified tracker/processor contracts | Không nhận fixture success làm feature acceptance; app/features |
| A11 | P1 | **Đã xử lý**: production config bỏ default, CLI bắt buộc + fail-closed (CB-B); YOLO defaults đã bỏ | Còn lại: version compatibility schema | Missing policy reject; app |
| A12 | P1 | Renderer/feature geometry vẫn dùng sources_.front(), decoder chia sẻ hạn chế | Per-source owners/output routes; chốt supported concurrency | Multi-source khác profile chạy hoặc reject trước acquisition; app |
| A13 | P1 | Generic native capability factory chưa được production dùng toàn bộ | Reconcile capability/entitlement/admission và effective state từ execution thực | Không advertise async/shared/dynamic unsupported; runtime |
| A14 | P1 | Anchor decoder vẫn tạo observation strings/vectors mỗi batch | Pool/reuse output, stable numeric IDs, atomic delivery không mất owner | Allocation counters bounded/measured; perception |
| A15 | P1 | QNN output tensor allocation/client staging, compatibility render copy | Profile từng copy; reusable registered memory khi BSP hỗ trợ | Numeric parity + completion + measured CPU/copies; adapter |
| A16 | P1 | Zvec mutex bao vendor calls, allocate query/doc, FLAT fixed | Bounded search worker, explicit queue/deadline, measured index selection | Không block camera, latency/recall budget; FR/index |
| A17 | P1 | 30 FPS person chưa đủ target CPU; FR chưa có workload budget | Đo stage p50/p95/p99, faces/gallery sweep, thermal, FR jobs/s | 25–30 FPS và CPU 15–25% theo workload/CPU convention đã chốt; perf |
| A18 | P1 | Model-specific capacity/threshold còn chưa calibration | Admit ROI rates theo measurements, version model/preprocess/policy | Không giảm refresh 1s để che backlog; model/runtime |
| A19 | P0 | Model artifact digest không xác thực signer/TOCTOU | Review resolver/load authority, signed bundle/revision trust boundary | Load không đổi artifact sau kiểm tra; platform/FW |
| A20 | P1 | Async/shared-QNN/update mới contract hoặc unsupported | Chỉ triển khai khi usecase cần; probe capability thực, no silent fallback | Native correctness + throughput/lifetime evidence; adapter |
| A21 | P1 | Nhận diện không có liveness từ SCRFD+EdgeFace | Chốt anti-spoof requirement, PAD hoặc cơ chế được duyệt và budget riêng | Accuracy + spoof acceptance nếu sản phẩm yêu cầu; product/model |
| A22 | P1 | Feature/gallery output authorization còn phải nối tới identity payload | Revoke theo revision, cache invalidation, delete pending matches | Không publish identity đã revoke/delete; FW/features |
| A23 | P2 | Bootstrap public ARM64 SDK chưa là deployment package portable | Target/ABI validation, redistribution notices, offline packaging và upgrade | Reproducible install/rollback theo target; build/release |
| A24 | P2 | **Đã chuẩn hóa** trong đợt clean-base docs: bỏ record lỗi thời, sửa contract/architecture/readme lệch code | Duy trì docs map và link check mỗi PR | Không có hai status hiện hành trái nhau; mọi module owner |
| A25 | P2 | Structural checker không kiểm literal semantics/ownership/ABI | AST/literal lint có allowlist; CI eSDK/native evidence | Không dùng grep pass để tuyên bố sạch hardcode; tooling |

### Source anchors

- A01–A03: `src/runtime/scheduler/vqec_vision_cascade_frame_store.hpp`,
  `include/vqec/vision/ai/ports/vqec_vision_image_alignment.hpp`,
  `src/app/vqec_vision_cascade_coordinator.cpp`, `vqec_vision_cascade_graph_session.cpp`,
  `vqec_vision_runtime_executor.cpp` and `vqec_vision_multi_model_session.cpp`.
- A04/A16: `src/adapters/zvec/vqec_vision_zvec_embedding_index.cpp` (fresh collection,
  in-memory revision, fault gate, query allocation and mutex).
- A05/A09–A12: `src/app/vqec_vision_production_platform.cpp/.hpp` (decoder selection,
  catalog/source geometry, constructors, reference factories and output composition).
- A14: `src/perception/detection/vqec_vision_anchor_distance_decoder.cpp` (output batch).
- A15/A20: `src/adapters/qualcomm/vqec_vision_qnn_engine.cpp` and
  [preprocessing report](../architecture/qualcomm_preprocessing.md).
- A19: `src/runtime/model_registry/` and production package loading require end-to-end review;
  existence of a secure resolver helper alone does not prove every load path uses it.

## 4. Thứ tự xử lý

1. Chạy live/golden FD-to-embedding và sửa mọi mismatch contract/correlation trước khi
   publish identity.
2. Tách cascade khỏi service thread, pool/cắt copy theo profile; gallery recovery dùng
   synthetic vectors có thể làm độc lập. Theo [FR completion plan](../planning/face_recognition_completion_plan.md).
3. Nối enrollment/matching/attendance và FW storage/event boundary, sau đó release fault/soak.
4. Đo performance từ đầu; ưu tiên hotspot thật. Backend factory/multi-source ownership được
   sửa khi mở rộng scope, không che hạn chế bằng docs hoặc flags.
5. Async/shared context không phải điều kiện tiên quyết cho synchronous correctness.

## 5. Quy tắc đóng issue

Ghi source commit, tests/configuration, evidence location, giới hạn, owner và trạng thái
`planned / source-delivered / logic-tested / board-smoke / accepted` theo documentation_style.
`accepted` phải có workload/contract-specific evidence và owner review. Docs chỉ phản ánh
source đã có; A01–A25 chỉ đóng khi đạt tiêu chí evidence tương ứng.

## See also

- [Implementation status](implementation_status.md), [capability matrix](capability_matrix.md)
- [FR validation](../testing/face_recognition_production_validation.md)
