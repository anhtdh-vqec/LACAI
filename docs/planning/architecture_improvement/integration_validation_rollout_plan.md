# Kế hoạch integration, validation và rollout

Plan này ghép bốn plan còn lại thành release có thể kiểm chứng: đóng composition gaps,
chạy workload security/traffic, test storage/event/DSP, package và rollback. AI APP lead
là integration owner; BSP+FW và AI Model ký các gate thuộc họ.

- **Status:** planned — không phải release acceptance hiện tại.
- **Layer:** docs
- **Source:** [review checklist](../../development/review_checklist.md),
  [face production validation](../../testing/face_recognition_production_validation.md), `n/a`.

## Trách nhiệm

- AI APP lead lập release profile, điều phối test matrix, giữ evidence ledger và quyết định
  feature status (`planned`, `source-delivered`, `logic-tested`, `board-smoke`, `accepted`).
- BSP+FW cấp target/image/released-FW harness, board resources, media/evidence/time facts,
  deploy/supervision/quota, DMA/thermal traces và ký compatibility.
- AI Model cấp immutable artifacts, catalog, dataset/golden/quality report, model-version
  migration và ký M0–M6; không coi mAP/demo là end-to-end acceptance.

## 1. Dependency graph và gates

```text
P1 contracts/team
   ├── P2 metadata/query ──┐
   ├── P3 event/evidence ──┼── P5 integration/release
   └── P4 DSP/compute ─────┘
```

P2/P3/P4 có thể phát triển song song sau contract fixture, nhưng P5 chỉ công bố build khi
schema/ABI/version, workload và ownership gates của tất cả producer/consumer đã ký.

## 2. Đóng composition trước khi benchmark

1. Tách production path khỏi harness; không đặt entitlement/resource/output grants `true`
   trong production (`service_main.cpp` A01).
2. Đăng ký processor theo feature/schema thật; không tạo `reference_zone_feature` cho mọi
   contract (`production_platform.cpp` A02).
3. Đưa output qua authorization/freshness/correlation/demand path; không bypass bằng
   direct renderer (`qtiv_renderer.cpp` A06).
4. Bind graph/processor theo source-model slot; test cùng model trên hai source, không bỏ
   duplicate graph check để che lỗi (`production_platform.cpp` A04).
5. Tích hợp artifact manifest/digest/allowed root đến lần load thật; hash không phải signature.
6. Giữ cascade/stop drain completion; không report running hoặc release frame khi SDK chưa done.
7. Ghi coverage/attribute age và source epoch; không merge latest overlay khác frame.

Đây là blocking hardening; benchmark CPU trước khi đóng các mục này chỉ là diagnostic.

## 3. Release profiles

Mỗi profile là file manifest bất biến chứa board/FW/SDK, source count/profile/FPS/cadence,
model hashes, feature/usecase grants, viewer/evidence demand, gallery size, storage/Kafka
policy, thermal/governor, test duration và acceptance metrics. Không hardcode profile vào code.

| Profile | Scope | Mục đích |
|---|---|---|
| R0 logic | reference/fake + fixtures | contract/schema/query/event/ownership; eSDK/QEMU logic |
| R1 security core | person + tracking + intrusion/count/heatmap + one attribute | end-to-end local metadata/query + event mock |
| R2 security extended | PPE/luggage/abandoned/blacklist/attendance/action/VLM theo kit | dependency/quality/privacy/resource gates |
| R3 traffic core | vehicle class/count/lane/direction/ANPR | common trajectory/passage/plate query |
| R4 traffic measured | speed/queue/signal/red-light chỉ khi C04 calibration/signal valid | measurement/violation candidate gate |
| R5 mixed stress | multi-source + storage/query + evidence + Kafka offline + DSP | sustained resource/admission/recovery |

Không gọi R2/R4 accepted nếu model kit/quality hoặc authoritative input chưa có. R3 có thể
chạy ANPR trước speed; mốc người dùng ghi `15/10` phải được chốt thành năm/profile/owner.

## 4. Test matrix

### 4.1. Functional and semantic

- 18 usecase positive/negative/unknown/not-observable/unsupported/expired fixtures.
- Q01–Q30: passage-attribution, track timeline/gap, relations, events, attendance,
  plate exact/fuzzy, aggregate denominator, VLM provenance, geometry backfill, purge/recompute.
- Track ID switch/reboot/source epoch; scene/zone/lane/calibration revision; clock jump/skew.
- Person/vehicle/scene common schema; no hardcoded security-only field assumptions.

### 4.2. Fault and ownership

- RAW FD reuse, delayed SDK completion, DSP/HTP reset, source disconnect/profile change,
  queue/pool full, model fault, stop/restart and concurrent generation replacement.
- SQLite crash before/after commit, WAL/checkpoint, file rename/manifest publish, compaction,
  disk full, purge/revoke, Kafka offline/ambiguous ACK, UDS wrong peer/duplicate/reorder.
- FW duplicate Start/End, missing pre-roll, media timeout/partial, evidence ready after restart.
- Assert no early buffer ACK/reuse; no event/aggregate duplicate; no unauthorized attribute.

### 4.3. Performance and resource

