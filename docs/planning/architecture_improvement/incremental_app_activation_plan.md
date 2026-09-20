# Kế hoạch activation delta theo ứng dụng

Kế hoạch triển khai activation delta và reference counting dependency dùng chung để bật, tắt hoặc
cấu hình một ứng dụng mà không khởi động lại các ứng dụng không liên quan.

**Status:** planned — ADR/kiến trúc đã được soạn; source, eSDK/QEMU và QCS6490 chưa qua gate.
**Layer:** docs. **Source:** `n/a`.

## Trách nhiệm

- Hoàn thiện nền runtime chung trước khi nhận usecase product thứ hai.
- Giữ snapshot đầy đủ của App Manager là authority; delta chỉ là kết quả so sánh nội bộ.
- Nghiệm thu bằng identity/lifecycle counter, FPS, CPU và RSS; không dùng việc tiến trình còn sống
  làm bằng chứng duy nhất.

## Phạm vi và điều kiện đóng

Plan đóng khi một source có ít nhất hai ứng dụng đã cài đặt, trong đó có dependency dùng chung và
dependency riêng, vượt qua chuỗi bật/tắt 5–10 giây mà:

- PID service và lease/source epoch của ứng dụng không liên quan không đổi;
- shared graph không nhận drain/load khi refcount vẫn lớn hơn 0;
- unique graph dừng submission, drain completion rồi unload khi refcount về 0;
- feature/output bị thu hồi trước khi event muộn được dispatch;
- preview VLC tiếp tục, FPS đạt workload khai báo, không tăng RSS theo số vòng;
- full eSDK/QEMU/CTest và kiểm tra cấu trúc/tài liệu pass.

Fixture hạ tầng được phép dùng cho ứng dụng thứ hai; fixture không được ghi thành product acceptance
cho FR, hút thuốc hoặc bất kỳ usecase chưa có golden/model gate.

## Work breakdown

| Mốc | Công việc | Đầu ra | Tiêu chí nghiệm thu |
|---|---|---|---|
| D0 | Logic-tested | ADR 0012, dependency plan/delta bounded | 18 app, shared/unique/conflict/stale/capacity tests pass; input lỗi không đổi output |
| D1 | Logic-tested phạm vi fake-port | Active mask ở pump; lifecycle add/remove từng slot trong session | `2 -> 1` không lifecycle; `1 -> 0` chỉ drain/unload slot đích; slot khác tiếp tục submit/result |
| D2 | Feature delta | Candidate manager/fan-out, transactional pipeline rebind, output policy candidate | Config/disable chỉ thay owner đích; owner khác giữ pointer/state/counter; revoke chặn event muộn |
| D3 | Service reconciler | So sánh snapshot, capacity của app đã cài, apply/fallback, publish revision/metrics | Desired/config/entitlement không thoát generation khi capacity/identity tương thích |
| D4 | Hard tests | Fault injection, eSDK/QEMU full suite, target-native tests | Load/drain failure, source loss, in-flight disable, rapid toggle, App Manager restart không leak/ACK sớm |
| D5 | Board gate | Hai app fixture + S04 trên QCS6490, VLC/FPS/CPU/RSS/lifecycle log | Chuỗi toggle đạt điều kiện đóng; ghi rõ fixture và phần chưa phải product acceptance |
| D6 | Chuẩn hóa tài liệu | Trạng thái, capability matrix, implementation status, board record | Tài liệu khớp source/evidence; không còn claim full-generation cho toggle tương thích |

## Kế hoạch kiểm thử chi tiết

### Planner và authority

- Không đổi snapshot revision, revision lùi, association trùng và app/source lạ.
- 18 ứng dụng trên 16 source trong giới hạn; vượt giới hạn trả lỗi mà giữ plan cũ.
- Hai app dùng chung model: count `2`; tắt một app: `2 -> 1`; tắt app cuối: `1 -> 0`.
- Cùng model ID nhưng khác digest/preprocess/cadence không được share.
- Revoke entitlement khi job đang chạy: output mới bị chặn ngay, graph chỉ unload sau completion.

### Session và lifetime

- Disable khi graph idle, loading, running, có input outstanding và có result vừa hoàn thành.
- Không release frame owner trước completion; timeout để lại recovery-required.
- Add slot không frame không được load HTP/DSP; add trong source live dùng epoch hiện tại.
- Slot lỗi load/start/drain không dừng slot khỏe; lỗi completion không được che bằng refcount.
- Rapid coalescing on/off không tạo hai lifecycle đồng thời cho cùng slot.

### Feature và output

- Config threshold mới tạo processor mới cho app đích; temporal state app khác không reset.
- Hai app dùng chung model nhưng output scopes khác không hợp quyền.
- Event mang policy revision cũ bị recheck và từ chối sau revoke.
- Candidate allocation/config failure giữ nguyên fan-out và policy cũ.

### Board

1. Xác minh alias/machine ID theo hồ sơ board; stage candidate đúng commit dưới `/opt/lacai`.
2. Chạy App Manager + service + camera compatibility + RTSP, mở URI bằng VLC từ host.
3. Cài/entitle hai package fixture đã ký, bật cả hai, ghi PID/source epoch/graph counters.
4. Toggle lần lượt từng app mỗi 5–10 giây tối thiểu 20 chuyển trạng thái; xen kẽ restart App Manager.
5. Thu FPS ring/RTSP, CPU `pidstat`, RSS/PSS, nhiệt độ nếu có và graph lifecycle counters.
6. Tắt shared consumer đầu, xác nhận graph giữ nguyên; tắt consumer cuối, xác nhận drain/unload.
7. Giữ workload VLC chạy sau gate nếu không có lỗi để người dùng kiểm tra trực quan.

## Thứ tự commit

1. Contract/ADR/plan và registry tài liệu.
2. Planner + unit/contract tests.
3. Pump/session mutable slot + lifetime tests.
4. Feature/output transactional rebind + tests.
5. Service reconciler + integration/fault tests.
6. Board evidence và cập nhật trạng thái.

Mỗi commit chỉ chứa thay đổi thuộc mốc tương ứng; không push. Sau mỗi source step chạy source/docs
layout và test liên quan bằng eSDK trước khi commit.

## Giới hạn và việc tiếp theo

- Plan không productize FR hoặc hút thuốc; chúng cần model/golden/quality gate riêng.
- Artifact update và app install bổ sung capacity có thể dùng source-local/full replacement ở baseline.
- Released-FW acceptance và backend conformance không được suy ra từ fixture App Manager.

## See also

- [Kiến trúc activation delta](../../architecture/incremental_app_activation.md)
- [ADR 0012](../../adr/0012_incremental_app_activation.md)
- [Product slice khói/lửa](fire_smoke_product_slice_plan.md)
- [Phân phối ứng dụng usecase](usecase_app_distribution_plan.md)
