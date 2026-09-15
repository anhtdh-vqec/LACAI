# Kiến trúc hệ thống LACAI

Baseline thống nhất: 2026-09-15. Đây là định hướng kiến trúc hiện hành; mức triển khai
và bằng chứng nằm ở [implementation status](../development/implementation_status.md).
Phân loại toàn bộ tài liệu: [documentation map](../README.md).
Vấn đề còn lại: [architecture alignment review](../development/architecture_alignment_review.md).

Mục tiêu giữ nguyên: một AI APP thay thế ai_app trong FW, nhận RAW lease, chạy model và
usecase có cấu hình, tận dụng phần cứng Qualcomm qua adapters và portable sang vendor khác.
Executable là `vqec_ai_vision_applications`. FW đã đồng ý đáp ứng boundary; việc này không
thay thế kiểm chứng released-FW integration hay nghiệm thu tính năng.

Hiện có production composition và historical person compatibility evidence trên `.48`; current board validation target là `.99`. FastCV/QNN HTP,
QTI overlay/encode và ring. Luồng FD → exact-frame alignment → EdgeFace → typed embedding
đã được nối ở source qua neutral ports. Live cascade/golden parity, recognition/attendance,
durable gallery recovery, generic backend factory, released-FW DMA completion và
performance acceptance vẫn chưa hoàn chỉnh.

Quyết định backend: ADR 0002 xác lập tái sử dụng Qualcomm plugins; ADR 0003 bổ sung owned
QNN adapter cho capability/dtype cần thiết. Cả hai giữ neutral ports. Ưu tiên tái sử dụng
vendor implementation đã kiểm chứng; không bắt buộc mọi công đoạn phải dùng plugin khi
SDK adapter có lý do và bằng chứng. Production hiện ghép FastCV + owned QNN trực tiếp;
backend selection hoàn toàn theo capability/config vẫn là mục tiêu chưa hoàn tất.
ADR 0004 chọn Zvec cho index FR, AI sở hữu matching/gallery semantics, FW cung cấp
protected storage/provisioning. Đây là thay đổi có chủ đích so với đề xuất gallery/search
thuộc FW ban đầu; giao thức storage cụ thể vẫn cần owner review.

Normative FW baseline: [FW release compatibility](../contracts/fw_release_compatibility.md).
Deployment: [multi-source configuration](multi_source_configuration.md).
FR implementation: [completion plan](../planning/face_recognition_completion_plan.md).

## 1. Ranh giới và nguyên tắc

- Unified FW RAW Source Service thuộc FW BSP/FW software; AI APP là consumer theo
  cùng lease contract trên AI Camera và AI Box. RTSP demux/decode của AI Box thuộc FW.
- AI Model bàn giao model integration package, không chỉ binary.
- AI APP sở hữu scheduler, model integration, perception, feature rules, lifecycle,
  entitlement enforcement, output schema và đo hiệu năng.
- FW software sở hữu installation/supervision, config endpoint, key provisioning,
  evidence storage/upload, protected storage/key provisioning và retention.
  AI APP sở hữu enrollment/matching/index synchronization qua neutral storage/index ports;
  FW cung cấp transport/UI và storage policy theo contract đã version hóa.
- BSP sở hữu driver/ISP/SDK, memory interoperability, cache/fence/reset contracts.
- Không OpenCV. QNN/FastCV/GStreamer chỉ trong adapter hoặc benchmark tools.
- Một process ai service ở v1; phân module theo dependency, không chia process theo bài.
- Internal trusted plugins; controlled restart khi update; chưa hot unload.

## 2. Luồng dữ liệu

The diagrams define intended ownership. Current production person composition uses
multi_source_supervisor/session/pump, private FastCV preprocessing, owned QNN, decoding,
reference tracking and QTI preview output. The app composition root may include concrete
adapters; orchestration and neutral contracts depend only on ports. The secondary cascade
is source-composed and logic-tested, while live model/golden evidence remains an
integration target.

Mandatory released preview path (in addition to the feature/event design below).
The diagram shows one source; runtime repeats the source-owned state for 1..16 admitted
sources while sharing only proven-compatible model resources:

```text
FW RAW NV12/FD -> source scheduling -> preprocess/inference -> tracks
                              |                                     |
                              v                                     v
                     AI-owned preview surface <---------- overlay commands
                              |
                         overlay renderer -> H264 encoder -> FW v4 ring sink
                                                                  |
                                                released RTSP -> MediaMTX -> UI

Legacy AI D-Bus -> compatibility adapter -> serialized model/runtime control
```