Per profile report FPS, drops/reasons, per-stage p50/p95/p99, capture→result/evidence latency,
CPU/thread/system, copied bytes, allocation/FD/pool high-water, RSS/PSS, DDR/HTP/cDSP,
temperature/power, storage bytes/WAL/outbox/archive, query scan/RSS/temp disk and Kafka backlog.
Run cold/warm, 1/2/maximum source, 0/1/multiple viewers, evidence on/off, storage/query mixed,
and sustained thermal soak. Routing latency alone không phải recognition/evidence latency.

## 5. CI, board và package

1. Structural/docs/source-layout checks mỗi PR; naming/contract/ownership tests theo target.
2. Neutral eSDK cross-build + QEMU CTest; host sanitizer là evidence riêng, không thay target.
3. Qualcomm expanded eSDK build chỉ khi QAIRT/SDK supplied; DSP Hexagon build pin riêng,
   không tải private SDK/model hay fallback host.
4. Board `.98` workspace `/opt/lacai`: stage manifest, binary, model kit, config, FW harness,
   run test, collect immutable logs/metrics; `.99`/`.48` không được truy cập.
5. Package contains executable/config/schema/adapter dependencies/manifest/SBOM, no secrets,
   model private library or biometric fixtures. Install/reboot/upgrade/rollback checked.
6. CI skipped runner variables are not pass. Record commit, image, toolchain, profile,
   configuration, temperature and test command for each claim.

## 6. Acceptance ladder

| Gate | Owner sign-off | Điều kiện |
|---|---|---|
| G0 contract | AI APP + BSP+FW + AI Model | C01–C10/version/fixtures/owner; no open breaking decision |
| G1 logic | AI APP | neutral tests, Q01–Q30 fixtures, fault semantics, eSDK/QEMU |
| G2 model | AI Model + AI APP | M0–M5 golden/replay/quality, version/provenance |
| G3 target | BSP+FW + AI APP | Qualcomm/DSP completion/cache/lease, board profile smoke |
| G4 integrated | all three | released-FW media/evidence, storage/query, authorization, mixed load |
| G5 sustained | all three | soak/thermal/restart/offline/resource envelope with agreed SLO |
| G6 release | AI APP lead + release owner | package/rollback/security/privacy, acceptance evidence ledger |

`accepted` chỉ dùng khi G6 có owner review và board evidence cho profile cụ thể. Một green
unit test, matching digest, model loaded, broker ACK hoặc “no crash” không đóng gate.

## 7. Task thực hiện

| Task | Owner | Đầu ra | Blocker/exit |
|---|---|---|---|
| I01 | AI APP lead | release manifest/profile templates + evidence ledger | G0 decisions |
| I02 | AI APP | composition hardening A01/A02/A04/A06/A08 | negative tests + production path |
| I03 | Cả ba | R0/R1 fixtures and test harness | C01–C09 |
| I04 | AI APP+BSP+FW | UDS/evidence/storage/query integrated mock | P2/P3 complete |
| I05 | AI APP+AI Model | detector/DSP vertical and quality replay | P4 C02/C03 |
| I06 | BSP+FW | `.98` board/released-FW test run and resource traces | G2 package/target |
| I07 | AI APP | mixed security/traffic/perf/thermal report | I01/I06 |
| I08 | all three | fix/retest, rollback, sign-off and status docs | no open P0/G4 failure |

## 8. Tiêu chí nghiệm thu

- [ ] Mỗi release claim liên kết profile, commit, toolchain, config, test command và owner;
  không dùng số lịch sử khác workload làm bằng chứng.
- [ ] Production composition không fixture-authorize, không reference sink/factory sai,
  không bỏ source slot, không bypass output policy.
- [ ] Storage/query, UDS/evidence và DSP test pass cả crash/retry/revoke/epoch/ownership;
  result có coverage/partial/unsupported semantics.
- [ ] Person và vehicle/traffic chạy common core; speed/red-light chỉ được mở với calibration,
  signal và time evidence hợp lệ.
- [ ] ARM CPU/DDR/power/thermal được báo cùng FPS/accuracy/latency; DSP saving có A/B report.
- [ ] Released-FW RTSP/UI/evidence conformance, one-writer ring và media actual interval pass.
- [ ] Install/reboot/upgrade/rollback/secret/SBOM/quota/retention/purge audit pass.
- [ ] G6 sign-off đủ ba team; thiếu owner/fixture/evidence là `planned` hoặc `blocked`,
  không ghi `accepted`.

## Giới hạn và công việc tiếp theo

- Plan này không tự cấp board, released-FW, Hexagon SDK hay model license.
- Chưa có SLO/cardinality cuối; AI APP lead phải chốt release profile trước I03.
- Sau G0, thực hiện I01–I03; không chạy benchmark “production” trên host compiler hoặc
  workload không reproducible.

## See also

- [Architecture improvement master plan](README.md)
- [Contract and team scope plan](contract_and_team_scope.md)
- [Metadata/query plan](metadata_query_plan.md)
- [Event/evidence transport plan](event_evidence_transport_plan.md)
- [DSP optimization plan](dsp_multiplatform_optimization_plan.md)
- [Review checklist](../../development/review_checklist.md)
