# Kế hoạch DSP và tối ưu đa nền tảng

Plan này giảm ARM CPU bằng cách đưa pixel/tensor loops lớn vào backend accelerator, bắt
đầu Qualcomm cDSP + QNN HTP, nhưng giữ neutral ports và semantics dùng được trên platform khác.
Không gọi offload là zero-copy hay acceptance cho đến khi có completion/cache/board evidence.

- **Status:** planned — chưa có DSP implementation mới trong plan này.
- **Layer:** docs
- **Source:** [owned QNN engine](../../adr/0003_owned_qnn_engine.md),
  [tensor contract](../../architecture/tensor_output.md), `n/a` cho đề xuất mới.

## Trách nhiệm

- AI APP owner execution orchestration, neutral buffer/result contracts, Qualcomm adapter,
  DSP kernels integration, profiler và end-to-end acceptance.
- BSP+FW owner Hexagon/QNN SDK/toolchain/signing, allocator/import/cache/fence/reset,
  power/thermal/resource traces và target completion evidence.
- AI Model owner tensor/preprocess/decode semantics, quantization, golden parity, quality;
  không đổi model semantics chỉ để đạt CPU thấp.

## 1. Mục tiêu và thứ tự ưu tiên

Mục tiêu trước mắt là giảm `CPU ms/accepted inference`, copied bytes/s và ARM RSS trong
workload đã ký, đồng thời giữ FPS, latency, accuracy, ownership và thermal budget. Ưu tiên:

1. profile allocation/copy/decoder/aligner/renderer;
2. reuse bounded workspace/pool và loại copy không ảnh hưởng completion;
3. detector vertical: DSP preprocess → QNN registered input/output → DSP postprocess/NMS;
4. face ROI align/normalize và embedding khi model kit đủ;
5. preview compose/encode ownership chỉ theo video migration plan, không lẫn với model DSP.

Không đưa SQLite/Kafka/D-Bus/feature branching lên DSP. Không bọc mọi CPU path bằng từ
“hardware”; backend phải báo capability hoặc fail closed nếu policy yêu cầu accelerator.

## 2. Neutral contract cần có

Đề xuất mở rộng/revision các port hiện có, không tự tạo API vendor trong core:

| Vai trò | Bắt buộc |
|---|---|
| `tensor_buffer_lease` | shape/dtype/layout/strides/quantization, capacity, domain, generation, owner |
| CPU map view | access mode, map/cache sync scope, bounded lifetime; map optional |
| opaque device memory | token không lộ QNN/FastRPC/Gst/Hexagon type |
| completion ticket | submit/completion/quiesce; timeout không phải hardware cancel |
| decoder result | compact observations, count/bounds/finite validation, source ticket/epoch |
| capability profile | operations, tensor shapes, import/export, cache/fence, max in-flight, cost |

Input/output không được reuse khi DSP/HTP còn đọc/ghi. Mapping cache theo allocation
identity + generation + layout + context, không theo FD number. Stop/disconnect/close
chỉ bắt đầu recovery/quarantine; completion phải từ SDK/BSP hoặc recovery guarantee.

## 3. Qualcomm vertical

```text
RAW lease -> Qualcomm image adapter -> cDSP preprocess -> QNN HTP
                                      registered tensor pool
                 compact cDSP postprocess/NMS -> neutral decoder -> tracker/features
```

- Allocate/register rpcmem/ION ở activation; model manifest cung cấp exact tensor identity.
- cDSP xử lý NV12 planes theo offset/stride/range, resize/letterbox/normalize/pad và dtype
  conversion đúng golden; không dùng một công thức quantization cho mọi model.
- QNN nhận registered input/output nếu capability; output result giữ lease đến decoder
  xong, không memcpy về vector nếu contract đã chứng minh borrowed view/completion.
- DSP postprocess trả compact boxes/kps/labels/candidates; ARM vẫn validate bounds, quality,
  source ticket và epoch. Candidate cap là model contract, không hidden truncation.
- Face cascade giữ exact frame lease; DSP crop/align/normalize nhiều ROI theo bounded batch,
  embedding output chuyển matcher khi completion thật.
- Mỗi flow/model có scratch/pool bounded; scheduler admission tính đồng thời HTP+cDSP,
  DDR, FW decode/encode, nhiệt và power.

## 4. Multiplatform adapter boundary

- Neutral layer mô tả operation/semantics; không có enum “Qualcomm-only”.
- Qualcomm adapter chứa FastRPC IDL/stub, rpcmem/ION, QNN memhandle, FastCV/HVX và power votes.
- Reference CPU adapter dùng golden/replay; nền tảng khác implement cùng port bằng NPU/GPU/
  accelerator/CPU theo capability, không silently thay precision/layout.
