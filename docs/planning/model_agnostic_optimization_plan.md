# Kế hoạch tối ưu base LACAI độc lập model

Ngày: 2026-09-12. Baseline review: `0b82e5d`.
Trạng thái: backlog và thiết kế đề xuất; không phải các tối ưu đã triển khai/nghiệm thu.
Tài liệu tổng hợp review source hiện tại và các gate còn lại của base.

## 1. Mục tiêu và nguyên tắc bắt buộc

Base phải tiếp nhận model qua metadata, package và capability, không sửa core/pump
theo tên model. Không thể bảo đảm mọi model chạy trên mọi accelerator hoặc đạt cùng
hiệu năng: model ngoài capability phải bị reject có lý do. Model mới trong capability
đã hỗ trợ chỉ cần package/config/decoder phù hợp, không thêm nhánh tên model trong runtime.

Ưu tiên tận dụng implementation đã được tối ưu và phần cứng Qualcomm thông qua adapter.
Tái sử dụng plugin/SDK cho conversion, resize, inference, transform, overlay và encode khi
đúng semantics và có bằng chứng. Không viết lại vendor stack để thuận tiện composition.
Không chọn accelerator chỉ vì tên gọi; đo cả transfer, synchronization, latency và power.

- Core/runtime/perception/features không chứa QNN/Gst/FastCV type hoặc vendor path.
- CPU reference phục vụ golden và chế độ dự phòng được cho phép rõ; không silent fallback.
- Policy, thresholds, timeouts, paths, pool budgets đến từ config đã validate; fixed
  ABI/schema constants có semantic owner và provenance. `constexpr` không che hardcode.
- Correctness, authorization và completion là điều kiện trước tối ưu.
- Không release/reuse input khi hardware còn đọc; timeout/FD close không là completion.
- Trần software không là capacity board. Không tăng queue để che processing lag.
- Không coi `std::move`/`swap` là deep copy; cũng không coi chúng là bằng chứng hết allocation.
- Plugin và direct SDK cùng tuân neutral port; backend selection là policy/capability,
  không logic theo product/model name.

Tham chiếu: [plugin adapter](../architecture/qualcomm_plugin_adapter_reference.md),
[ADR 0002](../adr/0002_qualcomm_plugin_backend.md),
[base audit](../development/base_audit.md), [base completion](base_completion_plan.md),
[FW release](../contracts/fw_release_compatibility.md),
[convention](../development/code_convention.md).

## 2. Hiện trạng và mức bằng chứng

Đã có plugin graph, direct-QNN loader/engine/graph/bundle, neutral image-processor port,
CPU reference processor, tensor submission trong pump và native tensor blobs. Đây là
tiến bộ về integration/capability, chưa chứng minh throughput hoặc DDR tốt hơn.

Các đường dẫn source dưới đây tương đối repository root. “Quan sát” là control flow
đã thấy; “cần đo” không phải kết luận đã có lỗi/performance regression trên device.
Các lỗi B01–B07 từng sửa phải giữ regression; không tự đưa chúng trở lại backlog mở
nếu chưa tái hiện. B07 authentication/production owner integration vẫn cần gate riêng.