AI APP owns burned-in preview overlay, encoding and ring production. FW retains
camera capture, main/sub encoding, RTSP/UI, recording and persistent evidence.
No viewer means no preview frame submissions, not automatically no inference.
Create/attach the ring before the first frame so viewer registration can trigger output.
Do not gate preview availability on an existing encoded frame. Do not couple video
cadence to the single outstanding inference job; scheduling separates these demands.
Ring SDK types stay private in adapters/fw_output; overlay commands and encoded-AU
ports stay neutral. The QTI production renderer/encoder/ring compatibility flow exists. General output-port
composition and released-FW conformance must be validated separately.

```text
FW RAW Source Service -- frame descriptor + handles --> source adapter
                                                   |
                                         source acquisition manager
                                                   |
                                         bounded scheduler/admission
                                                   |
                         Qualcomm image processor (FastCV; C2D optional)
                                                   |
                                        AI-owned tensor buffer pool
                                                   |
                                         Qualcomm QNN engine
                                                   |
                               decoder -> detections/pose/embedding/OCR
                                                   |
                                  per-source tracking + attribute cache
                                                   |
                           feature rules / temporal windows / relations
                                                   |
                                 output router + entitlement check
                                                   |
                                FW events / evidence / live data; AI gallery index

FW config + entitlement --> feature manager --> dependency graph + admission
Health / effective state / metrics -------------------------------> FW
```

Dependent ROI models use the bounded design in
[cascade inference](cascade_inference.md). They retain the exact source frame through
secondary completion and do not enter the full-frame multi-model cadence fan-out.

Release FW RAW frame lease sau khi TẤT CẢ image jobs đọc frame đó hoàn tất.
Inference thường dùng tensor AI-owned, không giữ RAW frame lease.
Temporal windows giữ crop/tensor nhỏ có budget, không giữ một dãy frame 4K.
Không suy ra không-copy-tensor từ việc FW RAW input là DMA-BUF.

## 3. Source tree và dependency

| Layer | Trách nhiệm | Được phụ thuộc |
|---|---|---|
| contracts + plugin | neutral descriptors/interfaces/versioned ABI | std C++ / C types |
| core | status, clock, lease primitives, bounded queue, geometry | contracts, std |
| runtime | lifecycle, graph, scheduling, admission, registry | contracts, core |
| perception | decode, tracker, typed attrs, pose, embedding, OCR | contracts, core |
| features | rules cho 13 bài, traffic | contracts, core, perception |
| adapters | camera/FW transport/vendor implementation | contracts, core, SDK private |
| outputs | routing, serialization, delivery policy | contracts, core |
| app | composition root, wiring concrete components | all above |

Không cyclic dependency. Runtime không include concrete Qualcomm header;
app inject factory/port. Shared public headers ở include/vqec/vision/ai;
private headers cạnh implementation. Một CMake target cho module có boundary
độc lập. Không giant common; không ../ kéo header sang repository firmware.

## 4. Contract roles và trạng thái

frame_descriptor, frame_lease, buffer_handle, buffer_view, tensor_descriptor,
tensor_view, image_transform, model_spec, inference_job, job_completion,
observation, track, entity, attribute, relation, feature_event, aggregate.

Ports: frame_source, buffer_manager, image_processor, inference_engine,
model_decoder, feature, entitlement_provider, event_sink, evidence_client, clock.
Đây là vai trò kiến trúc, không phải danh sách tên API đã có. API thực tế nằm trong
include/vqec/vision/ai và naming registry; capability chưa hỗ trợ phải reject activation.

- Image transform: ROI trong source pixels, rotation, letterbox và nghịch đảo.
- Buffer view không sở hữu; job giữ owner sống đến completion.
- Tensor dtype/quantization từng tensor; nhiều input/output, graph_name rõ.
- Track identity = source + epoch + track id; không đồng nhất với person identity.
- Attribute có schema id/version, value/confidence, quality, timestamp/expiry,
  model version và known/unknown/not_observable; không ép nhãn khi chất lượng kém.
- FR/embedding nhạy cảm, không đưa vào live output nếu thiếu quyền.
- Coordinates luôn ghi frame/space/unit; không trộn normalized box với pixels.

## 5. Thread model và lifecycle

