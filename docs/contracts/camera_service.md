# Unified FW RAW Source contract — proposal v1

Owner: FW software + BSP; consumer/reviewer: AI APP.
Status: chưa ký, không mô tả IPC API hiện đang tồn tại.
Contract không cố định 4K/25 FPS. Mỗi source phải trả effective profile chính xác;
RAW nghĩa là post-ISP uncompressed NV12 (không Bayer). 3840x2160@25 chỉ là một
workload cần đo; mọi tổ hợp resolution/FPS và số source đồng thời phải được BSP/FW
khai capability và nghiệm thu riêng.

## Baseline integration update — 2026-09-06

AI implementation will consume the existing FW third NV12/FD stream, not wait for
the proposed 4K/versioned contract below. See the standalone
[FW requirements and current integration baseline](fw_camera_integration_requirements.md)
for actual socket paths, profiles, legacy wire layout, control calls and safety gaps.
The operations and extended descriptors below remain a future proposal, not APIs
that the current receiver may assume exist. P0 safety sign-off remains a production gate.

## Unified multi-source RAW update — 2026-09-08

- AI Camera và AI Box đều phải cung cấp 1..16 logical FW RAW sources với cùng descriptor,
  lease/ownership, synchronization, epoch và backpressure semantics cho AI APP.
- Trên AI Camera, FW lấy RAW từ sensor/ISP/Camera Service. Trên AI Box, FW sở hữu toàn bộ
  RTSP endpoint, credential, demux, decode, decoder surface và reconnect trước RAW.
- AI APP chỉ resolve `raw_source_ref`; không nhận URI/codec/credential, không demux/decode
  RTSP và không có nhánh xử lý riêng theo loại sản phẩm.
- Capability phải nêu max source, RAW resolution/FPS/format/memory ranges và tổ hợp workload
  đồng thời. Chi phí decoder/RTSP trên AI Box do FW budget nhưng phải được tính khi FW trả
  khả năng tổng thể cho admission.
- Source profile change/reconnect increments epoch and forces bounded drain/rebind;
  AI APP không tiếp tục temporal state qua discontinuity.

Chi tiết cấu hình và memory admission nằm tại
[multi-source configuration](../architecture/multi_source_configuration.md).

AI APP resolves each configured identity through the bounded
[FW RAW-source resolver](../architecture/raw_source_resolution.md). A future FW registry
must return an attachment route without exposing upstream RTSP details. Until that RPC is
released, the compatibility helper maps the existing `third` Unix-socket naming convention
at activation time; no frame-path component derives product topology.

Compatibility note: the released Camera1 RPC carries both `camera_id` and `channel_id`,
but current FW validates `channel_id=0`; multi-sensor Camera uses distinct `camera_id`
values. AI acquisition now preserves both values in its request identity and sends the
configured channel instead of hard-coding it. A non-zero channel therefore still fails
closed against the current release until FW advertises support; this change does not
invent a new FW topology.

## Control operations

| Operation | Request -> Response |
|---|---|
| get_capabilities | version/client -> modes, formats, memory/sync, limits |
| acquire_source | request_id, app_instance, source_id, requirements -> lease_id, epoch, effective_profile, expiry |
| renew_lease | request_id, lease_id -> expiry/status |
| get_source_status | lease/source -> state, profile, epoch, counters |
| release_source | request_id, lease_id -> released hoặc draining |
| attach_consumer | authenticated lease + protocol -> transport endpoint/session |
| quiesce | source/lease, deadline -> ack request; quiesce_complete khi hết readers |
| release_frame | session, lease, epoch, frame token, completion -> ACK status |

Event: source_state_changed, source_profile_changed, source_discontinuity,
lease_revoked, frame_available, quiesce_complete.
Tên logical operation snake_case; binding DBus/socket do FW và APP chốt.
Function C++ implement binding vẫn theo prefix convention, không đổi wire name.

