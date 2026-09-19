# Kế hoạch event IPC và evidence với FW

Plan này xây đường event đáng tin cậy từ AI APP sang FW để tạo pre/post evidence, tách khỏi
D-Bus control/config và khỏi frame loop. Mặc định dùng UDS `SOCK_SEQPACKET` + durable outbox;
SHM chỉ là tối ưu live metadata sau khi đo.

- **Status:** planned — chưa có event transport/evidence client production.
- **Layer:** docs
- **Source:** [feature event dispatch](../../architecture/feature_event_dispatch.md),
  [FW release compatibility](../../contracts/fw_release_compatibility.md), `n/a` cho giao thức mới.

## Trách nhiệm

- AI APP tạo semantic event, policy authorization, request ID, outbox, retry/reconcile và
  link metadata; không encode/ghi clip trong transport.
- BSP+FW nhận command đã version, giữ evidence ring/media, dedup durable inbox, tạo clip,
  trả receipt actual interval/gaps và sở hữu RTSP/storage media.
- AI Model chỉ cung cấp event/quality output semantics; không gọi FW trực tiếp hoặc tự cấp
  quyền evidence.

## 1. Control plane và data plane

| Plane | Cơ chế | Dữ liệu | Quy tắc |
|---|---|---|---|
| Control | D-Bus hiện hành | desired/config/status/capability | low-rate, revision/CAS, accepted khác running |
| Event | UDS `SOCK_SEQPACKET` | event/evidence intent + receipt | message boundary, bounded parser, ACK levels, dedup |
| Live annotation (optional) | UDS batch hoặc SHM ring | latest overlay/metadata | lossy/latest-wins, không thay durable alarm |
| Media | FW internal ring/storage | encoded video/clip/snapshot | FW owner, one writer, actual media receipt |

D-Bus không bị loại bỏ tuyệt đối; không phù hợp để chở mọi bbox/event hoặc làm durable
queue. UDS peer permission/`SO_PEERCRED` và configured unique peer name là authentication
input nhưng vẫn cần authorization trong message.

## 2. Wire envelope và state

### 2.1. Envelope

`protocol_major/minor`, `schema_id/version`, `message_kind`, `payload_bytes`, `event_id`,
`event_revision`, `request_id`, source/device/boot/epoch, capture event time + UTC mapping/
uncertainty, producer/model/feature/config/policy/scene revisions, priority/deadline,
pre/post duration, evidence source/profile ref và authorized field/media scope.

Tất cả integer có unit/endianness; max message/payload và nested depth được validate trước
allocate. Không gửi native C++ layout, pointer, `std::string`, STL container hoặc QNN/Gst type.
Serializer (protobuf/FlatBuffers/compact C schema) là quyết định C07 sau benchmark; wire
schema phải có unknown-field và forward/backward policy.

### 2.2. State và ACK

```text
event:    started -> updated* -> ended / interrupted
delivery: queued -> durable_local -> fw_accepted -> terminal
evidence: requested -> recording -> finalizing -> ready / partial / failed
```

`durable_local`, broker delivery, `fw_accepted`, `recording`, `ready` là các mức khác nhau.
FW chỉ ACK `fw_accepted` sau khi dedup durable inbox/job đã commit theo contract. Lost ACK
retry cùng `request_id`/revision không tạo clip thứ hai. `ready` phải có media ID, actual
interval, gaps/partial reason và availability/retention.

## 3. AI APP implementation

1. Feature stage validates event fields, source epoch, freshness, output scope và evidence
   intent; dispatcher kiểm quyền ngay trước khi enqueue.
2. Copy metadata bounded vào event outbox writer; không giữ frame/tensor/embedding để chờ I/O.
3. Một transaction ghi canonical event/read projection và outbox command. Chỉ công bố
   `durable_local` sau commit/sync policy đã ký; queue RAM không phải receipt.
4. Event worker có priority lanes: evidence alarm trước bulk metadata, retry backoff/jitter,
   max attempts/bytes, stop drain/quarantine và counter dropped/retry/ambiguous.
5. Reconciler tra request ID khi ACK ambiguous/restart; cập nhật `evidence_reference` theo
   receipt, không tạo lại event/clip vô hạn.
6. Revocation/expiry kiểm lại trước retry; event queued không được gửi dưới policy cũ.

Không để SQLite/Parquet/Kafka query/compaction chạy trong thread inference. Bounded queue
đầy phải trả `resource_exhausted` hoặc degraded reason; alarm đã durable không bị silent
drop. Live annotation có thể drop-oldest nhưng không dùng rule đó cho evidence command.

## 4. FW implementation request

1. Server socket path/UID/mode/peer identity là deployment config; nhận version và capability
   trước command. Reject schema/epoch/source/profile không hỗ trợ.