| ID | Ưu tiên | Bằng chứng / vấn đề | Tác động và phạm vi |
|---|---|---|---|
| O01 | P0 | `src/adapters/qualcomm/vqec_vision_qnn_engine.cpp`: probe dựa trên function pointer để công bố async/shared memory/update và giới hạn | SDK API có mặt chưa chứng minh adapter triển khai. Admission có thể nhận policy vượt đường execute thực tế |
| O02 | P1 | Cùng file: `graphExecute` đồng bộ; `qnn_inference_graph.cpp` gọi trong submit; pump gọi submit trực tiếp | Source/model chậm có thể chặn serialized executor. Cần đo fairness/latency, không suy ra async từ tên port |
| O03 | P1 | `src/adapters/reference/vqec_vision_reference_processor.cpp`: mmap và vòng lặp pixel/channel | CPU baseline chưa tận dụng Qualcomm preprocessing; phải tách rõ mode production/reference |
| O04 | P1 | QNN execute dùng `QNN_TENSORMEMTYPE_RAW`/clientBuf | Chưa nối registered-buffer contract vào execution này; staging/copy nội bộ SDK chưa đo |
| O05 | P1 | QNN execute tạo vector output và resize bytes mỗi call | Allocation/initialization trên hot path; metadata cũng dựng lại. Output được QNN ghi trực tiếp, không gọi đó là copy output bổ sung |
| O06 | P1 | `src/adapters/qualcomm/vqec_vision_tensor_output.cpp`: resize + memcpy mỗi tensor | Copy rõ từ mapped sample vào owned bytes. Đây cũng là boundary ownership an toàn; chỉ bỏ khi có lease/completion tương đương |
| O07 | P1 | `src/app/vqec_vision_multi_model_pump.cpp`: preprocess từng slot, vector blobs local, input specs lấy lần đầu nhận frame | Conversion trùng giữa model tương thích; allocation/metadata work chưa hoàn toàn chuyển sang activation |
| O08 | P1 | Stage scratch/swap nhưng perception result stage/router tạo candidate local; executor move output arrays | Capacity chưa được tuần hoàn end-to-end; nested vector/string có thể cấp phát lại. Cần allocation instrumentation |
| O09 | P1 | Tensor preprocess branch của pump yêu cầu đúng một input | Neutral contract rộng hơn orchestration hiện tại; multi-input/dynamic/stateful models cần explicit support hoặc reject |
| O10 | P1 | Direct QNN input validation hiện kiểm dtype và byte count | Cùng bytes không chứng minh cùng shape/layout/name/quantization. Cần đối chiếu toàn bộ contract trước execute |
| O11 | P1 | Capabilities có multi-model domain/LoRA/shared-memory contracts | Contract tồn tại không chứng minh context sharing, update atomicity hoặc registration lifecycle đã chạy; chưa được dùng làm production claim |
| O12 | P1 | Plugin và SDK backend song song; tài liệu adapter còn mô tả plugin là implementation duy nhất | Dễ duplicate code, lệch validation và chọn đường CPU chậm hơn mà không biết |
| O13 | P1 | Source lease, shared tensor, graph, output có lifetime khác nhau | Tối ưu pool/async có thể gây early ACK, reuse trước completion hoặc giữ RAW quá lâu; phải có ownership ledger |
| O14 | P1 | Preview helpers/encoder ledger/ring adapter chưa tự tạo full hardware output path | Model optimization chưa đủ cải thiện hệ thống khi render/encode/delivery chưa ghép hoặc nghẽn |
| O15 | P1 | Event delivery/policy/retry và runtime result buffering | Backpressure output có thể làm inference chờ; alarm/live output cần policy khác nhau, bounded retry và captured revision |
| O16 | P1 | Admission dựa metadata/capability, chưa thay workload measurement | CPU/HTP/DDR/thermal/encode contention có thể vượt budget dù tensor bytes hợp lệ |
| O17 | P2 | Source gap, ROI/temporal join và dynamic shape cần resource policy | Không suy ra mọi task được hỗ trợ từ một image model; recurrent state cần epoch/reset và bounded storage |
| O18 | P1 | CI/test counts tùy option, QNN proprietary SDK không luôn có trong sysroot | Test xanh với option OFF không validate direct QNN; cần config/evidence matrix và board qualification |
| O19 | P2 | Literal/README/status và ABI expectations thay đổi nhanh | Cần inventory sống; tránh hardcoded defaults lan từ reference sang production |

## 3. Kiến trúc đích và copy ledger

```text
validated immutable deployment + model packages + authorization
  -> capability resolution + measured resource admission
  -> app composition (neutral ports, owners, stable slots)
FW RAW lease -> source scheduling -> compatible preprocess groups
  -> Qualcomm image adapter -> pooled tensor lease
  -> selected inference adapter -> pooled/native output lease
  -> decoder package -> tracker/attributes -> feature packages
  -> authorized event delivery / independent preview branch
```

Alternative vendor thay adapter factory/allocator/sync implementation. Scheduling,
admission rules, output authorization, semantic decoder/usecase giữ neutral. Model
package được phép pin target compatibility; không bắt mọi vendor có cùng capability.