Control loop serialize config/license/start/stop; input thread nhận frame;
bounded workers gọi backend; một state executor mỗi source; output worker tách.
Không mặc định mỗi model có nhiều thread/context; phải đo thread safety SDK.

Feature state: installed -> eligible -> loading -> ready -> running;
nhánh disabled, unsupported, denied, resource_limited, degraded, faulted.
desired_enabled khác effective_state; reason_code luôn trả được cho FW.

Start: validate config/license -> resolve dependencies -> resource admission ->
load model + warmup -> acquire source -> run -> publish readiness.
Stop: reject new work -> drop safely unsubmitted work -> drain submitted jobs ->
close outputs at revision boundary -> release leases -> release model/context.
Timeout drain: báo fault/quarantine; phối hợp BSP reset/quiesce; không tự ACK giả.
Camera/source reset: tăng epoch; clear tracker/temporal state, mark output gap;
không emit người mới đi qua line chỉ vì ID reset.

## 6. Chia sẻ compute và resource admission

Key chia sẻ gồm source/profile, model artifact/hash, preprocess, input shape/dtype,
ROI policy, cadence và quality requirements. Chỉ share khi mọi consumer đáp ứng.
Feature refcount không đủ nếu FPS/ROI khác; manager reconcile aggregate demand.

Admission nhận số nguồn, resolution/FPS chính xác của từng nguồn, model
latency/memory, crop rates, temporal
windows, pool capacity, concurrent FW encode/record. Reject có lý do nếu vượt.
Mỗi queue có max depth, tuổi job, priority, drop strategy, metrics.
Temporal feature xử lý missing frame theo model contract; không tùy tiện leaky.
FR/attribute chạy theo quality/cooldown/track change với max ROI per frame,
không mỗi người x mọi model x mọi frame không giới hạn.
Với mọi profile, packed NV12 = width * height * 3/2; allocation thật còn có
stride/padding. 3840x2160 packed = 12,441,600 bytes/frame và khoảng 311 MB/s ở
25 FPS chỉ là ví dụ sizing trước copy/DDR bổ sung, không phải runtime default hay
benchmark board.

## 7. Output, entitlement, package

Live tracks có thể lossy; alarm bounded durable delivery retry + dedup;
counter checkpoint/window; heatmap bucket; AI FR sở hữu gallery/search semantics,
Zvec sau embedding_index_port và persistence qua FW protected-storage boundary.
Alarm chứa event_id, source/epoch/timestamp, feature/config/model versions,
track refs, geometry, evidence request id; không tự copy video encode trong feature.

effective = installed AND licensed AND desired AND supported AND compatible
AND resource_admitted. Tắt feature chỉ gỡ dependencies không còn consumer.
Khi entitlement revoke: chặn output nhạy cảm ngay theo revision, drain compute;
không publish kết quả cũ sau revoke. Offline revocation có giới hạn đã thỏa thuận.

Package đề xuất: ai-runtime; ai-backend-qualcomm; ai-feature-<bundle>;
ai-model-<model>-<target>. Bundle là đơn vị deploy, feature_id là đơn vị thương mại.
Mỗi catalog model được map chính xác tới metadata/artifact triển khai qua
[model package registry](model_package_registry.md); runtime không dùng chung một path
ngầm định cho mọi model.
Manifest pin runtime ABI/backend/model compatibility; staging + validate +
controlled restart + health check + rollback coordinated với FW.
Model signed/checksum-verified, thư mục readonly; state/config ở vị trí FW cấp.
Không upgrade .so đang active rồi hot unload; không coi IPK dependency tự bảo đảm
atomic multi-package update.

## 8. Observability và giới hạn v1

Metrics: captured/accepted/dropped frames + reason; queue age; per-stage p50/p95/p99;
camera hold time; pool use; FD count; RSS; SDK jobs; thermal throttling; model warmup;
feature effective state; event retries/loss; source/model/config epochs.
Log có source/job/model/feature/correlation id, không per-frame INFO hoặc biometrics.

Chưa bao gồm graph editor, arbitrary third-party plugins, hotload, universal
optimizer, tự viết vector database engine hoặc bảo đảm cùng workload trên 4 vendor.
Zvec là dependency FR đã chọn, không phải một DB engine tự phát triển.
Traffic extension dùng entity/track/attribute/relations + OCR/calibration contracts.
Mọi claim production phải đi kèm board image + model + workload + dataset report.
