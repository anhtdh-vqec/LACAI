# Code convention — normative v1

**Status:** normative — current C++17 convention.

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
RTSP paths, CMake targets or the required external executable `vqec_ai_vision_applications`.

Finite file-category exceptions: Markdown documents (including README.md/AGENTS.md),
tool-defined configuration/build names (CMakeLists.txt, dotfiles), manifests/config
data and packaging metadata retain their conventional names. Third-party source and
external source citations retain upstream filenames. New project executable scripts
do NOT inherit the configuration-file exception. Any further exception needs review.

This section takes precedence over older illustrative filenames below.

Date: 2026-09-06. MUST = required; SHOULD = default, exceptions need review.
Applies to self-written source, tests and tooling; third-party keeps its own upstream style.
C++17 is the baseline. This document replaces the PascalCase convention discussed earlier.

## 1. Function names — required

`vqec_vision_ai_<dir_id>_<file_id>_<verb_object>`

- Use an underscore between every component; the * in the requirement means joining
  the function's semantic parts, not a literal character.
- dir_id: the directory directly containing the implementation; 3–5 lowercase
  letters/digits, starting with a letter, taken from the registry. Do not truncate
  the directory name on your own.
- file_id: the file stem without extension, abbreviated to 3–5 characters per registry.
- verb_object: snake_case, a verb describing the action + a sufficiently clear object.
- Applies to free functions, public/private/protected methods, static, inline,
  template, self-named callbacks and anonymous-namespace helpers.
- Header/source for the same API use the same prefix based on the implementation owner.
- Pure/header-only interfaces use the declaring directory and file as owner.
  Overrides in Qualcomm/reference must retain the declared interface name.
  A backend's own helper uses the prefix of the backend implementation file.
- Moving a file containing a public API: keep the old prefix via a compatibility
  exception + ADR/deprecation; do not change symbols silently. Private APIs change
  caller/tests in sync.
- A file with many classes still uses one prefix; add the object name in the suffix
  to distinguish them.

Expected registry examples:
`src/adapters/qualcomm/qnn/vqec_vision_qnn_engine.cpp` →
`vqec_vision_ai_qcom_qneng_load_model`.
`src/adapters/qualcomm/gstreamer/vqec_vision_fastcv_processor.cpp` →
`vqec_vision_ai_qcom_fcprc_resize_image`.
`src/runtime/scheduler/vqec_vision_job_scheduler.cpp` →
`vqec_vision_ai_sched_jobsc_submit_job`.

Invalid: `loadModel`, `qnn_run`, `vqec_vision_ai_qc_qnn_run`,
`vqec_vision_ai_qcom_qneng_do_stuff`.

### Finite exceptions

- Constructor/destructor: class name is dictated by C++.
- Operator/conversion operator: C++ spelling; only use when semantics are natural.
- `main`, entrypoint/symbol or virtual method required by an external ABI.
- Standard customization such as `begin/end` only when generic library interaction is
  genuinely needed, with the specific exception recorded; do not use it to dodge the
  convention.
- A lambda with no function name: the variable holding the lambda is snake_case; its
  parameters still carry _.
- Test macro/framework generated names keep the framework syntax; helpers follow the prefix.
- Names of vendor APIs being called do not change; self-written wrappers must use the
  correct prefix.

Exceptions are recorded in the registry with file/symbol/reason/owner; no repo-wide wildcard.

## 2. Variable and type names

| Category | Rule | Example |
|---|---|---|
| Parameter declaration and definition | _snake_case, same name in both places | _frame, _timeout_ms |
| Local variable | snake_case, clear role | input_stride_bytes, pending_jobs |
| Non-static member | snake_case_ | source_epoch_, job_queue_ |
| Global, namespace-scope object | g_snake_case | g_build_version |
| Static data member | g_snake_case | g_instance_count |
| Function-local static | g_snake_case | g_lookup_table |
| Constant local/member | same as the corresponding storage scope | max_planes, max_planes_ |
| Class/struct/enum/type alias | snake_case | frame_lease, tensor_descriptor |
| Enum value | snake_case | status_code::invalid_argument |
| Namespace | snake_case | vqec::vision::ai |
| Macro/include guard | VQEC_VISION_AI_UPPER_SNAKE_CASE | VQEC_VISION_AI_API |
| File/directory | snake_case | vqec_vision_frame_lease.hpp, vqec_vision_qnn_engine.cpp |

- _ is used only for parameters in parameter scope; do not declare a namespace/global
  starting with _. __ and _Upper are forbidden in self-written identifiers.