| Boundary | Hiện trạng / việc phải đo | Đích và điều kiện |
|---|---|---|
| FW → RAW adapter | Lease/FD sharing không tự copy pixels | Preserve native owner; record stride/modifier/sync; CPU access đúng cache contract |
| RAW → tensor | CPU baseline hoặc plugin conversion; plugin có thể staging | Reuse Qualcomm processor, pool compatible outputs; đo bytes read/write/staging |
| Tensor → QNN | RAW client buffers ở direct path | Register/import khi supported; fallback explicit; no zero-copy claim từ FD alone |
| QNN → result | Direct path allocated output; plugin mapped-to-owned copy | Pooled output; optional borrowed sample lease có bound, readiness và release contract |
| Result → observations | Decode/track semantic transformation + container churn | Numeric metadata + bounded arena/output slots; đây không phải zero-copy pixels |
| Observations → events | Strings/fields và policy validation | Pre-sized payloads, bounded delivery envelope, original revisions |
| Preview → encode/ring | AI phải có writable surface riêng | Hardware transform/render/encode; không xóa necessary ownership copy bằng sửa RAW chung |

Mỗi boundary ghi: allocation owner, byte count, copy reason, device/CPU access,
cache/fence, last reader, release event, maximum outstanding và overload action.
Đếm allocations, byte movement và buffer hold time riêng; `memcpy` grep không đo
SDK staging, GPU transfer hoặc cache maintenance.

## 4. Kế hoạch thực thi chi tiết

### S01 — Capability trung thực (O01/O11), làm trước

1. Inventory từng capability: SDK symbol, implemented adapter operation, supported
   dtype/layout/shape, concurrency, memory mode và board evidence.
2. Phân biệt available/implemented/qualified/admitted. Effective capability là giao
   của adapter + model + backend + policy, không dùng symbol presence một mình.
3. Khi chỉ synchronous một job, công bố đúng một job; không bật shared/update/async
   cho tới khi đường lifecycle tương ứng triển khai.
4. Negative tests: symbol có nhưng adapter chưa support; unsupported perf mode,
   registration budget, concurrency và update policy bị reject trước load/submit.

Gate: không policy nào được accept mà operation tương ứng không thực thi được.
Owner: Qualcomm + runtime; lead review contract.

### S02 — Baseline đo và lựa chọn backend (O12/O16/O18)

1. Pin LACAI commit, eSDK, image/kernel, plugin/QNN binary, firmware và package digest.
2. Dựng benchmark cùng inputs/metadata cho plugin path, direct-QNN path và CPU reference.
3. Đo correctness trước latency; tách load/warmup/steady-state/stop và transfer/compute.
4. Ghi reason chọn backend. Giữ plugin reuse mặc định theo ADR hiện hành; direct QNN
   là lựa chọn khi có capability gap hoặc lợi ích đo được. Nếu thay ưu tiên, cập nhật ADR.
5. Không execute artifact lạ trước provenance validation; không đưa SDK/model vào Git.

Gate: bảng baseline có logs/raw metrics; chưa biết lợi ích thì chưa tuyên bố nhanh hơn.

### S03 — Qualcomm image processor (O03/O09/O10)

1. Implement neutral image_processor_port bằng adapter reuse converter/FastCV phù hợp.
2. Validate color matrix/range, stride/modifier, channel/layout/dtype, resize/ROI/padding,
   normalization và quantization; không nhận shape chỉ vì tổng bytes bằng nhau.
3. Discover actual factory/caps/properties tại target; không kế thừa vendor auto fallback.
4. CPU baseline là oracle trong phạm vi thuật toán đã định nghĩa, không tự lấy output
   của nó làm đúng khi interpolation/range khác specification.
5. Golden gồm crop biên, stride padding, odd dimensions, saturation, quantized/float,
   transform inverse, unsupported layout và lỗi sync; thống nhất tolerance theo dtype/op.

Gate: parity được phê duyệt; ghi engine thực tế và CPU/DDR cost. Processor vendor mới
chạy được cùng conformance suite mà không sửa core.

### S04 — Allocation và metadata ngoài hot path (O05/O07/O08)

1. Sau load và trước nhận frame: resolve/cache input/output specs, checked sizes,
   allocate bounded slots và reserve nested payloads theo package ceilings.
2. Thay local blobs/output vectors mỗi frame bằng explicit pool slots; trả slot sau
   last reader, không chỉ sau submit. Không grow khi pool đầy.
3. Trace capacity xuyên decode → tracking → features → take_result; thiết kế recycle
   API/arena owner, không chỉ swap ở một class rồi hủy candidate ở class khác.
4. Counters allocation/frame, peak/live bytes, pool occupancy, misses và fallback copies.
5. Test variable output count, errors, repeated take, stop và exhaustion; verify contents
   không stale và slot không reuse khi còn consumer.

