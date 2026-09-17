# Kế hoạch tối ưu kiến trúc LACAI

Kế hoạch này chuyển đánh giá tại
[architecture improvement review](architecture_improvement/README.md) thành thứ tự thực thi
có owner, contract, gate nghiệm thu và điểm dừng. Đây là đề xuất để review; nó không đổi
ABI, quyền sở hữu encode, entitlement hay quy tắc DMA, và không thay hồ sơ acceptance.

**Status:** planned — phân tích đã ghi nhận; chưa triển khai thay đổi nào. **Layer:** docs.
**Source:** `docs/planning/architecture_improvement/README.md`, source LACAI tại mốc `4689940`.

## 1. Nguyên tắc và phạm vi

- Không viết lại toàn hệ thống. Nền tảng neutral ports/ownership/lifecycle giữ nguyên; sửa
  đúng composition, đường output bền vững và hiệu quả memory/compute.
- Mỗi bước có một owner chính, contract/schema trước source, test dương/tính âm và bằng
  chứng eSDK/QEMU + board `.98`. Không đóng bước bằng tài liệu hay job bị skip.
- Nghiệp vụ → dữ liệu → semantics truy vấn → retention/SLO → index/storage → benchmark →
  engine. Không chọn storage/format trước khi có catalog.
- Alarm/evidence/identity là first-class: `unsupported`, `partial`, `unknown`, `expired`
  phải là kết quả được test, không bị trả thành `zero`/`false`.
- Không mở rộng sang feature/18 usecase đầy đủ trước khi P0–P2 xong.

## 2. Ánh xạ ưu tiên (tránh trùng namespace)

Đánh giá dùng nhãn A01–A16 cục bộ. Bảng dưới ánh xạ sang backlog hiện hành
[architecture alignment review](../development/architecture_alignment_review.md) A01–A25 để
một issue chỉ có một chủ.

| Assessment | Nội dung | Ánh xạ alignment review | Phase |
|---|---|---|---|
| A01 | Entitlement/admission dựng `true` trong production | A11 (một phần), A22 | P1 |
| A02 | Cùng reference feature factory cho mọi processor contract | A10 (một phần) | P1 |
| A03 | `reference_event_sink`, không outbox bền vững | A22, delivery mới | P4 |
| A04 | Graph/processor bỏ qua `source_slot` | A12 | P1 |
| A05 | Ép dimensions nguồn bằng nhau, cascade một source | A12 | P1/P7 |
| A06 | Renderer gọi trực tiếp, bỏ qua gate | A22, A09 (output) | P1 |
| A07 | PTS/overlay cache ghép latest nhiều model | A03, A18 | P1 |
| A08 | Chuỗi trust artifact chưa đóng kín | A19 | P1 |
| A09 | Cascade đồng bộ, stop `join` worker | A03, A13 | P3/P7 |
| A10 | ARM work còn lại: preprocess/align/copy/decode | A02, A14, A15 | P2/P3 |
| A11 | Chưa có attribute producer thực | A05, A10 | P4 |
| A12 | Feature/tracker/pose/OCR placeholder | A01, A08 | P4/P5 |
| A13 | Admission/capability chưa measured | A13, A18 | P2/P7 |
| A14 | Gallery key file-backed, chưa rotation/hardware | A04, A06 | P5 |
| A15 | CI chưa đồng bộ quy tắc eSDK/option | A25 | P0/P7 |
| A16 | Status tài liệu dễ gộp helper/production | A24 | P0 |

## 3. Quyết định phải chốt trước (blocking)

Không bắt đầu P4/P5/P6 khi các mục sau chưa có người phụ trách trả lời:

1. Retention local, số source/track tối đa, latency/concurrency query mục tiêu.
2. Usecase ID ổn định cho 18 mục security; profile/query nào mở trước; field được lưu/xuất.
3. Alarm nào bắt buộc durable/evidence; hành vi khi disk full/offline vượt quota.
4. Có bắt buộc burn-in vào evidence không; source/profile nào cấp prebuffer.
5. FW có nhận video ownership không; release/negotiation/rollback.
6. Cloud nhận field nào; embedding có bị cấm mặc định; ai giữ credentials.
7. DSP SDK/signing/cache/completion nào BSP cam kết trên QCS6490 / Linux 1.8.
8. Target CPU áp cho workload/nhiệt độ nào, có tính FW và data service không.
9. Traffic nào thuộc roadmap đầu (count/lane/ANPR hay signal/calibration-dependent).
10. Center application/infrastructure có nằm trong release này không.

Gate chung: mỗi mục có chủ, câu trả lời ghi vào authority doc/contract, và có fixture.

## 4. Lộ trình theo phase

Mỗi phase là một chuỗi commit focused, docs/contract/test đi cùng source.

### P0 — Authority, contract và harness benchmark