- Backend selector dùng model catalog + capability + measured profile + admission. Unsupported
  hoặc accelerator-required thiếu capability phải trả reason; CPU fallback chỉ khi policy nói rõ.
- DSP decoder là implementation của semantic decoder contract; model kit/decoder version
  phải pin ontology, NMS, coordinate transform, threshold/tie behavior.
- Vendor library/toolchain không vào `include/vqec/vision/ai` hoặc perception/features.

## 5. Task thực hiện

| Task | Owner | Đầu ra | Tiêu chí kết thúc |
|---|---|---|---|
| D01 | AI APP | CPU/copy flamegraph và per-stage counters baseline | workload manifest, warm/thermal state, p50/p95/p99 |
| D02 | BSP+FW | C02 accelerator matrix, SDK/toolchain/signing/reset contract | target diagnostic + completion/cache trace |
| D03 | AI Model | C03 one detector golden pre/post/decode + quantization | M0–M4 report, exact tensors/layout/quality |
| D04 | AI APP | tensor lease/result view contract + reference implementation | ownership/epoch/stop tests pass |
| D05 | AI APP+BSP+FW | Qualcomm cDSP preprocess + registered QNN vertical | eSDK ARM + approved DSP build; no vendor leakage |
| D06 | AI APP+AI Model | cDSP postprocess/NMS parity | golden candidate/box/score/tie/threshold pass |
| D07 | AI APP | face ROI/embedding path hoặc quyết định chưa làm | quality/ROI cap/thermal benchmark |
| D08 | AI APP+BSP+FW | target A/B report CPU/DDR/FPS/latency/thermal | measured benefit and no lifetime violation |
| D09 | AI APP | second backend/reference parity test | same neutral result semantics or explicit unsupported |

## 6. Chỉ số benchmark bắt buộc

Đo cùng source/profile/model hash/cadence/objects/ROIs/viewer/evidence/thermal/governor.
Ghi process và system CPU, CPU ms/inference/ROI, copied bytes, allocations, RSS/PSS,
capture→result p50/p95/p99, FPS/drops/queue age, HTP/cDSP utilization nếu có, DDR/power/
temperature và accuracy. So CPU một logical core với tổng board CPU phải ghi riêng.

Không dùng startup microbenchmark thay sustained soak. CPU giảm nhưng queue age/FPS/accuracy
xấu là không đạt. Mục tiêu 15–20% chỉ là target workload được ký, không cam kết chung.

## 7. Tiêu chí nghiệm thu

- [ ] Neutral source/header không include QNN/FastRPC/FastCV/Gst/Hexagon types.
- [ ] Tensor contract kiểm dtype/layout/shape/quantization/stride/size/generation; map view
  lifetime và hardware completion được test, FD reuse không nhầm allocation.
- [ ] DSP path có golden NV12/NV21/stride/offset/range/crop/letterbox/quantization và decoder
  parity; unknown/invalid/candidate cap đúng model contract.
- [ ] Không có early ACK/recycle/reuse khi DSP/HTP còn truy cập; DSP reset đi qua quarantine/
  recovery evidence, không dùng timeout/destructor để giả completion.
- [ ] Qualcomm build dùng toolchain được BSP duyệt; ARM integration build/test dùng eSDK;
  signed/runtime library provenance và package được kiểm tra.
- [ ] A/B giữ nguyên workload và semantics; report có CPU/copy/DDR/latency/FPS/accuracy/
  thermal, không chỉ log “DSP initialized”.
- [ ] Reference CPU path và một backend khác compile/logic-test hoặc trả unsupported rõ;
  không hardcode Qualcomm trong runtime/features.
- [ ] Face/traffic DSP chỉ bật khi C03 quality/calibration/time dependencies đạt; không đưa
  embedding/plate sensitive data vào log hoặc shared memory debug.

## Handoff

D02/D03 là đầu vào từ contract plan. D01 baseline chạy cùng integration plan. D05/D06 chỉ
bàn giao production sau board completion + golden; event/storage plans nhận compact neutral
results, không phụ thuộc vendor buffer.

## Giới hạn và công việc tiếp theo

- Chưa có Hexagon toolchain/source DSP mới trong LACAI; không claim target DSP acceptance.
- Registered QNN output hiện có trong source không đồng nghĩa input zero-copy/completion đã đóng.

## See also

- [Architecture improvement master plan](README.md)
- [Contract and team scope plan](contract_and_team_scope.md)
- [Model integration](../../contracts/model_integration.md)
- [Tensor output](../../architecture/tensor_output.md)
- [ADR 0003 owned QNN engine](../../adr/0003_owned_qnn_engine.md)
