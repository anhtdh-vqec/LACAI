# Audit base LACAI — 2026-09-11

Baseline source: `24ab721`; worktree sạch trước audit. Đây là review tĩnh các boundary
và chạy regression bằng eSDK, không phải formal proof hoặc board acceptance. Các
finding dưới đây chưa được sửa bởi commit tài liệu này. Mục tiêu là base tích hợp
model/usecase qua package contract, không phải hoàn thành 13 thuật toán trước khi tích hợp.

## Kết luận và tiêu chí hoàn chỉnh

Có thể bắt đầu một model/usecase single-model. Chưa đủ production: service đang dùng
reference owners/fixture packages, chưa nối Camera/Qualcomm thật, output preview còn
thiếu renderer/encoder/ring lifecycle. Không có khái niệm tối ưu tuyệt đối độc lập
workload. Chọn tối ưu theo correctness trước, rồi latency/memory/DDR/power đo trên cùng
board/image/model/input; không thay backend đã tối ưu chỉ vì microbenchmark.

Ba gate độc lập: G1 base logic (composition, isolation, config, integration seam),
G2 vertical slice device (RAW thật → inference → decode/track/feature → output),
G3 FW release (control, package, recovery, workload/soak). Test xanh chỉ chứng minh
các case được chạy, không tự đóng G1–G3.

## Điểm mạnh cần giữ

- Core/ports trung lập; GStreamer/QNN ở Qualcomm adapter. ADR 0002 tái sử dụng plugin.
- Nhận một frame rồi fan-out model theo cadence; có source epoch, ticket/PTS và drain.
- Registry/catalog, decoder/tracker/stage, feature manager và output gate đã có seam rõ.
- Runtime executor, reference source/graph/sink và service harness chạy được; CI workflow
  và structural checker tồn tại. Đây là tiến bộ thực, không còn chỉ skeleton.
- Encoder ledger phân biệt input completion/output completion; timeout không giải phóng DMA.

## Findings theo ưu tiên

