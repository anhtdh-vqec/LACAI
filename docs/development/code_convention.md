# Code convention — normative v2

## 0. File naming (lead-requested migration)

Project-owned C/C++ source/header and executable tooling files MUST be named
`vqec_vision_<logical_name>.<extension>`, with lowercase snake_case logical names.
Examples: `vqec_vision_output_gate.cpp`, `vqec_vision_output_gate.hpp`,
`vqec_vision_output_gate_test.cpp`, `vqec_vision_check_source_layout.ps1`.
Spelling is `vision`, never `vison`. Directories remain snake_case without this prefix.

The registry file_id is derived from the logical name WITHOUT `vqec_vision_`;
existing dir_id/file_id pairs and function symbols remain unchanged by this migration.
Header/source pairs use the same logical name. Rename includes, CMake references,
tests and documentation together; do not leave duplicate forwarding headers silently.
Public header path changes require downstream rebuild/migration; no stable installed
SDK is delivered yet. This change does not rename wire keys, D-Bus methods, ring IDs,
RTSP paths, CMake targets or the required external executable `cameraai_app`.

Finite file-category exceptions: Markdown documents (including README.md/AGENTS.md),
tool-defined configuration/build names (CMakeLists.txt, dotfiles), manifests/config
data and packaging metadata retain their conventional names. Third-party source and
external source citations retain upstream filenames. New project executable scripts
do NOT inherit the configuration-file exception. Any further exception needs review.

This section takes precedence over older illustrative filenames below.

Ngày: 2026-09-06. MUST = bắt buộc; SHOULD = mặc định, ngoại lệ cần review.
Áp dụng source tự viết, tests và tooling; third-party giữ upstream style riêng.
C++17 là baseline. Tài liệu này thay thế convention PascalCase đã thảo luận trước.

## 1. Tên hàm — bắt buộc

`vqec_vision_ai_<dir_id>_<file_id>_<verb_object>`

- Dùng underscore giữa mọi thành phần; dấu * trong yêu cầu được hiểu là nối ý
  nghĩa function, không phải ký tự literal.
- dir_id: thư mục trực tiếp chứa implementation; 3–5 ký tự chữ thường/số,
  bắt đầu bằng chữ, lấy từ registry. Không tự cắt chuỗi tên thư mục.
- file_id: stem file không extension, viết tắt 3–5 ký tự theo registry.
- verb_object: snake_case, động từ diễn tả hành động + đối tượng đủ rõ.
- Áp dụng free function, method public/private/protected, static, inline,
  template, callback tự đặt tên và helper anonymous namespace.
- Header/source cùng một API dùng cùng prefix dựa trên implementation owner.
- Interface thuần/header-only dùng thư mục và file khai báo làm owner.
  Override ở Qualcomm/reference phải giữ nguyên tên interface khai báo.
  Helper riêng của backend dùng prefix theo file implementation backend.
- Di chuyển file chứa API public: giữ prefix cũ bằng compatibility exception +
  ADR/deprecation; không đổi symbol âm thầm. Private API đổi đồng bộ caller/tests.
- File có nhiều class vẫn một prefix; thêm tên đối tượng ở suffix để phân biệt.

Ví dụ registry dự kiến:
`src/adapters/qualcomm/vqec_vision_qnn_engine.cpp` →
`vqec_vision_ai_qcom_qneng_load_model`.
`src/adapters/qualcomm/vqec_vision_fastcv_processor.cpp` →
`vqec_vision_ai_qcom_fcprc_resize_image`.
`src/runtime/scheduler/vqec_vision_job_scheduler.cpp` →
`vqec_vision_ai_sched_jobsc_submit_job`.

Không hợp lệ: `loadModel`, `qnn_run`, `vqec_vision_ai_qc_qnn_run`,
`vqec_vision_ai_qcom_qneng_do_stuff`.

### Ngoại lệ hữu hạn

- Constructor/destructor: tên class do C++ quy định.
- Operator/conversion operator: spelling của C++; chỉ dùng khi semantics tự nhiên.
- `main`, entrypoint/symbol hoặc virtual method do external ABI bắt buộc.
- Standard customization như `begin/end` chỉ khi thực sự cần tương tác generic
  library, ghi ngoại lệ cụ thể; không dùng để né convention.
- Lambda không có tên hàm: biến chứa lambda snake_case; tham số vẫn có _.
- Test macro/framework generated names giữ cú pháp framework; helper tuân prefix.
- Tên API vendor đang gọi không đổi; wrapper tự viết phải đúng prefix.

Ngoại lệ lưu trong registry kèm file/symbol/reason/owner; không wildcard toàn repo.

## 2. Tên biến và kiểu