Gate: target allocation budget được khai báo; phần đường chạy cam kết zero steady-state
allocation phải đo được zero sau warmup, kể cả negative paths trong phạm vi đó.

### S05 — Registered/native buffer execution (O04/O06/O13), sau S01/S04

1. Nối neutral buffer contract vào SDK adapter: allocation identity + generation,
   offset/stride/size, memory kind, registration/device/context scope.
2. Implement register/unregister và cache theo owner identity, không theo số FD đơn lẻ.
3. Document CPU acquire/release cache, accelerator readiness/completion, partial failure.
4. Plugin outputs: cân nhắc giữ GstSample phía adapter và xuất neutral lease/view nếu
   consumer bounded; giữ copy fallback nếu không chứng minh memory retention an toàn.
5. Test FD reuse, disconnect, late completion, pool full, load/unload, failure injection;
   kiểm actual DMA completion trên board, không dùng fake callback làm bằng chứng.

Gate: giảm copy/staging đo được hoặc chứng minh cần copy; không kéo vendor types lên core.

### S06 — Shared preprocessing (O07), sau S03–05

1. Tạo compatibility key tại activation: source generation, effective transform/ROI,
   dimensions/layout/dtype, channel order, normalization/quantization và sync contract.
2. Schedule một preprocessing job cho cùng frame/key; consumers chia sẻ read-only lease.
3. Không share state/input khi model khác semantics dù cùng kích thước. Cadence mỗi
   model vẫn độc lập; job chỉ tạo khi có consumer due/admitted.
4. Bounded cache theo frame/generation; late consumer, cancel và graph fault không làm
   mất owner của consumer khác. Dynamic ROI chỉ share khi ROI thực trùng nhau.

Gate: test hai consumer tương thích chỉ một preprocess; incompatible key tách; measure
CPU/DDR giảm và RAW hold time không vượt admission budget.

### S07 — Execution và fairness (O02/O13/O16), sau S01/S04/S05

1. Tách blocking SDK call khỏi control/source executor bằng bounded worker/execution port.
2. Không thread-per-model mặc định. Chọn concurrency theo SDK thread-safety/context và
   workload measurement; backend synchronous vẫn có thể chạy trong bounded worker.
3. Pending ticket giữ input/output owner và completion; stop không hủy tài nguyên bằng
   timeout. Không coi worker return là DMA completion nếu backend không bảo đảm.
4. Quy định queue capacity/age/drop/cancel-unsubmitted/priority/fairness; deadline dùng
   monotonic clock, expose queue age tách execution latency.
5. Tests slow graph không chặn healthy source/control; overload, late completion,
   stop/revoke, worker exception và backend stall. Không tăng concurrency khi memory
   hoặc HTP contention làm p95/p99 xấu hơn.

Gate: measured source fairness/control latency và bounded owner count dưới saturation.

### S08 — Model coverage qua capabilities (O09/O10/O11/O17)

1. Khai báo matrix single/multi-input, scalar/image/tensor, fixed/dynamic shapes,
   per-tensor/per-axis quantization, native output, batch, stateful sequence.
2. Chỉ triển khai từng class khi có requirement/conformance fixtures. Phần chưa hỗ trợ
   trả unsupported trước acquire; không suy luận từ model name hoặc tensor byte count.
3. Dynamic shape có envelope và pool profile; đổi shape bằng explicit generation/drain.
4. State/temporal join có source epoch, ordering/freshness/window, memory và reset policy.
5. Model update/LoRA nếu support: compatibility, authorization, atomic activation,
   rollback và no-active-reader rule; không bật chỉ vì tìm thấy SDK entrypoint.

Gate: thêm model thuộc class supported chỉ thêm package/decoder/config; không sửa pump.

### S09 — Hardware output và backpressure (O14/O15)

1. Ghép AI-owned preview surface → adapter transform/overlay → hardware encoder → FW ring.
2. Reuse installed Qualcomm components sau golden/ownership check; privacy scopes phải
   bind với pixels trước encode. Không sửa shared RAW để tiết kiệm một copy.
3. Ring tồn tại trước viewer demand; inference và preview cadence độc lập.
4. Live output bounded drop policy khác durable alarm retry/spool. Retain original
   event/config/policy revisions và dedup ID; không retag khi retry.