| ID / mức | Bằng chứng source | Hạn chế / tác động | Điều kiện đóng |
|---|---|---|---|
| A01 / P0 | `src/app/vqec_vision_runtime_executor.cpp`, step trả về ngay khi pipeline lỗi | Fan-out đã chạy feature khỏe nhưng executor chưa set pending; mất tracked/events thành công khi một peer lỗi | Retain report/output hợp lệ theo mask; test một thành công + một lỗi + take-once + drain |
| A02 / P0 | `runtime_composition_factory.cpp` lấy fanouts theo source slot; `multi_model_feature_pipeline.cpp` chỉ kiểm trùng trong một pipeline | Chưa chứng minh source/model/catalog identity và unique stage/fanout toàn bundle; caller có thể chia sẻ temporal state sai nguồn | Binding descriptor pin revision/source/model; reject mismatch/alias xuyên nguồn, kể cả hai fanout chứa cùng stage |
| A03 / P0 | `feature_activation_manager.cpp` reconcile thay records; fanout giữ stage pointer | Reconcile trong lúc consumer còn sống có thể làm dangling borrow; serialized call không tự bảo đảm lifetime | Freeze activation snapshot hoặc generation owner; rebuild/drain trước thay; test revoke/reconfigure khi result pending |
| A04 / P0 | `runtime_executor.cpp` dispatch_events lấy revision hiện tại từ gate | Không giữ revision tại thời điểm result/event được tạo/queued như contract dispatch yêu cầu; kết quả cũ có thể được retag dưới policy mới | Output envelope giữ original revision/source/model/config và success mask; reject stale revision, không relabel retry |
| A05 / P0 | `service_main.cpp` drain loop không take_result; `runtime_executor.cpp` step bị chặn khi has_pending | Nếu kết quả xuất hiện lúc stop, loop có thể không tiến tới stopped; harness hết vòng rồi thoát. Không dùng mô hình này cho DMA thật | Drain consume/discard có chủ đích, latch lỗi đầu tiên, giữ owner khi chưa quiescent; test stop đúng lúc có output |
| A06 / P1 | `runtime_composition_factory.cpp` commit candidate bằng move vào _bundle | Precondition stopped là tài liệu; replacement có thể hủy bundle đang active nếu caller đưa owner mới | Guard state hoặc API replacement transaction; test replace active/recovery bị reject |
| A07 / P1 | `service_main.cpp` reference owners, fixture registration, grants=true, now_ns cộng bước | Service là harness, không real executor clock/readiness/policy; source lỗi rồi drain thành công có thể không phản ánh lỗi đầu tiên trong exit status | Production mode rõ, không fixture fallback; monotonic clock/event wait, status/exit/readiness và policy thật |
| A08 / P1 | `model_decode_stage.cpp`, `tracking_stage.cpp`, `feature_stage.cpp` dùng scratch/swap; result_stage/router tạo candidate local; executor move arrays | Reuse cục bộ không chứng minh bounded allocation toàn hot path; clear vector hủy nested strings/vectors và caller mới không trả capacity | Pre-sized output storage/arena xuyên chain + allocation counters sau warmup; benchmark failure/variable-size payload |
| A09 / P1 | `tensor_output.cpp` reserve/resize/memcpy từng output | FLOAT32 packed copy hiện tại; không native mixed dtype, không zero-copy output | Capability reject rõ; đo copies/DDR trước; chỉ mở native dtype theo model cần + golden |
| A10 / P1 | `plugin_graph.cpp` engine fcv, single-job admission, retention slots; research Q01–Q18 | Nhiều graph app không chứng minh QNN multi-graph/share context; plugin factory hiện diện không chứng minh DMA completion/HTP performance | Pin installed binary/image/model, compare vendor sample, trace retain/release và load/start/stop/fault trên board |
| A11 / P1 | `service_main.cpp` paths/timeouts/grants; `plugin_graph.cpp` retention size/sentinel | Literal fixed contract và deployment policy đang lẫn nhau; đổi sang constant đơn thuần không giải quyết hardcode | Inventory theo domain; schema/config policy + semantic contract constants + negative tests |
| A12 / P1 | artifact_digest/model loaders và runtime activation nhận resolved paths | SHA-256/schema hợp lệ không là authenticity/TOCTOU-safe path; chưa có trusted platform owner factory | Immutable resolver/package provenance, allowed roots, reject substitution và mismatch trước load |
| A13 / P1 | `outputs/`, `adapters/fw_output/`; preview/encoder ports | Có helper nhưng thiếu renderer/encoder thực, per-job scopes/context, ring open/single writer và event transport durable | Full preview demand/PTS/scope/stop + FW reader test; events có retry/dedup/quota theo contract |
| A14 / P2 | feature catalog temporal_join; harness skip non-single-model | Không join đồng thời/freshness, ROI scheduling hoặc package thực; skip không là supported | Effective unsupported có lý do; chỉ xây join khi model/usecase cần và có bounded policy/golden |
| A15 / P1 | `.github/workflows/ci.yml` structural + conditional eSDK; docs/status | CI default chưa bật full service/adapter matrix; workflow tồn tại không chứng minh runner chạy. Status còn phủ định service/CI/JSON fallback đã có | Matrix eSDK rõ, lưu logs/config/revision; docs inventory phân biệt lịch sử và hiện tại |

A01–A06 là suy luận từ control flow cần regression tái hiện trước khi sửa. Các khả năng
unsafe không đồng nghĩa đã có sự cố trên device. Full review đồng bộ/ABI vẫn cần lead
và platform owner; audit này không tự ký thay họ.

## Ràng buộc tối ưu kiến trúc

Không tạo thêm abstraction nếu chỉ chuyển tên giữa các lớp. Đặt composition tại app,
policy tại runtime, algorithm tại package và SDK tại adapter. Không thêm thread per model
trước khi đo SDK concurrency/global FCV behavior. Giữ bounded fairness và admission toàn
board gồm FW encode/record; trần 16 sources × 16 models không phải capacity sản phẩm.

Reuse plugin Qualcomm cho preprocess/inference/transform/overlay/encode khi compatible.
Chỉ viết adapter và semantic translation; với postprocess/tracker, kiểm dependency/license
và golden trước reuse. Không đưa OpenCV vào production qua transitive dependency.

## Bằng chứng và giới hạn

Xem [validation snapshot](../testing/base_audit_2026_09_11.md) cho configuration/result
thực chạy. 65/65 board tests ngày 2026-09-10 là lịch sử, không được gán cho HEAD mới.
Không chạy model thật, fault DMA hoặc benchmark trong audit này. Các con số tối ưu
p95 latency, RSS, hold time, dropped frames và thermal phải được chốt theo workload.

Kế hoạch thực thi: [base completion plan](../planning/base_completion_plan.md).