- No mutable global variables in business logic. The g_ prefix is not permission for
  singleton/global state; runtime SDK exceptions need a clear owner and lifecycle.
- No type-based Hungarian notation p/pp/u32; use role and ownership names:
  borrowed_frame, frame_fd, tensor_bytes; no data1/tmp2/obj/x except local math.
- Names are normally 2–4 words; this is guidance, do not cut meaning to meet a limit.
  i/j are allowed for small loops, x/y in coordinates. Sizes/durations carry units.
- Boolean: is_/has_/can_/should_; parameter: _is_enabled; member: is_ready_.
- Do not name a model variable person if it holds the metadata set of multiple models.

## 3. Layout and format

The first LACAI baseline uses version 1 for every project-owned schema and wire/ABI.
`vqec_vision_version_registry.h` is the numeric authority; per-contract aliases and
JSON schema/fixture literals must match it. A pre-release compatibility adapter does
not consume a product version number. An increment needs an approved migration ADR.
Vendor and released-FW ABI identifiers are read from their owners' contracts, not
renumbered by LACAI.

### Literal policy — MUST: no magic number, magic string, hardcode

Applies to production code, tools, config defaults and fixture/harness. Literals without
semantics/provenance are forbidden, as is embedding deployment decisions into algorithms.

- Values that change by board/model/usecase/deployment (path, endpoint, factory selection,
  model/feature ID, threshold, FPS, dimensions, timeout, queue/pool budget, retry, log
  level) MUST come from configuration/catalog/manifest validated at activation.
  Converting a literal to `constexpr` does not make it any less hardcoded.
- Protocol/schema/ABI keys, vendor property names, enum nicks and safety ceilings MUST
  have a single per-domain owner, a meaningful name, units, contract/version provenance
  and a change policy. Vendor strings live in the adapter; do not pull them into the
  neutral layer. Do not change wire spelling to make code look nice. Do not group values
  with different meanings just because they are equal.
- Numeric enum/sentinel MUST use a named type or constant; check overflow before
  arithmetic. Default values are valid only when the schema states them and validation
  checks the limits; a missing required value must be rejected, not silently replaced by
  a chosen backend/model.
- Do not hardcode entitlement/admission to true in production. The harness must clearly
  identify fixture mode; fixture values have names or a data file and are not reused as
  product defaults. Independent boundary tests may keep literals and record the reason to
  catch contract drift.
- Exceptions with self-evident semantics: zero/one of initialization/arithmetic, local
  index, standard format formulas with explanation, diagnostic prose, include paths and
  external ABI spelling. Exceptions do not permit burying a timeout, port, schema ID or
  policy in code.
- Each PR reviews literals by domain: fixed contract or configurable policy, source and
  unit, missing/invalid/boundary tests, no silent fallback. The exception registry records
  file, literal/domain, reason, owner and removal condition. Do not declare hardcode
  cleanliness via grep.
- Enforcement: the structural checker currently does NOT check literal semantics. Manual
  review is required; AST/literal lint with an allowlist is the next step, not existing
  evidence.

- UTF-8, LF, trailing newline at end of file; 4 spaces; no tabs; 100 columns.
- .hpp for C++; .h only for C-compatible ABI; .cpp implementation.
- Brace attached; always use braces for if/for/while, even for a single line.
- Header includes everything it needs; include guards follow the project prefix, no
  reserved names.
- Include order: own header; standard; third-party; project (groups separated by a blank
  line). The formatter tool is the authority on whitespace.
- No using namespace in headers; no public vendor include.
- Comments explain WHY, invariants, units; they do not restate the statement.
- TODO must have an issue/owner/removal condition; do not leave anonymous TODOs on an
  error path.

## 4. Signature examples (illustrative, not an implemented API)

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

Prototype function pointers also name their parameters: `void (*callback)(void* _context)`.
External callbacks use a context object; do not use a global to find the instance.

## 5. Ownership and resource lifetime

- RAII for FD, map, pool lease, SDK context, dynamic library, worker.
- unique_ptr/move-only by default. shared_ptr only with genuinely multiple owners, with
  the release point documented; do not use shared_ptr to hide an unclear lifetime.
- A raw pointer/reference is a borrow; the owner must live long enough, especially async.
- No scattered new/delete/malloc/free in logic; the resource wrapper is the place to
  manage it.
- FD received from IPC: specify transfer/dup/close, CLOEXEC, invalid=-1.
- An FD number is not identity: cache by allocation identity + generation, plane layout +
  device/context; invalidate when epoch/context changes.
