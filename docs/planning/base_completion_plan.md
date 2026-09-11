# Kế hoạch hoàn thiện base — 2026-09-11

Baseline và findings: [audit](../development/base_audit.md). Kế hoạch này ưu tiên sửa
đường lỗi trước mở feature, bổ sung cho FW01–FW09 và delivery plan; không thay thế các
release gate. Mỗi bước là một PR/commit có source, contract, test, evidence. Không đóng
bước bằng tài liệu hoặc test fake khi gate yêu cầu device. Owner dưới đây là vai trò đề
xuất, chưa phải xác nhận phân công. Không ước lượng deadline khi thiếu model/workload.

| Bước / phụ thuộc | Công việc cụ thể / đầu ra | Owner | Gate bắt buộc |
|---|---|---|---|
| B01 / bắt đầu | Thêm failing regression A01/A05: mixed feature failure, stop with pending result; sửa executor giữ partial report và drain consume/discard | Runtime | Healthy events lấy được đúng một lần; error masks rõ; stopped đạt khi owner thực drain; lỗi gốc không mất |
| B02 / B01 | Output envelope giữ source/model/config/catalog/policy revision; dispatch nhận success mask, không dùng revision mới thay revision cũ; phân biệt denied/transport error | Runtime + output | Revoke/regrant giữa produce/take/dispatch/retry bị chặn; stale batch không publish; partial sink acceptance không duplicate |
| B03 / B01 | Composition binding descriptor thay raw pointer metadata-free; đối chiếu dependency và source/model revision; global stage/fanout uniqueness | Runtime | Sai slot/cross-source alias/revision mismatch reject trước create/start; healthy multisource vẫn pass |
| B04 / B03 | Freeze manager records khi có borrow; controlled replacement/reconcile generation; guard replace active bundle | Runtime + lead | Reconfigure/revoke/pending/failed activation không UAF; bundle cũ giữ nguyên khi reject; review ownership |
| B05 / song song B01–04 | Literal inventory source/header/tools; chia protocol/vendor/schema constant và deployment config; validate defaults/missing/range | Lead + module owners | No silent fallback; threshold/path/timeout/grant không chôn trong production; semantic review theo convention §3 |
| B06 / B03–05 | Tách reference harness và production composition mode; package registry fail closed; real monotonic clock, wakeup/bounded work budget, health/exit/signal handling | Runtime | Fixture không được nhận model contract thật ở production; heterogeneous sources test; startup/stop/error exit deterministic |
| B07 / B04–06 | Trusted deployment/model/feature/artifact resolver; owned immutable config snapshot; authenticity + allowed roots + TOCTOU policy; construct Camera/Qualcomm retention owners | Runtime + FW/BSP | Invalid/mutated artifact reject trước SDK load; borrowed objects sống hết drain; partial-start rollback |
| B08 / B07 | Một approved model kit: tensor schema/order/dtype/quantization/preprocess, decoder registry, tracker factory, dataset/golden, digest/version | Model + perception | Border/letterbox/channel/range/stride golden; unsupported tensors reject; source identity bảo toàn; không hardcode model vào core |
| B09 / B08 | Một single-model usecase package: config schema, processor factory, events, scope, epoch/gap rules | Usecase + output | Replay empty/noisy/gap/reset/quality; denied/resource-limited state; feature không phụ thuộc SDK |
| B10 / B07–09 | Board vertical slice Camera RAW → Qualcomm plugins → tensors → package → events; pin image/plugins/model/runtime | Qualcomm + FW/BSP | Actual frames/model; negotiated caps; acquire/cache/input completion trace; stop/disconnect/stall/FD reuse; đối chiếu vendor sample |
| B11 / B10 baseline | Pool/arena toàn chain; numeric IDs hot path; output buffers trả về pool; cache tensor metadata; workload admission + profiler | Runtime + Qualcomm | Allocation/copy counts sau warmup, p50/p95/p99/RSS/DDR/CPU/thermal trước-sau cùng input; correctness unchanged; không packed/native dtype giả |
| B12 / B07,B10 | Private preview surfaces, Qualcomm renderer/transform/encoder adapter; per-job scope/context; ring create/open/unique writer; independent preview cadence | Qualcomm + output/FW | AI không sửa RAW chung; first/last viewer, no viewer, slow inference, SPS/PPS/keyframe/PTS, duplicate completion và full drain |
| B13 / B02,B09 | FW event sink + bounded queue/spool, event_id dedup, TTL/quota/retry/backoff, captured policy per event | Output + FW | Sink outage/restart/ambiguous ACK/revoke during retry; counters phân biệt discard/denied/error; evidence authorization riêng |
| B14 / B06,B10,B12 | FW control compatibility, config persistence, source restart/backoff/epoch, BSP recovery handshake; packaging/IPK/supervision/readiness/update/rollback | Runtime + FW/BSP | FW01–FW09 run; active readers không release khi timeout; quiescence proof trước reuse; version-coherent rollback |
| B15 / B01 onward; cuối B14 | CI full eSDK matrix, warnings/AST/literal checks, fault/replay/sanitizer where supported, artifact provenance, board soak/performance | Lead + QA | Required jobs thực chạy (không skipped=pass); logs pinned revision/config; agreed workload/KPI và owner signoff |

## Cách triển khai mỗi bước

1. Đọc contract + code liên quan; tạo test chứng minh lỗi/requirement, không mirror code.
2. Nếu đổi external boundary: cập nhật schema/ADR trước source và giữ migration rõ.
3. Implement nhỏ theo ownership graph; không nhân thêm service/process/plugin stack.
4. Build và tests qua eSDK; board khi DMA/SDK/FW là đối tượng kiểm tra. Ghi rõ case
   chưa chạy và vì sao. ABI/ownership/sync/entitlement cần lead + owner review.
5. Rà diff/literal/dependency, cập nhật inventory và evidence; focused commit theo chỉ
   đạo phiên làm việc. Không ghi secrets/model/private SDK vào Git.

## Definition of done

G1 hoàn tất khi B01–B06 pass, API tích hợp có negative tests và owner review; B08–B09
có thể phát triển song song nhưng không dùng feature mới để che lỗi base. G2 cần B07–B11
với model/usecase thật và evidence board. G3 cần B12–B15 và FW acceptance. Một model
reference đủ xác nhận seam; không tự suy ra 13 usecase hoặc mọi platform đã support.

Temporal joins/ROI/alternate backends chỉ thêm theo requirements đã chọn, sau G2 và
benchmark. Thiếu capability phải trả unsupported, không silently skip trong production.
Chốt workload: số nguồn/profile/rational FPS, models/cadence, viewers/recording, memory,
latency/age/drop/thermal budget và dataset trước tối ưu. Nếu không đạt, giảm workload
qua admission có lý do hoặc tối ưu bottleneck đo được; không tăng trần queue để che lag.