| Loại | Quy tắc | Ví dụ |
|---|---|---|
| Tham số khai báo và định nghĩa | _snake_case, cùng tên hai nơi | _frame, _timeout_ms |
| Biến local | snake_case, rõ vai trò | input_stride_bytes, pending_jobs |
| Member không static | snake_case_ | source_epoch_, job_queue_ |
| Global, namespace-scope object | g_snake_case | g_build_version |
| Static data member | g_snake_case | g_instance_count |
| Function-local static | g_snake_case | g_lookup_table |
| Constant local/member | như storage scope tương ứng | max_planes, max_planes_ |
| Class/struct/enum/type alias | snake_case | frame_lease, tensor_descriptor |
| Enum value | snake_case | status_code::invalid_argument |
| Namespace | snake_case | vqec::vision::ai |
| Macro/include guard | VQEC_VISION_AI_UPPER_SNAKE_CASE | VQEC_VISION_AI_API |
| File/thư mục | snake_case | vqec_vision_frame_lease.hpp, vqec_vision_qnn_engine.cpp |

- _ chỉ dùng cho parameter ở parameter scope; không khai báo namespace/global
  bắt đầu bằng _. Cấm __ và _Upper ở identifier tự viết.
- Không biến global mutable trong business logic. Prefix g_ không phải cho phép
  singleton/global state; ngoại lệ runtime SDK cần owner và lifecycle rõ.
- Không Hungarian notation p/pp/u32 theo kiểu; dùng tên vai trò và ownership:
  borrowed_frame, frame_fd, tensor_bytes; không data1/tmp2/obj/x trừ toán cục bộ.
- Tên thông thường 2–4 từ; đây là hướng dẫn, không cắt mất nghĩa để đạt giới hạn.
  i/j được phép vòng lặp nhỏ, x/y trong tọa độ. Kích thước/thời gian có đơn vị.
- Boolean: is_/has_/can_/should_; parameter: _is_enabled; member: is_ready_.
- Không đặt tên biến model là person nếu nó chứa cả bộ metadata nhiều model.

## 3. Layout và format

### Literal policy

Repeated protocol values and safety/resource limits must have one named owner with
units, provenance and change policy. Platform-dependent tuning belongs in validated
configuration within safety ceilings, not a renamed hardcoded default. Do not merge
unrelated values merely because their current numeric values match. Literal zero/one,
array indices, standard byte framing and diagnostic prose need not become meaningless
constants; explain format arithmetic. Tests may retain independent known boundary values
to detect unintended contract changes. No blanket claim of no magic literals from grep.

- UTF-8, LF, newline cuối file; 4 spaces; không tab; 100 columns.
- .hpp cho C++; .h chỉ C-compatible ABI; .cpp implementation.
- Brace attached; luôn braces cho if/for/while, kể cả một dòng.
- Header tự include đủ; include guard theo project prefix, không reserved names.
- Include: own header; standard; third-party; project (nhóm cách dòng).
  Tool formatter là nguồn chuẩn về whitespace.
- Không using namespace trong header; không public vendor include.
- Comment giải thích WHY, invariant, đơn vị; không kể lại câu lệnh.
- TODO phải có issue/owner/điều kiện bỏ; không để TODO vô danh trên đường lỗi.

## 4. Ví dụ chữ ký (minh họa, chưa phải API đã implement)

```cpp
namespace vqec::vision::ai {

class qnn_engine {
public:
    qnn_engine() = default;
    ~qnn_engine() noexcept;

    [[nodiscard]] status
    vqec_vision_ai_qcom_qneng_load_model(const model_spec& _model);

private:
    bool is_ready_{false};
};

}  // namespace vqec::vision::ai
```

Prototype function pointer cũng đặt tên tham số: `void (*callback)(void* _context)`.
External callbacks dùng context object; không dùng global để tìm instance.

## 5. Ownership và resource lifetime

- RAII cho FD, map, pool lease, SDK context, dynamic library, worker.
- unique_ptr/move-only mặc định. shared_ptr chỉ khi thực sự nhiều owner, ghi
  release point; không dùng shared_ptr để che vòng đời không hiểu.
- Raw pointer/reference là borrow; owner phải sống đủ lâu, đặc biệt async.
- Không new/delete/malloc/free rải trong logic; resource wrapper là nơi quản lý.
- FD nhận từ IPC: quy định transfer/dup/close, CLOEXEC, invalid=-1.
- FD numeric không phải identity: cache theo allocation identity + generation,
  plane layout + device/context; invalidation khi epoch/context đổi.
- frame_lease destructor không được ACK sớm khi job còn đọc. Job sở hữu lease
  đến completion; shutdown phải drain, không dựa destructor để cancel SDK.
- Destructor noexcept, không throw, không chờ vô hạn. Explicit shutdown báo lỗi;
  nếu không chứng minh quiescence, quarantine resource và báo FW recovery.
- Validate kích thước/overflow trước allocate/map/index/crop.

## 6. Threading

### Hot-path memory and data layout

- Startup/configuration code may own `std::string` and `std::vector`; frame callbacks,
  scheduling, preprocessing, decode and tracking MUST use pre-sized bounded storage or
  an explicitly measured pool/arena. No JSON, filesystem access or unbounded container
  growth on the per-frame path.
- Reserve identifier/string storage at activation and pass stable numeric/catalog IDs on
  the hot path. Format human-readable names only at control/output boundaries.