- The frame_lease destructor must not ACK early while a job is still reading. The job owns
  the lease until completion; shutdown must drain, not rely on the destructor to cancel
  the SDK.
- Destructor noexcept, no throw, no unbounded wait. Explicit shutdown reports errors; if
  quiescence cannot be proven, quarantine the resource and report FW recovery.
- Validate size/overflow before allocate/map/index/crop.

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

- A single owner serializes the state of each source/tracker/temporal feature.
- Camera callbacks only validate/enqueue; vendor callbacks only enqueue completion.
- No detached thread, polling busy loop or unbounded queue.
- Queues must have capacity + overflow policy + stop policy + metrics.
- Do not hold a mutex while calling RPC/blocking SDK/I/O/outward callbacks.
- Fixed lock order; atomics for small data, not as a replacement for complex lock state.
- A timeout is the end of the wait, and does NOT mean the hardware job is cancelled.
- Mark completed only after real completion, or after recovery guarantees DMA has stopped.
- Use steady/monotonic clocks for duration/deadline; UTC only for correlation/export.

## 7. Error handling and API

- status/result<T> for expected errors; no bool that loses the cause at an external
  boundary.
- Distinguish invalid_argument, unsupported, incompatible_model, resource_exhausted,
  timeout, cancelled, source_lost, backend_fault, unauthorized.
- [[nodiscard]] for results that must not be ignored.
- No exception across a C ABI or vendor callback; catch and convert to status.
  Internally use RAII and controlled exceptions; do not throw on every normal frame.
- No assert for IPC/model/user data that can be wrong; validate and return an error.
- Public functions document input/output, ownership, thread safety, blocking/deadline,
  completion semantics, errors, pre/postconditions.
- Do not expose std::string/vector/exception/C++ object layout across a binary plugin ABI.
- Export opaque handles + fixed-width fields + struct_size + abi_major/minor;
  allocator/release must have the same owner, do not free another module's memory.

## 8. Image/tensor correctness

- Each plane has offset/stride/size; do not assume width == stride.
- NV12/NV21/RGB/BGR, color range/matrix, crop/rotate/letterbox are explicit metadata.
- Keep the source-to-tensor transform to map boxes/landmarks; test border/odd ROI.
- Each tensor has its own dtype/layout/rank/dims/strides/quantization.
- Do not assume every output is float32 or the same type; do not cast everything for NMS
  convenience.
- Golden tests must cover interpolation, rounding, clipping, pad, channel order.
- CPU normalization/postprocess may avoid OpenCV when there is budget and a reason; do not
  label a CPU loop as hardware.

## 9. Build, dependency and security

- CMake target-scoped compile options/includes/libs; no global link_directories.
- Do not pull headers from a sibling repo with ../../; the contract SDK/version is the
  dependency.
- Vendor libraries only in the adapter target; the logic-only eSDK profile does not need
  the vendor SDK. Do not configure/build C++ with the host compiler.
- Pin toolchain/sysroot/compiler/runtime; do not download dependencies implicitly at build.
- Warnings as errors on self-written source in CI; third-party is scoped separately.
- Choose -Wall -Wextra -Wpedantic and check conversions per compiler/toolchain.
- No -ffast-math by default; do not change numerical semantics without golden evidence.
- Library/model paths come from a verified manifest + allowed root, not raw remote input.
- Do not log images, face embeddings, license key/token; bound parser/message sizes.
- Feature/attribute/output permissions are checked at runtime and at the output boundary.

## 10. Testing and enforcement

- clang-format checks whitespace, it does NOT prove names or ownership are correct.
- Naming needs an AST checker that understands declaration owner/override/exceptions;
  regex only does preliminary lint.
- A structural filename/include/CMake checker exists; the AST registry/naming checker is
  not implemented; the structural/eSDK CI workflow exists but the eSDK job depends on the
  runner/ESDK_ROOT. The structural checker does not prove symbol naming is correct.
- A reviewer checks 100% of function/parameter/global names while no AST checker exists.
- Unit + contract + golden + replay; board tests are required for DMA/SDK/performance.
- ASan/UBSan/TSan on supported targets; a host mock does not prove device sync.
- Each PR has a checklist; ABI/sync/security need lead + module owner.
- Change rules via a documentation/ADR PR, not a private exception in one source file.

## See also

- [Documentation style](documentation_style.md), [naming registry](naming_registry.md)
- [Review checklist](review_checklist.md), [AGENTS.md](../../AGENTS.md)