request_id idempotent cho mutation; retry cùng id trả cùng kết quả, không tạo
lease mới. Version major không tương thích phải reject; minor negotiate.
Errors: unauthorized, unsupported_profile, source_busy, invalid_lease,
stale_epoch, protocol_mismatch, resource_exhausted, timeout, source_lost.

## Media transport

Đề xuất Unix domain socket + SCM_RIGHTS cho FD, descriptor message có framing
và size limits. DBus chỉ control nếu FW chọn. Có thể thay transport bằng ADR,
giữ semantics. Không gửi pixel 4K qua control bus, không serialize numeric FD
rồi kỳ vọng FD dùng được ở process khác.

Bắt buộc bind transport vào authenticated peer credentials + authorized lease;
consumer_id do client gửi không phải bằng chứng quyền truy cập.
Xác định descriptor per-frame hay pool-registration + token; nếu register pool,
pool allocation_id/generation bền trong session và invalidate sau restart.

## Frame descriptor tối thiểu

magic, protocol_major/minor, descriptor_size, message_type;
producer_instance_id, source_id, source_epoch, profile_revision;
frame_sequence, buffer_allocation_id, buffer_generation, frame_lease_token;
capture_timestamp_ns, clock_domain, UTC mapping revision/uncertainty;
width,height, pixel_fourcc, modifier, orientation;
plane_count và mỗi plane: handle_index, offset_bytes, stride_bytes,
row_count, valid_size_bytes, allocation_size_bytes;
color_matrix, color_range, chroma_siting;
acquire_sync_kind + sync handle/token nếu có; deadline/hold budget.

Validate offset + span <= allocation_size có checked arithmetic, plane/FD count
có upper bound, dimensions đúng negotiated profile, timestamp không nhầm UTC.
Không expose GstVideoFormat/GstBuffer trong neutral ABI.
Raw C++ struct không phải wire serializer: pin endian, widths, layout, lengths.

## Ownership và synchronization

Producer cấp frame read-only. AI không overlay/write vào input shared.
AI giữ lease đến reader cuối hoàn tất; close FD không thay ACK, ACK không dựa
timeout. Acquire fence phải được wait/import trước device read.
Cache CPU START/END và hardware completion là hai cơ chế khác nhau.
C2D gpointer token không gửi như Linux sync_file FD.
release_frame dùng completion token đúng negotiated mechanism; reject stale
token/epoch, duplicate ACK idempotent, không double-return pool slot.

Camera không recycle buffer bị AI/device giữ chỉ vì consumer timeout.
Khi consumer chết: FW+BSP phải có quarantine/quiesce/reset guarantee để chứng
minh DMA dừng; nếu chưa có, không bật unsafe reclaim policy.
Một source acquisition manager của APP tổng hợp mọi feature requirement.

## QoS và biến động

Chốt max_inflight, max_hold_ms, queue_depth, drop policy, min/max FPS và profile
priority so với recording/live streaming. Không vô hạn giữ buffer chờ inference.
Đổi resolution/stride/format: notify, epoch/profile revision boundary, drain old
pool trước replace; không sửa descriptor âm thầm với memory layout cũ.
Mất source/restart: invalidate epoch, reset temporal state; reconnect có backoff.

## Nghiệm thu chung

4K pattern có stride padding/color bars; slow consumer; queue full; duplicate/
lost ACK; disconnect giữa job; producer restart; stale FD/pool generation;
profile change khi frame đang chạy; malformed descriptor; unauthorized peer;
Camera record/live tiếp tục đúng KPI khi APP overload.
Ghi trace capture -> receive -> last hardware read done -> release ACK.

## Sign-off còn thiếu

BSP: memory/sync/reset guarantees + formats.
FW software: wire schema, auth, idempotency, supervision + source lifecycle.
AI APP: hold budgets, accepted modes, reconnect/overload behavior.
Thông số numeric QoS và clock accuracy phải điền sau đo, trước gate tuần4.