- Prefer contiguous structures for iteration and separate cold/optional payloads from
  frequently accessed geometry, score and identity fields. Data-oriented layout changes
  require benchmarks; do not use packing pragmas to save bytes in normal C++ objects.
- A borrowed pointer/view is valid only inside its documented owner lifetime. Async work
  retains an explicit lease/owner; `string_view`/raw spans must not outlive the catalog,
  tensor sample or result arena that backs them.
- A `shared_ptr` or FD duplication is an ownership operation, not proof of zero-copy.
  Document every copy boundary and the hardware completion that permits reuse. Claims of
  zero-copy require allocator/import, cache/fence and board-trace evidence per stage.
- Collect pool occupancy, high-water bytes, allocation count and fallback-copy counters.
  A memory optimization without correctness and workload measurements is not accepted.

- Một owner serial cho state của mỗi source/tracker/temporal feature.
- Camera callbacks chỉ validate/enqueue; vendor callbacks chỉ enqueue completion.
- Không detached thread, polling busy loop hoặc queue vô hạn.
- Queue phải có capacity + overflow policy + stop policy + metrics.
- Không giữ mutex khi gọi RPC/SDK blocking/I/O/callback ra ngoài.
- Lock order cố định; atomic dùng cho dữ liệu nhỏ, không thay lock state phức hợp.
- Timeout là hết thời gian chờ, KHÔNG đồng nghĩa hủy hardware job.
- Chỉ mark completed sau completion thật, hoặc recovery bảo đảm DMA đã dừng.
- Dùng steady/monotonic clock cho duration/deadline; UTC chỉ correlation/export.

## 7. Error handling và API

- status/result<T> cho lỗi dự kiến; không bool mất nguyên nhân ở external boundary.
- Phân biệt invalid_argument, unsupported, incompatible_model, resource_exhausted,
  timeout, cancelled, source_lost, backend_fault, unauthorized.
- [[nodiscard]] cho kết quả không được bỏ qua.
- Không exception vượt C ABI hoặc vendor callback; catch và chuyển status.
  Nội bộ dùng RAII và exception có kiểm soát, không throw mỗi frame bình thường.
- Không assert cho dữ liệu IPC/model/user có thể sai; validate trả lỗi.
- Hàm public ghi input/output, ownership, thread safety, blocking/deadline,
  completion semantics, errors, pre/postconditions.
- Không expose std::string/vector/exception/C++ object layout qua binary plugin ABI.
- Export opaque handles + fixed-width fields + struct_size + abi_major/minor;
  allocator/release phải cùng owner, không tự free memory của module khác.

## 8. Image/tensor correctness

- Mỗi plane có offset/stride/size; không mặc định width == stride.
- NV12/NV21/RGB/BGR, color range/matrix, crop/rotate/letterbox là metadata rõ.
- Giữ source-to-tensor transform để ánh xạ box/landmark; test border/odd ROI.
- Tensor có dtype/layout/rank/dims/strides/quantization riêng từng tensor.
- Không mặc định mọi output float32 hoặc cùng type; không ép toàn bộ để tiện NMS.
- Golden test phải bao gồm interpolation, rounding, clipping, pad, channel order.
- CPU normalization/postprocess được phép không OpenCV khi có budget và lý do;
  không gắn nhãn hardware cho CPU loop.

## 9. Build, dependency và security

- CMake target-scoped compile options/includes/libs; không global link_directories.
- Không kéo header từ sibling repo bằng ../../; contract SDK/version là dependency.
- Vendor library chỉ adapter target; host build không cần vendor SDK.
- Pin toolchain/sysroot/compiler/runtime; không download dependency ngầm khi build.
- Warnings as errors trên source tự viết trong CI; third-party tách scope.
- Chọn -Wall -Wextra -Wpedantic và kiểm tra conversion theo compiler/toolchain.
- Không -ffast-math mặc định; không thay numerical semantics nếu thiếu golden.
- Library/model paths đến từ verified manifest + allowed root, không raw remote input.
- Không log ảnh, face embedding, license key/token; giới hạn kích thước parser/message.
- Quyền feature/attribute/output kiểm tra ở runtime và output boundary.

## 10. Testing và enforcement

- clang-format kiểm whitespace, KHÔNG chứng minh đúng tên hoặc ownership.
- Naming cần AST checker hiểu declaration owner/override/ngoại lệ; regex chỉ lint sơ bộ.
- Đã có structural filename/include/CMake checker; AST registry/naming checker và CI
  vẫn chưa triển khai. Structural checker không chứng minh symbol naming đúng.
- Reviewer kiểm 100% tên hàm/parameter/global trong thời gian chưa có AST checker.
- Unit + contract + golden + replay; board test bắt buộc cho DMA/SDK/performance.
- ASan/UBSan/TSan trên target hỗ trợ; host mock không chứng minh device sync.
- Mỗi PR có checklist; ABI/sync/security cần lead + module owner.
- Sửa rule bằng PR tài liệu/ADR, không tự exception trong một source file.