5. Test first/last viewer, slow sink, oversized AU, EOS/input/output completion, revoked
   scope, dead writer/reader và full queue; observe impact lên inference latency.

Gate: FW consumer đọc được output; sync/authorization pass; output outage không làm
unbounded retention hoặc âm thầm mất durable event.

### S10 — Admission, recovery và vận hành (O13/O16/O18)

1. Admission tổng CPU/accelerator/DDR/RSS/thermal, FW capture/record/encode và viewers.
2. Profile resource gắn runtime/image/package version; undeclared workload reject có lý do.
3. Define degraded modes explicit, observable; không tự hạ dtype/quality hoặc chuyển CPU.
4. Recovery: stop admission → drain/quarantine → BSP quiescence proof → new generation.
5. Supervision/readiness, config persistence, install/rollback và signed/immutable artifact
   gate theo FW01–FW09; device-free resolver không tự hoàn tất authentication.

Gate: stress/restart/disconnect/thermal soak trên workload đã chốt, không leak FD/pool/job.

### S11 — CI, tài liệu và portability conformance (O18/O19), xuyên suốt

1. eSDK matrix ghi options: neutral/reference, plugin adapter, direct QNN SDK, loaders,
   digest/service và optional FW SDK. Missing SDK/runner là not-run, không green evidence.
2. Structural checker + warnings + AST/literal review; sanitizer nếu eSDK target hỗ trợ.
3. Cùng port conformance tests chạy cho reference và adapters; kiểm neutral include/link
   không phụ thuộc vendor. Không cần implement vendor thứ hai chỉ để chứng minh naming.
4. Logs không chứa secrets/biometrics; artifact report pin exact config/revision và metric.
5. Update README/architecture/capability matrix sau mỗi bước; giữ historical evidence dated.

Gate: CI run thực, reviewer ký ownership/sync/ABI/entitlement; regression/performance
comparison tái lập được và không quảng cáo capability chưa qualified.

## 5. Ma trận kiểm thử không gắn tên model

| Trục | Các case cần có |
|---|---|
| Input | Single/multi-input; shapes bằng bytes nhưng khác dims; variable stride; malformed allocation; missing input |
| Tensor | Supported integer/float types; per-tensor/per-axis quant; mixed outputs; wrong order/name/layout; overflow; NaN/Inf where applicable |
| Geometry | Aspect ratios, crop/rotate/pad, odd dimensions, exact inverse mapping và clipping |
| Memory | CPU/native/imported modes; shared readers; delayed completion; FD reuse; budget exhaustion; failed registration |
| Scheduling | Mixed cadence/latency; output backpressure; saturated accelerator; fairness; dropped/gapped epochs |
| Lifecycle | Partial load/start failure; stop with pending result; unload/reload; active replacement; timeout/quarantine/recovery |
| Policy | Unsupported request; revoked output; stale revision; configured fallback only; no inferred authorization |
| Output | Partial feature failure; healthy output retained; stale batch; sink outage/retry; preview demand independent |
| Portability | Same neutral tests; vendor unavailable; alternative capabilities reject deterministically; no SDK types leak |

## 6. Metrics và quyết định tối ưu

Mỗi workload record chứa sources/profile/cadence, tensor classes/shapes/dtypes,
pipeline stages, viewers/recording, limits và board ambient/thermal conditions.
Đo warmup riêng; lưu sample count, duration và cách tính percentile.

- Capture→submit, queue wait, preprocess, inference, decode/track/feature, output và
  end-to-end p50/p95/p99; không chỉ average FPS.
- Accepted/completed/dropped frames theo reason, oldest job age và control response.
- Allocations/frame, live/peak RSS, pool high-water, copied bytes/frame, map/unmap,
  registration/cache misses, CPU/DDR và accelerator utilization nếu công cụ hỗ trợ.
- RAW/input/output hold time, outstanding jobs/readers, FD count và recovery count.
- Power/temperature/throttle, model warmup và concurrent FW workload.

Không đặt ngưỡng số tùy tiện trong source. Product/BSP chốt budget bằng versioned config
trước nghiệm thu. A/B cùng input/image/package, chạy lặp và đánh giá variance. Chỉ nhận
tối ưu khi correctness không đổi trong tolerance và metric mục tiêu cải thiện mà các
budget khác vẫn đạt. Giữ rollback path nếu native memory/concurrency gây regression.

## 7. Thứ tự ưu tiên và Definition of Done