2. Validate request ID, revision, scope, size, pre/post limits, source mapping và signature/
   principal context; ghi durable inbox/dedup result trước ACK.
3. Evidence capture chạy trên FW encoded prebuffer độc lập viewer demand; pre-roll thiếu phải
   trả actual begin và `partial`, không báo đủ interval.
4. Storage job có bounded members/timeout/recovery; Start/End/Update ordering, duplicate,
   late End, FW restart và media full có disposition.
5. Receipt phân biệt `accepted`, `recording`, `ready`, `partial`, `failed`, `already_exists`,
   `rejected`, `expired`; media ID/request ID luôn correlate.
6. FW không đọc DB SQLite/Parquet riêng của AI; nếu UI cần data gọi query API C06.

## 5. SHM decision gate

Chỉ làm SHM sau khi profiler chứng minh UDS copy/serialization làm vượt CPU/latency budget.
SHM ring bắt buộc có fixed layout/version, sequence/generation, active-reader lease,
memory barriers, eventfd/lost-wakeup handling, stale consumer reclaim, crash recovery,
overflow policy và one-way ownership. Event alarm vẫn đi UDS/outbox hoặc được commit bền
trước khi overwrite. Không dùng video ring shared như metadata command ABI.

## 6. Task thực hiện

Usecase product đầu tiên đi qua E01–E07 là `security.fire_smoke_detection`. Envelope, outbox,
ACK và transport vẫn generic; chỉ S04 payload mapping được enable trong lát cắt đầu tiên. Chi tiết
thứ tự tích hợp App Manager, S04 event episode, P2 metadata và reference/FW receiver nằm tại
[kế hoạch product slice khói/lửa](fire_smoke_product_slice_plan.md). Reference receiver chỉ tạo
bằng chứng AI-side, không thay released-FW acceptance ở E07.

| Task | Owner | Đầu ra | Tiêu chí kết thúc |
|---|---|---|---|
| E01 | AI APP + BSP+FW | C07 wire schema/ACK/error/state RFC | Hai lead ký envelope, max bounds và version policy |
| E02 | AI APP | Neutral `event_delivery`/receipt port + outbox design | Ownership, retry, revoke, stop contract có unit tests |
| E03 | AI APP | UDS adapter/server mock, peer authorization, seqpacket framing | malformed/oversize/unknown/version tests pass |
| E04 | BSP+FW | Evidence service mock + durable inbox/dedup + media receipt | lost ACK/duplicate/restart/end ordering tests pass |
| E05 | AI APP | Integrate feature dispatch → outbox, priority/backpressure metrics | inference không block; durable receipt rõ |
| E06 | BSP+FW | Released prebuffer/profile/source mapping evidence | no-viewer, startup short pre-roll, profile change verified |
| E07 | AI APP + BSP+FW | Board IPC/evidence soak and reconciliation report | target trace, actual media intervals và resource budget |
| E08 | AI APP | SHM spike (chỉ nếu E07 profiler yêu cầu) | pass lease/crash/overflow gate hoặc quyết định không dùng |

## 7. Tiêu chí nghiệm thu

- [ ] D-Bus chỉ điều khiển; event không bị gửi qua API không có retry/dedup semantics.
- [ ] Duplicate command/retry ambiguous không tạo duplicate media; same request ID trả cùng
  disposition hoặc conflict nếu payload đổi.
- [ ] Durable local ACK, FW accepted ACK, recording và ready được quan sát riêng; restart
  không mất pending outbox ngoài cửa sổ đã công bố.
- [ ] Authorization kiểm actual payload fields, source/feature/policy revision ở enqueue và retry.
- [ ] Socket parser bounded; wrong peer/UID/schema/version/source/epoch bị reject có reason.
- [ ] Evidence actual interval, keyframe/prebuffer gap, media ID và retention state được trả;
  no viewer không làm mất evidence consumer.
- [ ] Alarm lane không bị query/Kafka/compaction starvation; overflow có counter/disposition.
- [ ] UDS board conformance chạy với released FW; SHM chỉ được bật nếu có measured benefit
  và pass active-reader/crash/overflow tests.

## Handoff

E01–E04 cần C01/C04/C07 từ contract plan. E05 nhận schema/query outbox từ metadata plan.
E07 là input integration plan; chỉ sau released-FW receipt mới đóng C07.

## Giới hạn và công việc tiếp theo

- Chưa có FW-approved event ABI; không sửa source production trước sign-off E01.
- Pre-roll, media source và retention do BSP+FW xác nhận theo release; AI APP không tự hứa.

## See also

- [Fire/smoke product slice](fire_smoke_product_slice_plan.md)
- [Architecture improvement master plan](README.md)
- [Contract and team scope plan](contract_and_team_scope.md)
- [Feature event dispatch](../../architecture/feature_event_dispatch.md)
- [FW release compatibility](../../contracts/fw_release_compatibility.md)