| Việc | Đầu ra | Owner | Gate |
|---|---|---|---|
| Soạn C01–C10 (ưu tiên C01, C03, C05, C06, C07) | schema/IDL + fixtures hợp lệ/lỗi + compatibility matrix | AI APP lead; BSP+FW/AI Model ký | Có consumer ký; authority docs cập nhật trước đổi boundary |
| ADR phạm vi video/data ownership | ADR mới hoặc mở rộng ADR 0004/0006 | AI APP lead + BSP+FW | Quyết định encoding/annotation/metadata owner |
| Chuẩn benchmark và manifest tái lập | script/manifest đo cùng điều kiện (board/BSP/QAIRT/model/cadence/thermal) | AI APP + BSP+FW | Chạy được trên `.98`, log ghim revision |
| Sửa A16/A15 | một đầu mối status; CI đồng bộ eSDK | AI APP | docs checker + CI structure |

Kế thừa contract đang có: C01/C05 từ [FW–AI contract](../contracts/fw_ai_app_contract.md),
[usecase control](../contracts/fw_usecase_control.md); C03 từ
[model integration](../contracts/model_integration.md).

### P1 — Đóng lỗ composition (đúng end-to-end)

Mục tiêu: tính đúng cục bộ của contract thành tính đúng production.

| Việc | Nguồn chính | Gate |
|---|---|---|
| Entitlement/admission lấy từ snapshot authority thật, không dựng `true` | `src/app/vqec_vision_service_main.cpp`, `production_platform` | Test hai feature dùng chung model nhưng chỉ một được cấp quyền; fixture chỉ thuộc harness |
| Chỉ đăng ký processor đúng schema/nghiệp vụ; unsupported bị từ chối | `production_platform.cpp` | Negative test: contract không khớp phải reject, không trả feature khác |
| Output production đi qua authorization/freshness/correlation/demand gate | `qtiv_renderer.cpp`, output gate/dispatch | Renderer không bypass gate; evidence không dùng timestamp suy đoán |
| Per-binding `source_slot` cho graph/processor; capability công bố đúng | `production_platform.cpp`, `runtime_composition_factory` | Hai nguồn khác kích thước dùng cùng detector chạy hoặc reject trước acquire |
| Chuỗi trust artifact: manifest xác thực, allowed roots, load bất biến | `src/runtime/model_registry/` | Hash không phải chữ ký; receipt không tự chống TOCTOU |
| Tách harness khỏi production composition root | `service_main.cpp` | Cùng owner-struct redesign nếu cần; không trộn fixture |

Đây là điều kiện tiên quyết để mọi số đo P2 có nghĩa.

### P2 — Baseline CPU và profile

1. Chuẩn hóa phép đo: cùng board/BSP/QAIRT/model/cadence/viewer/thermal; phân biệt CPU time
   với thời gian chờ DSP; không so FPS stream với inference FPS.
2. A/B: một detector tắt preview/FR/storage; thay CPU path bằng DSP path; preview 0/1/N
   viewer; FD+FR sweep số face; nhiều source cùng detector.
3. Metrics: CPU per-core, ms/inference, ms/ROI, stage p50/p95/p99, capture-to-result,
   event-to-durable-ACK, event-to-FW-ACK, queue age, drop theo nguyên nhân, RSS/PSS, FD,
   copied bytes/s, nhiệt/power.
4. Route `route_latency_*` chỉ là reservation→routing; bổ sung mốc end-to-end
   capture→recognition/evidence sau completion thật.

Gate: bảng acceptance CPU/latency/copy + accuracy regression; target 15–20% chỉ là mục tiêu
thử nghiệm cho workload được ký, chưa phải cam kết.

### P3 — Một vertical DSP (một detector)

Thứ tự (theo đánh giá mục 10.4):

1. Lease tensor buffer + result view; mở rộng contract memory thay vì framework song song.
2. cDSP preprocess → QNN HTP registered input/output → cDSP decode/NMS cho **một** detector.
3. Golden parity: color, letterbox, quantization, decoder/NMS, source transform.
4. Board completion: không ACK/recycle RAW/output trước hardware completion; DSP/HTP reset
   cần quiesce/quarantine.
5. Sau đó mới DSP face ROI/align/normalize với batch giới hạn/fairness.

Nguồn: `image_processor_port`, `inference_graph_port`, `model_decoder_port`,
`qualcomm/qnn_engine`, `fastcv_processor`, `fastcv_aligner`, `application_composition`.
Hexagon toolchain/SDK pin riêng; eSDK ARM vẫn là build chính.

### P4 — Metadata/event tối thiểu

1. Chốt common core schema person/vehicle/scene (D01–D18 subset), identity/interval/revision/
   coverage validators; không hardcode security-only.
