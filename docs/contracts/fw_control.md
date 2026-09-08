# FW control, outputs, entitlement và BSP handoff

Status: proposal for four-team review, không phải implemented protocol.

The proposal below extends, not replaces, the released AI D-Bus and H264 preview
interfaces. See [FW release baseline](fw_release_compatibility.md) for exact legacy
methods, persistence behavior, ring ABI and RTSP/UI demand chain. AI APP owns preview
overlay/encode/ring output; FW owns persistent evidence and recording. New control
methods and signed-grant provisioning must not be assumed present in released FW.

## Ownership

| Team | Deliverable |
|---|---|
| FW BSP | board/image/sysroot/SDK + memory/import/sync/reset sample, capability limits |
| FW software | camera transport, service supervision, config, provisioning, install/update, event/evidence/gallery/search |
| AI Model | full model package + golden + quality report |
| AI APP | runtime/backend integration, feature rules, entitlement enforcement, outputs/metrics + app IPK |

BSP kit phải pin header và runtime binaries đồng bộ, device dependencies,
permissions, allocator/cache/fence API, SDK threading rules, test model, profiler,
thermal budgets, redistributable scope. Source OSS không thay kit này.

## Control plane

get_capabilities, get_health, list_features, get_effective_config,
validate_config, apply_config(expected_revision, request_id), set_feature_state,
apply_entitlement, get_runtime_status, request_diagnostics, prepare_shutdown.

Config atomic snapshot: validate schema/license/model/resources trước publish.
Revision CAS; stale revision reject; failed apply không để half-enabled graph.
Response tách accepted command và actual running readiness.
Status theo source/feature: installed, entitled, desired, supported, compatible,
admitted, effective_state, reason, config/model/license revisions.

Auth từ transport + FW policy; không tin field customer_id do caller tự ghi.
Parse bounds; rate limits; diagnostic dump không có secrets/biometrics mặc định.

## Entitlement

Source update: [output_gate](../architecture/output_gate.md) now provides a pure
deny-by-default evaluator for trusted source/feature/attribute policy with revision CAS,
monotonic validity and queued-output revision checks. It is not a signed-grant verifier,
not connected to a transport/router yet, and does not implement compute admission.

Signed grant chứa grant_id, issuer/key_id, revision, device/customer scope,
feature_ids, attribute scopes, source/camera limits, not_before/expires,
offline policy và signature. Key provisioning/trusted time thuộc FW.
Runtime verify grant, enforce deny-by-default cho feature ngoài scope.
Installed bundle không tự cấp quyền mọi feature trong bundle.

Bật: validate -> admission -> load deps -> start -> effective running.
Tắt/revoke: block unauthorized output -> stop scheduling feature -> drain ->
release only unused deps. Tracker shared không bị reset nếu consumer khác còn dùng.
Recheck revision khi dispatch queued output; retry spool cũng phải theo policy
revocation/retention, không xuất embedding cũ vô điều kiện.

Signed offline license không biết remote revocation mới khi không có kết nối.
Clock rollback/key rotation/grace policy phải chốt với FW. Chống root bypass cần
trusted firmware chain của FW; APP software check không tự cung cấp bảo đảm đó.

## Outputs

Envelope: schema version, event_id, source/epoch, capture time + clock mapping,
feature_id, model/config/license revision, track/entity references, quality,
typed attributes, geometry coordinate space, evidence correlation.

- Live observations/tracks: bounded lossy, drop metrics.
- Alarm: at-least-once + event_id dedup; bounded spool, retry/backoff/TTL.
- Counts: window_id/sequence/checkpoint, consumer dedup; không cộng lặp khi retry.
- Heatmap: grid/calibration version, bucket times, exposure/gap metadata.
- Retrieval: authorized embeddings/index records, model version, retention.
- Evidence request: source/time window/event id; FW trả pending/ready/failed +
  evidence reference. AI không tự giữ raw4K để làm clip recorder.

Disk/network unavailable: explicit degraded state, spool bounds và overflow
priority đã thỏa thuận; không gọi delivery durable vô hạn. FW quản lý storage
quota và retention. AI không log/raw-export mặt/embedding theo mặc định.

## Package/update

Logical packages runtime/backend/features/models độc lập, metadata compatible.
FW install transaction: download/verify -> stage -> validate set -> stop/drain ->
activate coherent set -> health check -> commit or rollback coherent set.
Không chạy script package kill service trước khi có quiescence guarantee.
Signed model/feature manifest không replace camera authorization.

## Acceptance

Forged/expired/wrong-device grant; config CAS conflicts; unauthorized attributes;
revoke during inference/output retry; offline/clock changes; partial install;
ABI/model mismatch; crash/restart; retry dedup; disk full; gallery embedding
version mismatch; evidence time drift. Mỗi test có owner và expected reason.