Critical path: S01 → S02 → S03/S04 → S05 → S06/S07 → S08 theo capability cần thiết.
S09/S10 phát triển sau owner contracts ổn định; S11 chạy xuyên suốt. Không cần chờ mọi
class model ở S08 mới đo vertical slice, nhưng không gọi subset là universal support.

Mỗi bước: contract/ADR nếu boundary đổi → failing tests → implement → eSDK checks →
board checks khi hardware liên quan → evidence + docs → focused review/commit.
Owner: runtime cho scheduling/owners, Qualcomm cho SDK/memory/processor/encoder,
perception cho semantic parity, FW/BSP cho completion/recovery, lead/QA cho admission.

Base được coi tối ưu trong phạm vi declared capabilities khi:

1. Capability và policy phản ánh đúng implementation/qualification.
2. New supported model không thêm hardcoded branch trong core/runtime.
3. Qualcomm acceleration được dùng tại các stage đủ điều kiện; CPU paths có lý do đo được.
4. Memory/copy ledger và bounded execution có tests + device evidence, không early reuse.
5. Golden/replay, mixed workload, output policy, recovery và FW release gates đạt.
6. Portability giữ ở neutral contracts; vendor-specific tuning nằm trong adapter/config.
7. Hiệu năng có baseline và report tái lập, không suy ra từ source/tests hoặc tên backend.

Tài liệu này không đóng bất kỳ finding nào bằng kế hoạch. Khi thực hiện, cập nhật từng
Oxx/Sxx với commit, config, test/evidence, reviewer và limitations còn lại.

## 8. Tiến độ thực hiện

| Bước | Trạng thái | Thay đổi | Kiểm chứng | Còn lại |
|---|---|---|---|---|
| S01 | Đã triển khai (device-free) | `qneng_probe_capabilities` chỉ công bố synchronous một job + native output; `supports_async/shared_memory/artifact_update/multi_model_domain = false`; chỉ profile `balanced`; topology `0`. Thêm negative test vào `vqec_vision_inference_execution_test`; bảng capability inventory trong `qualcomm_execution_policy.md`. | default 53/53, expanded 71/71 dưới eSDK QEMU | Board qualification; async/shared/update/domain chỉ bật khi có lifecycle path + negative tests. O11 vẫn mở ở mức contract. |
| S04 | Một phần (device-free) | O05/O07: engine resolve + cache input/output spec tại `prepare` (validate dtype/shape ở đây, `get_tensors` trả cache, `execute` không dựng lại metadata và so khớp đầy đủ name/shape/dtype/quantization). Pump thêm `resolve_targets`, session gọi sau khi mọi graph running để bỏ lazy lookup trên hot path. | default 53/53, expanded 71/71 dưới eSDK QEMU | O05 (alloc output bytes mỗi call) và O08 (container churn) cần pool slot + instrumentation; phụ thuộc S05. Chưa có zero steady-state allocation claim. |
| S08 | Một phần (device-free) | O09/O10: multi-input/dynamic/stateful/update/batch bị reject `unsupported` tại activation trước acquire (test multi-input trong `multi_model_pump_tensor_test`); bảng model class coverage trong `model_catalog.md`. O10 đã xử lý ở S04. | default 53/53, expanded 71/71 | Chưa có conformance fixture/board cho từng class; dynamic/stateful/update chỉ bật khi có envelope + pool profile + epoch/reset policy |
| S11 | Một phần (device-free) | Thêm `esdk_configuration_matrix.md` (option/evidence + not-run rule); CI tách job neutral/expanded và gate QAIRT; cập nhật `implementation_status.md` và `qualcomm_adapter.md` để không mô tả plugin là implementation duy nhất; matrix được link từ `esdk_emulation.md`. | default 53/53, expanded 71/71 dưới eSDK QEMU | CI runner chưa được cấu hình; AST/literal review, sanitizer, neutral include/link dependency check và port-conformance dùng chung reference+adapter còn thiếu. |
| S02/S03/S05–S07/S09–S10 | Chưa thực hiện | | | Cần board/measurement hoặc phụ thuộc các bước trước. |

Quy ước trạng thái: `Đã triển khai (device-free)` nghĩa là source + test + tài liệu đã có
và chạy dưới eSDK QEMU; không phải board/BSP, model-accuracy, performance hay zero-copy
acceptance. Reviewer ký ownership/ABI/entitlement được ghi khi có.