2. Bounded ingest + projection tối thiểu + query facade cho Q01–Q08/Q14/Q27, mỗi query chỉ
   quảng bá khi producer tương ứng đã qualify.
3. Delivery: event ID ổn định, outbox bền vững, UDS `SOCK_SEQPACKET` + ACK theo mức, dedup,
   receipt; ba state machine event/delivery/evidence tách nhau.
4. Edge data service là boundary port/worker riêng; không I/O disk/Kafka trong frame loop.

Gate: query oracle security+traffic, `unsupported` rõ, crash/retry không nhân đôi evidence.

### P5 — Archive và cloud

- ADR storage sau benchmark: SQLite facts/index/outbox baseline; spike hybrid Parquet/DuckDB;
  không custom engine/graph DB trước khi chứng minh cần.
- Archive manifest/immutable sequence, hot/cold snapshot, correction/tombstone, purge.
- Kafka exporter (librdkafka, idempotent producer, delivery report) + center ingest
  workstream tách biệt có người/ngân sách; broker ACK không phải lake commit.

Gate: hot/cold consistency, offline quota, broker và lake receipt riêng.

### P6 — Chuyển video owner sang FW (khi được duyệt)

- Annotation contract C08; FW công bố capability render/encode/evidence + registry versioned.
- Giữ `legacy_ai_encode` + thêm `fw_video_owner` qua config/capability; một writer/ring.
- Prebuffer luôn có nếu nghiệp vụ cần; `no viewer` không đồng nghĩa `no evidence`.
- Chỉ gỡ adapter AI sau khi FW ký released-FW RTSP/UI/evidence conformance và rollback.
- Đo tổng CPU/DDR/power hệ thống, không chỉ process AI.

### P7 — Scale và vận hành

- Multi-source bindings, cascade fairness/bounded scheduling, control responsive.
- Bounded recovery sau quiesce/reset contract; không false completion.
- Admission từ profile đo trên board + FW concurrent load.
- Package/CI/soak, ma trận option (FastCV/control/enrollment), C10 deployment.

## 5. Thứ tự ngắn (đề xuất ~90 ngày)

1. P0 contract C01/C03/C05/C06/C07 + ADR video/data + benchmark harness.
2. P1 đóng lỗ composition A01/A02/A06/A08/A04/A05 (có negative test).
3. P2 baseline CPU/profile tái lập trên `.98`.
4. P3 vertical DSP một detector (song song P2 sau khi harness xong).
5. P4 metadata/event tối thiểu bắt đầu sau khi P1 ổn định; không chờ DSP xong.

## 6. Validation loop mỗi bước

```bash
source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux
cmake --build build-esdk-full -j4 && ctest --test-dir build-esdk-full --output-on-failure   # 123/123
bash tools/vqec_vision_check_source_layout.sh
bash tools/vqec_vision_check_docs_layout.sh
# board .98 (xem docs/testing/board_workspace.md)
#   native: 117/117 qua tools/vqec_vision_board_native_tests.sh
#   production smoke: H.264 1920x1080 30/1, dbus 5/5, first_error=0
```

DMA/cache/fence, DSP completion, storage throughput và performance chỉ kết luận bằng board,
không bằng QEMU/host.

## 7. Rủi ro và điểm dừng

- DSP skel cần toolchain Hexagon được duyệt và pin; nếu thiếu, dừng P3 và báo path/tool,
  không thay bằng host build.
- Chưa có profile mới để phân bổ CPU theo stage; không cam kết mức giảm CPU.
- Chưa benchmark SQLite/DuckDB/Parquet/librdkafka trên target; P5 là spike có điều kiện.
- Center là deliverable riêng; exporter edge xong không được báo là đã triển khai center.
- Không wire `inference_worker`/`recovery_controller` reserved chỉ để tăng số module xong
  (ADR 0006).

## 8. Không thuộc phạm vi kế hoạch này

- Không viết lại toàn bộ runtime; không thêm process per feature.
- Không chốt storage/format/Kafka trước catalog và benchmark.
- Không triển khai đủ 18 usecase, attribute producer, pose/OCR hay VLM trong các phase nền.
- Không đổi ABI/ownership/entitlement khi chưa có ADR/contract được duyệt.

## Xem thêm

- [Architecture improvement review](architecture_improvement/README.md) — phân tích nguồn
- [Architecture alignment review](../development/architecture_alignment_review.md)
- [Base completion plan](base_completion_plan.md), [clean-base plan](clean_base_plan.md)
- [System architecture](../architecture/system_architecture.md)
- [FW release compatibility](../contracts/fw_release_compatibility.md)
- [Model integration](../contracts/model_integration.md), [FW–AI contract](../contracts/fw_ai_app_contract.md)
- [Board workspace](../testing/board_workspace.md), [FR validation](../testing/face_recognition_production_validation.md)
