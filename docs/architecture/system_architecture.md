# Kiến trúc hệ thống v1

Status: design baseline; cross-team contracts pending agreement.
Source inventory update (2026-09-09): the private Qualcomm graph has submit/result/drain;
camera_session and multi_model_session compose one acquisition lifecycle. The portable
multi_model_pump shares one frame across due graphs using rational cadence, and
multi_source_supervisor advances 1..16 pre-composed sessions with fault isolation.
Deployment/catalog loaders and activation snapshots exist; authenticated construction of
source/graph/session owners and a runnable service are still missing. Model decoder,
feature dependency catalog and model-to-feature routing contracts exist; concrete model,
tracker/feature algorithms, hardware overlay/encoder integration and board validation are
not delivered. Configured AArch64 eSDK targets cross-build, but target tests are not run in
the x86 workspace.
See [current source inventory](../development/implementation_status.md).
Preview ownership/pool, encoder ledger, authorized dispatch and an optional FW SDK
ring sink are source-delivered; they do not yet form a running output pipeline.
Normative external baseline: [FW release compatibility](../contracts/fw_release_compatibility.md).
Implementation sequence: [FW replacement gates](../planning/fw_compatibility_execution.md).
Multi-source/configuration baseline:
[multi-source configuration](multi_source_configuration.md).
Mục tiêu 12 tuần: runtime + Qualcomm vertical slice + release workload đã nghiệm
thu; 13 feature không có nghĩa 13 model luôn chạy đồng thời hoặc mọi SoC đã hỗ trợ.

## 1. Ranh giới và nguyên tắc

- Unified FW RAW Source Service thuộc FW BSP/FW software; AI APP là consumer theo
  cùng lease contract trên AI Camera và AI Box. RTSP demux/decode của AI Box thuộc FW.
- AI Model bàn giao model integration package, không chỉ binary.
- AI APP sở hữu scheduler, model integration, perception, feature rules, lifecycle,
  entitlement enforcement, output schema và đo hiệu năng.
- FW software sở hữu installation/supervision, config endpoint, key provisioning,
  evidence storage/upload, gallery/search service và retention.
- BSP sở hữu driver/ISP/SDK, memory interoperability, cache/fence/reset contracts.
- Không OpenCV. QNN/FastCV/GStreamer chỉ trong adapter hoặc benchmark tools.
- Một process ai service ở v1; phân module theo dependency, không chia process theo bài.
- Internal trusted plugins; controlled restart khi update; chưa hot unload.

## 2. Luồng dữ liệu

The diagrams describe the complete intended pipeline, not completed integration.
Current orchestration source is `multi_source_supervisor -> source_session_port ->
multi_model_session -> multi_model_pump -> raw_source_port / inference_graph_port`.
Perception/features and hardware output stages below remain planned. The current Qualcomm
inference implementation follows ADR 0002's private converter/QNN plugin graph; direct
FastCV/QNN SDK modules in the conceptual flow are not separately implemented adapters.

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
ports stay neutral. Their contracts and partial source implementations exist;
hardware rendering/encoding and runtime ring lifecycle integration remain pending.

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
                                FW events / evidence / search / live data

FW config + entitlement --> feature manager --> dependency graph + admission
Health / effective state / metrics -------------------------------> FW
```

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

## 4. Contracts nội bộ cần implement trước

frame_descriptor, frame_lease, buffer_handle, buffer_view, tensor_descriptor,
tensor_view, image_transform, model_spec, inference_job, job_completion,
observation, track, entity, attribute, relation, feature_event, aggregate.

Ports: frame_source, buffer_manager, image_processor, inference_engine,
model_decoder, feature, entitlement_provider, event_sink, evidence_client, clock.
Đây là vai trò kiến trúc, tên method sau này theo naming registry.

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
counter checkpoint/window; heatmap bucket; search index/gallery qua FW service.
Alarm chứa event_id, source/epoch/timestamp, feature/config/model versions,
track refs, geometry, evidence request id; không tự copy video encode trong feature.

effective = installed AND licensed AND desired AND supported AND compatible
AND resource_admitted. Tắt feature chỉ gỡ dependencies không còn consumer.
Khi entitlement revoke: chặn output nhạy cảm ngay theo revision, drain compute;
không publish kết quả cũ sau revoke. Offline revocation có giới hạn đã thỏa thuận.

Package đề xuất: ai-runtime; ai-backend-qualcomm; ai-feature-<bundle>;
ai-model-<model>-<target>. Bundle là đơn vị deploy, feature_id là đơn vị thương mại.
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
optimizer, standalone DB/search backend hoặc bảo đảm cùng workload trên 4 vendor.
Traffic extension dùng entity/track/attribute/relations + OCR/calibration contracts.
Mọi claim production phải đi kèm board image + model + workload + dataset report.
