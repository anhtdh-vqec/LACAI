# Kế hoạch thực thi 12 tuần — team 3–4 người

Tính từ kick-off, không tự coi ngày tài liệu là ngày bắt đầu dự án.
Đầu ra: release v1 trên một Qualcomm target, các feature đạt gate và workload
được công bố; không đồng nghĩa production cả 13 bài trên bốn SoC.
Tất cả 13 bài nằm trong backlog; đủ model qualified đúng hạn là dependency
bắt buộc để đạt mục tiêu nghiệm thu đủ 13 bài trong quý này.

## Người và trách nhiệm

| Vai trò | Primary | Backup / review |
|---|---|---|
| A — lead | architecture/contracts, runtime feature graph/admission, entitlement | D cho control; B cho lifecycle |
| B — platform | camera, buffers/sync, FastCV/QNN, profiling | A + D |
| C — perception | model integration/golden, decode/tracker/features | A + D |
| D — integration/quality | build/CI/replay/package, FW outputs, test automation + feature | B/C theo module |

Nếu 3 người: A nhận FW control/package planning, B nhận cross-build/board,
C nhận replay/golden; unit/CI chia theo module. Cắt second-platform PoC,
preview/overlay và traffic implementation; không cắt sync/security tests.
Mỗi critical module có ít nhất hai người hiểu, tránh single owner DMA knowledge.

## P0 — tuần 1–2: đóng nền

- A: review/ký Camera/Model/FW contracts; neutral types + lifecycle states; ADR.
- B: BSP kit inventory, known-good SDK sample, 4K allocation/import exploration.
- C: nhận person model kit, golden format và baseline dataset cho 13 feature.
- D: target-scoped CMake/host-cross profiles, format/static checks, naming registry
  checker, replay harness + fake backend + CI setup.
- Deliver: host skeleton build, fake frame -> output, agreed naming/ownership,
  SDK/model dependencies có owner/date, source tree không vendor leakage.
- Gate: chưa ký memory sync/RAW profile thì đánh dấu integration risk, không
  tuyên bố board path ready.

## P1 — tuần 3–4: Qualcomm vertical slice

- B: camera acquire/frame/release, bounded leases/pools; FastCV image processor,
  QNN load/execute/teardown; synchronous correctness baseline.
- C: person decoder, preprocess/tensor/output golden; actual metadata validation.
- A: source epochs, stop/drain/error routing, stage scheduling.
- D: board run automation/metrics, install test harness, FW coexistence test.
- Gate: RAW4K -> person detections, đúng golden/coordinates, camera không bị giữ
  vô hạn, disconnect/timeout không early release, live/record cùng chạy.
- Không optimize zero-copy registration trước khi có correctness baseline.

## P2 — tuần 5–6: shared perception + wave 1

- C: tracker, intrusion/counting/heatmap/gathering rules.
- A: compatible dependency sharing, feature state/config revisions/admission.
- B: measured pool/copy/ROI optimization, bounded worker sizing.
- D: event delivery/replay regression, restart/epoch/counter tests.
- Gate: W1 demo trên board; tắt một feature không reset tracker của feature khác;
  counting không tăng giả khi restart; latency/memory reports cho W1.

## P3 — tuần 7–8: alpha + wave 2

- A: entitlement enforcement desired/effective, revoke during in-flight jobs.
- C/D: PPE, luggage tracking, abandoned-object relationships; attribute schema.
- B: QNN optimized memory path nếu SDK hỗ trợ và benchmark đáng giá;
  thiếu support giữ explicit baseline, không fake zero-copy.
- D/A: IPK split, FW supervisor/config/event/evidence, coherent update/rollback.
- Gate: alpha đầu-cuối có purchased subsets, denial reasons, verified install,
  evidence correlation và feature quality reports.

## P4 — tuần 9–10: wave 3 + beta

- C: action/smoking/weapons temporal integration nếu kits đã đạt M0–M4;
  FR/blacklist/retrieval phối hợp gallery/search FW service.
- A/B: mixed workload resource policies, quality-aware crop scheduling, faults.
- D: integration matrix, permissions, config/model ABI incompatibility tests.
- Second-platform contract PoC chỉ khi core đúng tiến độ và có SDK.
- Gate: beta, feature freeze cuối tuần10; qualified matrix đủ 13 hoặc ghi rõ
  feature blocked + responsible dependency, không giấu bằng trạng thái implemented.

## P5 — tuần 11–12: hardening/release

Fault injection camera restart/profile change, backend stall/recovery, license
revoke, output offline/disk quota, repeated update/rollback, sanitizers where
supported, agreed 24–72h board soak và coexist FW services.
Không nhận model/feature lớn mới sau freeze nếu ảnh hưởng stabilization.
Release manifest pin artifact/SDK/image; publish supported workloads/KPIs,
known limitations, operator runbook, rollback set và signed acceptance reports.

## Critical external dates

| Deadline | Dependency | Owner |
|---|---|---|
| end W1 | board, image, SDK/sysroot + known-good sample | BSP |
| end W2 | camera memory/control contract + person full model kit | FW/BSP + Model |
| W3 | actual RAW4K input + config/event/license schema | FW |
| before W7 | PPE/object/face/attribute qualified kits | Model |
| before W9 | temporal smoking/action/weapon kits; gallery/search/evidence | Model + FW |
| end W10 | freeze image/SDK/model versions | all teams |

Late dependency -> replan feature release, không vay toàn bộ 2 tuần hardening.
Lead weekly demo + dependency review; cross-team integration twice/week khi bringup.
Task không dùng phần trăm mơ hồ: planned/implementing/integrated/qualified/released.

## Learning syllabus (gắn deliverable)

| Thời gian | Nội dung | Bài thực hành nghiệm thu |
|---|---|---|
| W1 | RAII/move ownership, status, C++17, CMake targets | move-only FD/lease + partial-init cleanup tests |
| W1–2 | NV12/stride/ROI/color, quant/layout | padded frame -> expected tensor without OpenCV |
| W2–3 | DMA-BUF/cache/fence/lifetime | trace prove no early ACK under delayed completion |
| W3–4 | QNN graph/tensor metadata, FastCV modes | load known model + exact golden comparisons |
| W5–6 | MOT/geometry/temporal state | replay ID switches, crossing hysteresis, missing frames |
| W7–8 | entitlements/ABI/IPK/update | revoke + compatible update + rollback tests |
| W9–10 | profiling/mixed-load/thermal | p95 latency + copies + memory under FW coexistence |
| W11–12 | incident/recovery/release | fault drill and operator runbook |

Mỗi tuần 0.5 ngày learning/pair review, không học tách khỏi task.
Critical sync exercise B + A/D cùng review.

## Definition of done

Task: convention + ownership/API docs + unit/error paths + review.
Hardware task: board evidence thêm vào; host green chưa đủ.
Feature: kit/version/dataset/KPIs, golden+replay, lifecycle/config/license/output,
mixed workload budget + known limitations, owner sign-off.
Release: no unresolved critical buffer/security/ABI faults; supported workload
matrix, clean installation and rollback, measured soak report, operations docs.
