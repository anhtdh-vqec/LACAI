# LACAI — Device-Free Basecode Completion & Optimization Plan

Scope: dated planning/review record. Current delivery is governed by
[implementation status](implementation_status.md);
current priorities are in [alignment review](architecture_alignment_review.md).

> Scope: các việc có thể hoàn thành **không cần QCS6490 device thật**, nhằm đưa LACAI tới trạng thái basecode v1 sạch, ổn định, testable và sẵn sàng cho hardware qualification sau này.
>
> Không bao gồm: HTP throughput thật, thermal, DMA hardware-completion proof, camera sensor/CamX qualification, hardware encoder performance, multi-camera performance trên silicon.

---

## 1. Mục tiêu

Mục tiêu của giai đoạn device-free là đưa codebase tới trạng thái:

- architecture freeze ở mức cần thiết;
- không còn known lifecycle bug nghiêm trọng trong camera/QNN/runtime;
- production composition có thể wire bằng mock/fake backend nhưng **không silently fallback**;
- QNN execution không block control/source executor;
- steady-state hot path giảm allocation;
- preprocess, decoder, tracker, feature, scheduler có deterministic contract;
- source failure/recovery có state machine và test rõ ràng;
- codebase dễ maintain hơn khi team lớn;
- CI có thể chứng minh phần lớn contract bằng native/QEMU/fake backends;
- khi có device thật, phần việc còn lại chủ yếu là **hardware qualification**, không phải redesign architecture.

---

# 2. Definition of Done cho “Device-Free Basecode v1”

Basecode có thể coi là hoàn thiện ở mức không cần device khi tất cả các điều sau đúng:

- [ ] Camera buffer release không còn phụ thuộc vào `send()` trực tiếp trong destructor.
- [ ] Source/session lifecycle có explicit state machine, reconnect/backoff policy và epoch transition test.
- [ ] QNN/backend blocking execution đã được tách khỏi runtime/control executor.
- [ ] Runtime có bounded execution queue và backpressure policy.
- [ ] Tensor input/output có reusable pool; hot path không allocate vô hạn theo frame.
- [ ] Reference preprocess có conformance suite đủ mạnh để làm oracle cho Qualcomm implementation sau này.
- [ ] Model admission validate đầy đủ static shape, dtype, quantization, input/output identity trước hot path.
- [ ] Scheduler có explicit QoS semantics thay vì chỉ `busy => skip`.
- [ ] Có contract cho secondary/ROI inference để feature code không gọi backend trực tiếp.
- [ ] Production composition root tồn tại và có thể chạy với fake platform owner.
- [ ] Production mode tuyệt đối không fallback sang fixture/harness.
- [ ] Runtime health/event channel tách khỏi step return code.
- [ ] Source failure không làm chết source khác, nhưng lỗi vẫn observable.
- [ ] Renderer/encoder/output contracts có fake backend và lifecycle tests.
- [ ] Root CMake được chia nhỏ theo module.
- [ ] Naming convention C++ được đơn giản hóa cho code mới.
- [ ] Sanitizer/static-analysis/fuzzing chạy trong CI.
- [ ] Wire protocol, lifecycle state machine và resource-limit tests đủ mạnh.
- [ ] Có benchmark device-free cho scheduler, queue, allocation và CPU preprocess để phát hiện regression.
- [ ] Documentation phản ánh đúng implementation, không advertise capability chưa có.

---

# 3. Priority Summary

| Priority | Work item | Device needed? | Mục tiêu |
|---|---|---:|---|
| P0 | Camera release dispatcher | No | Fix lifecycle correctness |
| P0 | Bounded inference worker | No | Không block scheduler/control loop |
| P0 | Production composition root | No | Hoàn thiện runtime wiring |
| P0 | Runtime health/recovery state machine | No | Fault isolation + observability |
| P0 | Tensor memory pool | No | Giảm allocation hot-path |
| P0 | Model/package admission hardening | No | Fail early, fail closed |
| P1 | Inference QoS policies | No | Scheduler semantics rõ ràng |
| P1 | Secondary/ROI inference contract | No | Chuẩn bị cascade AI |
| P1 | Preprocess conformance suite | No | Oracle cho Qualcomm backend |
| P1 | Decoder/tracker/feature plugin contracts | No | Product extensibility |
| P1 | Output fake pipeline | No | Test end-to-end lifecycle |
| P1 | CMake modularization | No | Maintainability |
| P1 | Sanitizers/static analysis | No | Correctness |
| P1 | Wire protocol fuzzing | No | Robustness/security |
| P1 | Deterministic tracing/metrics | No | Debuggability |
| P2 | Naming cleanup | No | Readability |
| P2 | Allocation micro-optimization | No | Performance hygiene |
| P2 | Documentation/code generation cleanup | No | Reduce drift |
| P2 | Device-free microbenchmarks | No | Regression detection |

---

# 4. P0 — Camera Buffer Release Dispatcher

## Vấn đề

Hiện release token/ACK của camera buffer không nên phụ thuộc vào non-blocking socket I/O trực tiếp trong destructor của final frame owner.

Rủi ro:

```text
last owner destroyed
    ↓
send(MSG_DONTWAIT)
    ↓
EAGAIN / short send / socket error
    ↓
local FD closed
    ↓
producer không chắc đã nhận release
```

Destructor phải kết thúc ownership, không nên chịu trách nhiệm hoàn thành unreliable transport I/O.

## Việc cần làm

Tạo abstraction:

```text
frame owner destructor
    ↓
release_queue.enqueue(buffer_token)
    ↓
release_dispatcher
    ├── send success
    ├── EAGAIN → retry
    ├── peer closed → fail session
    └── deadline exceeded → recovery path
```

Đề xuất module:

```text
src/adapters/camera/
    frame_release_queue.*
    frame_release_dispatcher.*
```

## Contract nên có

```cpp
struct frame_release_request {
    uint64_t epoch;
    uint64_t buffer_id;
};

class frame_release_sink {
public:
    virtual status enqueue(frame_release_request) = 0;
};
```

## Policy

- bounded queue;
- no unbounded allocation;
- duplicate release detection;
- epoch validation;
- retry on transient error;
- permanent socket error transitions source/session to failed;
- destructor không throw;
- release queue full phải fail deterministic, không silently drop;
- stop/drain phải flush hoặc explicitly abandon epoch.

## Tests

- [ ] success path;
- [ ] `EAGAIN`;
- [ ] socket buffer full;
- [ ] producer disconnect;
- [ ] duplicate token;
- [ ] token from old epoch;
- [ ] stop while release pending;
- [ ] reconnect creates new epoch;
- [ ] queue full;
- [ ] 10k frame lease/release stress;
- [ ] multiple owners retain same frame;
- [ ] final owner releases exactly once.

## Acceptance criteria

- Không còn socket send trong frame destructor.
- Mỗi leased buffer có exactly-one terminal release outcome.
- Old epoch release không thể làm ảnh hưởng new epoch.
- Unit tests không leak FD/token.

---

# 5. P0 — Tách Blocking Inference khỏi Runtime Executor

## Vấn đề

Dù API bên ngoài có dạng submit/poll, backend sync vẫn có thể block trực tiếp trong `submit_tensors()` hoặc tương đương.

Điều này khiến:

```text
source scheduler
    ↓
model submit
    ↓
blocking inference call
    ↓
other sources wait
```

Với multi-source runtime, đây là architectural bottleneck ngay cả trước khi có device thật.

## Giải pháp device-free

Không cần QNN async API ngay.

Tạo execution layer:

```text
runtime/control thread
    ↓
bounded_inference_queue
    ↓
inference_worker
    ↓
sync backend execute
    ↓
completion_queue
```

Backend fake có thể sleep 1/5/20/100 ms để mô phỏng latency.

## Interface gợi ý

```cpp
struct inference_job {
    source_id source;
    model_id model;
    frame_ticket ticket;
    tensor_slot input;
    deadline deadline;
    qos_policy qos;
};

struct inference_completion {
    job_id id;
    status result;
    output_slot output;
};
```

## Requirements

- bounded jobs;
- bounded completions;
- cancellation;
- stop/drain semantics;
- source isolation;
- model isolation;
- no blocking backend call trên control thread;
- no detached thread;
- no unbounded thread-per-frame;
- worker count configurable;
- executor ownership/lifetime explicit.

## Tests

- [ ] backend latency simulation;
- [ ] worker queue full;
- [ ] source A slow, source B vẫn progress;
- [ ] stop during execute;
- [ ] shutdown with pending jobs;
- [ ] completion arrives after source epoch changed;
- [ ] backend throws/fails;
- [ ] worker death/fault;
- [ ] fairness;
- [ ] latest-wins/drop policy;
- [ ] no deadlock under repeated start/stop.

## Acceptance criteria

Control/scheduler loop latency không phụ thuộc trực tiếp vào backend inference duration.

---

# 6. P0 — Tensor Input/Output Memory Pool

## Vấn đề

Per-frame `std::vector::resize()`, allocation/free cho tensor input/output sẽ gây allocation churn và làm khó đo latency ổn định.

## Mục tiêu

Steady state:

```text
startup
  ↓
allocate fixed tensor slots
  ↓
FREE
  ↓
IN_USE_BY_PREPROCESS
  ↓
IN_USE_BY_BACKEND
  ↓
READY_FOR_DECODER
  ↓
FREE
```

## Thiết kế

```cpp
enum class tensor_slot_state {
    free,
    preprocessing,
    submitted,
    completed,
    decoding,
};

struct tensor_slot {
    slot_id id;
    tensor_spec spec;
    aligned_buffer storage;
};
```

Pool phải:

- preallocate;
- bounded;
- alignment configurable;
- immutable tensor spec after prepare;
- detect double-release;
- support poison/debug mode;
- expose high-watermark metrics.

## Device-free tests

- [ ] slot acquire/release;
- [ ] pool exhaustion;
- [ ] double free;
- [ ] use-after-release detection in debug build;
- [ ] concurrent acquire/release;
- [ ] output decoder retains buffer;
- [ ] session stop with slots outstanding;
- [ ] model reload invalidates old pool safely.

## Acceptance criteria

Sau warm-up, normal inference path không tạo allocation proportional với frame count.

Có thể kiểm bằng custom allocator counter/test hook.

---

# 7. P0 — Production Composition Root

## Mục tiêu

Hiện production mode cần có composition thật, nhưng vẫn có thể dùng fake platform implementation khi không có device.

Không được:

```text
production backend fail
    ↓
silently fallback to fixture
```

## Structure đề xuất

```text
production_composition
├── source_factory
├── image_processor_factory
├── inference_backend_factory
├── decoder_registry
├── tracker_registry
├── feature_registry
├── output_backend_factory
├── health_sink
└── metrics_sink
```

Tách:

```text
composition contract
platform owner
runtime executor
```

## Fake production owner

Tạo fake implementation dùng trong CI:

```text
fake_camera_source
fake_image_processor
fake_inference_backend
fake_encoder
fake_ring_sink
```

Nhưng chạy qua **production composition**, không phải harness composition.

Mục đích là test wiring thật.

## Acceptance criteria

Có thể chạy:

```bash
lacai --mode production --platform fake
```

và đi xuyên toàn bộ production composition mà không dùng fixture fallback.

Production Qualcomm owner chưa có thì `--platform qualcomm` phải fail rõ ràng.

---

# 8. P0 — Runtime Health & Recovery State Machine

## Vấn đề

Return code `pending` không đủ để biểu diễn trạng thái source lỗi nhưng các source khác vẫn chạy.

## Cần tách 3 lớp

### Step result

```text
did_work
pending
stopping
```

### Health state

```text
healthy
degraded
reconnecting
failed
stopped
```

### Event stream

```text
source_disconnected
source_reconnected
model_failed
queue_overflow
release_timeout
backend_fault
```

## State machine đề xuất

```text
CREATED
  ↓
STARTING
  ↓
RUNNING
  ├── transient fault → RECOVERING → RUNNING
  ├── stop            → DRAINING → STOPPED
  └── fatal fault     → FAILED
```

## Recovery policy

Configurable:

```cpp
struct recovery_policy {
    uint32_t max_retries;
    milliseconds initial_backoff;
    milliseconds max_backoff;
    double multiplier;
};
```

## Device-free tests

- [ ] source disconnect/reconnect;
- [ ] repeated transient failure;
- [ ] max retry reached;
- [ ] stop while recovering;
- [ ] stale completion after epoch rollover;
- [ ] source A recovery không block source B;
- [ ] event ordering deterministic;
- [ ] restart resets only source-scoped resources.

---

# 9. P0 — Model Admission & Package Validation Hardening

Mọi unsupported condition phải fail **trước hot path**.

## Validate trước activation

- model ID uniqueness;
- artifact existence;
- artifact checksum;
- model class supported;
- exactly one image input nếu base v1 chỉ support một input;
- static dimensions;
- static batch;
- supported dtype;
- supported quantization type;
- supported color format;
- output tensor identity;
- decoder compatibility;
- preprocess compatibility;
- expected tensor byte size;
- scheduler cadence valid;
- resource limits;
- feature binding validity.

## Không được

- infer tensor meaning bằng position nếu package có thể khai báo tên;
- silently assume NHWC/NCHW;
- silently assume uint8/float32;
- silently accept dynamic dimensions;
- silently truncate model count;
- silently choose fallback decoder.

## Package schema

Nên có version:

```yaml
schema_version: 1

model:
  id: person_detector
  backend: qnn

input:
  layout: NHWC
  dtype: uint8
  width: 640
  height: 640
  color: RGB
  quantization:
    type: per_tensor
    scale: ...
    zero_point: ...

preprocess:
  resize: letterbox
  matrix: bt601_limited

output:
  decoder: yolo_v8
```

## Tests

Property/negative test cho từng invalid field.

---

# 10. P1 — Scheduler QoS Semantics

Không phải mọi workload đều có semantics:

```text
busy → skip
```

Cần explicit policy.

## Suggested QoS

```cpp
enum class inference_qos {
    latest_wins,
    drop_if_busy,
    replace_pending,
    must_process_once,
    event_triggered,
};
```

## Ví dụ

### Detection

```text
latest_wins
```

Old frame không còn giá trị.

### OCR

```text
must_process_once
```

Event hiếm nhưng không được miss.

### Secondary classifier

```text
replace_pending
```

Có thể thay ROI cũ bằng ROI mới cùng object.

## Tests

- queue full;
- replace semantics;
- deadline expired;
- fairness;
- cadence + busy;
- event task priority.

---

# 11. P1 — Secondary / ROI Inference Contract

Basecode camera AI gần như chắc chắn sẽ cần cascade:

```text
detector
   ↓
ROI
   ↓
face / plate / pose / embedding / OCR
```

Không nên để feature module gọi QNN trực tiếp.

## Contract tối thiểu

```cpp
struct inference_task {
    task_id id;
    source_id source;
    frame_id frame;
    model_id model;
    optional<roi> region;
    optional<track_id> track;
    inference_qos qos;
    deadline deadline;
};
```

## Result

```cpp
struct inference_task_result {
    task_id id;
    source_id source;
    frame_id frame;
    optional<track_id> track;
    model_id model;
    status state;
    decoded_result result;
};
```

## Base v1 chưa cần implement full cascade

Chỉ cần:

- contract;
- queue;
- fake secondary backend;
- result correlation;
- lifecycle test.

---

# 12. P1 — Preprocess Conformance Suite

Reference CPU processor nên trở thành **golden oracle**.

## Test vectors cần có

- NV12 solid color;
- gradient;
- odd/even dimensions;
- non-default stride;
- non-zero plane offset;
- aspect ratio landscape;
- portrait;
- square;
- letterbox top/bottom;
- letterbox left/right;
- RGB/BGR;
- normalization;
- uint8 quantization;
- int8 quantization;
- float32;
- clipping;
- rounding boundary;
- invalid plane size;
- invalid stride;
- unsupported transform.

## Golden outputs

Lưu test vectors nhỏ, deterministic.

Không cần frame ảnh lớn.

## Mục đích

Sau này Qualcomm/FastCV implementation chỉ cần chạy cùng suite:

```text
reference processor
        vs
Qualcomm processor
```

và compare theo tolerance.

---

# 13. P1 — Decoder Contract & Real CPU Decoders

Không cần device để implement decoder thật.

Nên hoàn thiện ít nhất 1–2 decoder phổ biến:

- YOLO detection;
- SCRFD/face detector hoặc generic SSD;
- optional classification.

## Decoder phải độc lập backend

Input:

```text
tensor view + model metadata
```

Output:

```text
neutral detections
```

Không được dependency trực tiếp QNN type.

## Tests

- normal output;
- zero detection;
- max detections;
- malformed tensor;
- NaN/Inf;
- threshold edge;
- NMS;
- letterbox reverse mapping;
- image bounds clipping.

---

# 14. P1 — Reference Tracker

Không cần device để có ít nhất một tracker thật.

Có thể implement simple baseline:

```text
IoU matching
+
Hungarian/greedy
+
track age
+
lost timeout
```

Không cần ngay DeepSORT/ByteTrack đầy đủ.

Mục tiêu:

- validate perception-to-feature dataflow;
- validate track identity lifecycle;
- enable secondary inference correlation;
- test source restart/epoch behavior.

## Acceptance criteria

Tracker deterministic với prerecorded synthetic detections.

---

# 15. P1 — Feature/Event Engine Tests

Tạo ít nhất các processor neutral:

- ROI enter/exit;
- line crossing;
- dwell time;
- object count;
- event cooldown/dedup.

Device-free hoàn toàn.

Mục tiêu là chứng minh feature API đủ dùng mà không chọc xuống backend.

---

# 16. P1 — Output Pipeline Fake Backend

Trước khi có hardware renderer/encoder, cần fake backend chạy full lifecycle.

```text
perception result
    ↓
overlay commands
    ↓
fake renderer
    ↓
fake encoder
    ↓
fake FW ring sink
```

## Fake renderer

Có thể chỉ serialize overlay commands, không cần render pixel.

## Fake encoder

Sinh fake encoded AU:

```text
header + frame id + timestamp
```

## Fake ring sink

Bounded ring semantics:

- writer ownership;
- capacity;
- wraparound;
- backpressure;
- consumer lag;
- restart;
- epoch.

## Tests

End-to-end source → model → decode → track → feature → output.

---

# 17. P1 — Metrics & Tracing

Instrumentation nên hoàn thiện trước device để sau này lên board chỉ việc export.

## Counters

```text
frames_received_total
frames_released_total
frame_release_retry_total
frame_release_failed_total

frames_skipped_cadence_total
frames_skipped_busy_total
frames_dropped_queue_full_total

inference_submitted_total
inference_completed_total
inference_failed_total

source_disconnect_total
source_reconnect_total

tensor_pool_exhausted_total
```

## Gauges

```text
camera_leases_outstanding
inference_queue_depth
completion_queue_depth
tensor_pool_used
source_last_frame_age_ms
```

## Histograms

```text
source_step_latency
preprocess_latency
queue_wait_latency
backend_latency
decode_latency
feature_latency
end_to_end_latency
frame_lease_hold_time
```

Device-free fake backend có thể tạo deterministic latency để verify histogram.

---

# 18. P1 — Root CMake Modularization

Root `CMakeLists.txt` không nên tiếp tục phình.

## Mục tiêu

```text
CMakeLists.txt
src/
  core/CMakeLists.txt
  app/CMakeLists.txt
  adapters/
    camera/CMakeLists.txt
    qualcomm/CMakeLists.txt
  perception/CMakeLists.txt
  features/CMakeLists.txt
  output/CMakeLists.txt
tests/
  CMakeLists.txt
```

Top-level:

```cmake
add_subdirectory(src/core)
add_subdirectory(src/app)
...
```

## Không thay đổi architecture

Chỉ giảm friction.

## Acceptance criteria

- same target names hoặc migration rõ;
- same CI output;
- dependency graph đơn giản hơn;
- module có owner rõ;
- compile options centralized.

---

# 19. P1 — Compiler Warnings, Sanitizers, Static Analysis

## Compiler flags

CI build ít nhất:

```text
-Wall
-Wextra
-Wpedantic
-Wconversion
-Wsign-conversion
-Wshadow
-Wnon-virtual-dtor
-Wold-style-cast
-Woverloaded-virtual
-Wnull-dereference
-Wdouble-promotion
-Wformat=2
```

Có thể rollout dần để tránh noisy migration.

## Sanitizers

Host/native CI:

- ASan;
- UBSan;
- TSan nếu threading code ổn định;
- LSan nếu platform hỗ trợ.

## Static analysis

- clang-tidy;
- cppcheck optional;
- include-what-you-use optional.

### clang-tidy focus

- dangling lifetime;
- move/copy misuse;
- narrowing;
- unchecked optional;
- virtual destructor;
- concurrency;
- RAII;
- modernize only where useful.

---

# 20. P1 — Fuzzing Wire Protocol & Parsers

Các input boundary nên fuzz:

- camera wire header;
- ancillary metadata parser;
- model/package parser;
- result decoder;
- configuration loader;
- ring message parser.

## LibFuzzer/AFL target

Ưu tiên:

```text
camera packet decode
model manifest parse
tensor decoder parse
```

## Assertions

Fuzzer không được gây:

- crash;
- UB;
- FD leak;
- unbounded allocation;
- infinite loop;
- state corruption.

---

# 21. P1 — FD/Resource Leak Tests

Test lặp:

```text
start
receive
process
stop
restart
```

10k–100k iterations với fake FD/source.

Theo dõi:

- `/proc/self/fd`;
- allocator count;
- outstanding lease;
- tensor slots;
- threads;
- queue size.

Acceptance:

```text
steady resource count
```

sau mỗi cycle.

---

# 22. P1 — Deterministic Time Abstraction

Nếu scheduler/recovery/cooldown dùng `steady_clock::now()` trực tiếp khắp code, nên inject time source.

```cpp
class monotonic_clock {
public:
    virtual uint64_t now_ns() const = 0;
};
```

Production:

```text
steady_clock
```

Test:

```text
fake_clock
```

Lợi ích:

- cadence tests deterministic;
- retry/backoff tests không sleep;
- timeout tests nhanh;
- event cooldown tests chính xác.

---

# 23. P1 — Explicit Epoch Semantics

Epoch cần là first-class lifecycle boundary cho:

- camera reconnect;
- model reload;
- source restart;
- output ring restart;
- stale completion.

Rule:

```text
result(epoch=N)
```

không được attach vào:

```text
source(epoch=N+1)
```

## Tests

- stale frame release;
- stale inference completion;
- stale feature event;
- stale ring output;
- delayed worker completion after restart.

---

# 24. P1 — State Machine Invariants

Nên encode invariants thay vì chỉ comment.

Ví dụ:

```text
STOPPED => outstanding_jobs == 0
STOPPED => outstanding_leases == 0
RUNNING => source != nullptr
DRAINING => no new submission
FAILED => no new acquisition
```

Debug build:

```cpp
assert(invariant());
```

Unit test toàn bộ transitions.

---

# 25. P2 — C++ Naming Cleanup

Không nên rename toàn repo ngay.

Nhưng nên sửa convention cho code mới:

### C ABI / exported symbols

Giữ prefix dài:

```cpp
vqec_vision_ai_qcom_...
```

### C++ methods

Ưu tiên:

```cpp
graph.submit_tensors()
pump.step()
session.request_stop()
source.health()
```

thay cho:

```cpp
graph.vqec_vision_ai_ports_infgr_submit_tensors()
```

## Migration

- code mới dùng convention mới;
- rename module khi đang touch;
- không mass rename gây merge conflict.

---

# 26. P2 — Allocation Instrumentation

Thêm test allocator/counter:

```text
allocations/frame
bytes allocated/frame
max live allocations
```

Cho:

- preprocess;
- inference;
- decoder;
- tracking;
- feature;
- output.

Mục tiêu trước device:

```text
steady-state allocations/frame ≈ 0
```

ở core scheduler/inference plumbing.

---

# 27. P2 — Small Object / Metadata Optimization

Chỉ làm sau profiling host.

Candidate:

- reserve vectors;
- `std::array` khi tensor count bounded;
- fixed-capacity containers cho model slots;
- avoid string parsing hot path;
- intern model IDs;
- use immutable shared model metadata;
- avoid copying tensor spec;
- move result blobs;
- small-buffer optimization cho tiny metadata.

Không premature optimize pixel path bằng tay nếu chưa có benchmark.

---

# 28. P2 — Fake Latency & Failure Injection Framework

Rất hữu ích khi chưa có device.

Fake backend config:

```yaml
latency_ms: 15
jitter_ms: 5
fail_every_n: 0
hang_every_n: 0
queue_capacity: 4
```

Fake source:

```yaml
fps: 30
disconnect_after_frames: 100
reconnect_after_ms: 500
malformed_every_n: 0
```

Dùng để stress:

- scheduler;
- recovery;
- fairness;
- queue;
- stop/drain;
- timeout.

---

# 29. P2 — Device-Free Performance Benchmarks

Không chứng minh QCS6490 performance, nhưng phát hiện regression codebase.

Benchmark:

### Scheduler throughput

```text
N sources × M models
```

fake backend latency configurable.

### Queue contention

1/2/4/8 worker threads.

### Tensor pool

acquire/release throughput.

### Preprocess CPU reference

fixed prerecorded NV12 frames.

### Decoder

synthetic tensor outputs.

### Tracker

10/100/1000 objects.

### Wire parser

packets/s.

Store baseline trong CI, flag regression > threshold.

---

# 30. P2 — Documentation Hygiene

Tách rõ ba trạng thái capability:

```text
Implemented
Tested on host/QEMU
Qualified on device
```

Không dùng từ “supported” nếu chỉ implemented.

Ví dụ matrix:

| Capability | Implemented | Host tested | QCS6490 qualified |
|---|---:|---:|---:|
| static single-input model | Yes | Yes | No |
| sync QNN execution | Yes | Mocked | No |
| async QNN | No | No | No |
| shared memory | No | No | No |
| DMA-BUF camera input | Yes | Protocol tested | No |
| hardware completion | No | No | No |

---

# 31. Work Packages Đề Xuất

## WP-01 — Lifecycle correctness

Bao gồm:

- camera release dispatcher;
- epoch semantics;
- stop/drain invariants;
- resource leak tests;
- reconnect state machine.

**Exit criteria:** lifecycle tests pass 10k repeated cycles.

---

## WP-02 — Non-blocking runtime execution

Bao gồm:

- bounded inference job queue;
- worker execution;
- completion queue;
- cancellation;
- fake latency backend;
- fairness tests.

**Exit criteria:** slow model không block source khác.

---

## WP-03 — Memory discipline

Bao gồm:

- tensor pool;
- no per-frame output allocation;
- allocation counters;
- pool exhaustion tests.

**Exit criteria:** steady-state core inference plumbing gần zero allocation/frame.

---

## WP-04 — Production composition

Bao gồm:

- production composition root;
- fake platform owner;
- fake output backend;
- no fallback guarantee;
- full E2E CI scenario.

**Exit criteria:**

```text
fake camera
→ preprocess
→ fake inference
→ real decoder
→ real tracker
→ feature
→ fake encoder
→ fake ring
```

chạy qua `--mode production`.

---

## WP-05 — Model/runtime contract

Bao gồm:

- model manifest schema;
- admission validation;
- preprocess conformance;
- decoder compatibility;
- negative tests.

**Exit criteria:** invalid model/package fail trước runtime hot path.

---

## WP-06 — Scheduler v1

Bao gồm:

- QoS enum;
- deadline;
- latest-wins;
- must-process-once;
- secondary task contract.

**Exit criteria:** scheduler behavior deterministic, documented và tested.

---

## WP-07 — Code quality

Bao gồm:

- CMake split;
- sanitizer builds;
- clang-tidy;
- fuzzing;
- naming convention update;
- documentation cleanup.

**Exit criteria:** CI quality gate ổn định.

---

# 32. Thứ Tự Triển Khai Đề Xuất

## Sprint A — correctness first

1. Camera release dispatcher.
2. Epoch + recovery state machine.
3. Deterministic fake clock.
4. Resource leak/stress tests.
5. Health/event channel.

Không tối ưu performance trước khi lifecycle ổn.

---

## Sprint B — runtime concurrency

1. Bounded inference queue.
2. Worker execution.
3. Completion queue.
4. Cancellation/drain.
5. Fake latency/failure backend.
6. Multi-source fairness tests.

---

## Sprint C — memory + model path

1. Tensor pool.
2. Model manifest/admission.
3. Preprocess conformance suite.
4. One real decoder.
5. One reference tracker.

---

## Sprint D — production composition

1. Production composition root.
2. Fake platform owner.
3. Fake output backend.
4. Full E2E production-mode CI.
5. Recovery/failure injection E2E.

---

## Sprint E — scheduler extensibility

1. QoS policy.
2. Secondary/ROI task contract.
3. Deadline handling.
4. Feature→secondary inference routing.
5. stale completion handling.

---

## Sprint F — cleanup & hardening

1. Split CMake.
2. Compiler warnings.
3. ASan/UBSan.
4. clang-tidy.
5. fuzz camera/package parsers.
6. allocation benchmark.
7. naming convention cleanup for touched modules.

---

# 33. Những Việc KHÔNG Nên Làm Trước Khi Có Device

Tránh mất thời gian vào các tối ưu không thể validate:

- tự viết custom SIMD preprocess cho ARM trước khi benchmark FastCV/QIM;
- tune worker count dựa trên giả định HTP concurrency;
- implement QNN async API chỉ vì API tồn tại;
- claim zero-copy chỉ vì dùng DMA-BUF;
- tune CPU affinity;
- tune URM/performance governor;
- optimize cache/memory bandwidth theo giả định;
- tune HTP performance profile;
- tune multi-camera FPS target;
- optimize hardware encoder;
- hardcode QCS6490-specific queue depth chưa có measurement;
- implement thermal mitigation policy dựa trên suy đoán.

Những việc này cần silicon evidence.

---

# 34. Những Việc Nên Chuẩn Bị Sẵn Để Lên Device Làm Nhanh

Dù không thể qualify hardware, có thể chuẩn bị interface/hook:

## Metrics hooks

- backend latency;
- HTP utilization placeholder;
- CPU utilization;
- temperature;
- DMA lease duration;
- encoder latency.

## Backend capability query

```cpp
backend_capabilities {
    async;
    shared_memory;
    max_inflight;
    supported_dtypes;
    supported_layouts;
};
```

## Performance configuration

Không hardcode.

```yaml
runtime:
  inference_workers: 1
  max_jobs: 4
  tensor_pool_slots: 4
```

Sau này board test chỉ thay config.

## Trace correlation ID

Một frame nên trace xuyên:

```text
source
→ preprocess
→ inference
→ decode
→ track
→ feature
→ output
```

Dùng:

```text
source_epoch + frame_id + job_id
```

---

# 35. CI Matrix Đề Xuất

## Build matrix

- GCC Debug
- GCC Release
- Clang Debug
- ASan + UBSan
- optional TSan

## Test matrix

### Neutral

Không Qualcomm headers/libs.

### Expanded fake backend

Enable production composition nhưng dùng fake adapter.

### QEMU/eSDK

Nếu toolchain có.

### Fuzz

Nightly hoặc scheduled.

## Required CI gates

- unit tests;
- integration tests;
- no sanitizer failure;
- no FD leak test failure;
- no allocation-regression vượt threshold;
- no wire fuzz crash;
- documentation capability matrix validation optional.

---

# 36. Suggested Test Pyramid

```text
             E2E production fake
             ──────────────────
           integration lifecycle
         ────────────────────────
       scheduler / pool / recovery
     ──────────────────────────────
   parser / state / decoder / feature
 ─────────────────────────────────────
           unit/property tests
```

Device-free CI nên cover càng nhiều lifecycle và failure càng tốt để board time chỉ dùng cho hardware behavior.

---

# 37. Critical Invariants Nên Được Viết Thành Test

## Camera

```text
leased == released + outstanding
```

## Tensor pool

```text
total_slots == free + busy
```

## Inference

```text
submitted == completed + cancelled + outstanding
```

## Source lifecycle

```text
STOPPED => outstanding == 0
```

## Epoch

```text
old_epoch_result never mutates new_epoch_state
```

## Output

```text
encoded frame belongs to exactly one source epoch
```

## Shutdown

```text
no threads + no FD + no tensor slot + no lease outstanding
```

---

# 38. Recommended “Architecture Freeze” Rule

Từ thời điểm này:

> Không thêm một abstraction mới nếu không có ít nhất một concrete use case hoặc test chứng minh abstraction đó giải quyết vấn đề hiện tại.

Cho phép abstraction mới khi:

- lifecycle correctness cần;
- production composition cần;
- secondary inference cần;
- hardware backend cần interface boundary.

Không nên thêm chỉ để “future-proof”.

---

# 39. Final Device-Free Exit Checklist

## Camera / IPC

- [ ] Release dispatcher.
- [ ] Bounded release queue.
- [ ] Retry/backpressure handling.
- [ ] Epoch-safe release.
- [ ] Disconnect/reconnect tests.
- [ ] FD leak tests.
- [ ] Wire fuzzing.

## Scheduler / Runtime

- [ ] Blocking backend moved to worker.
- [ ] Bounded job queue.
- [ ] Completion queue.
- [ ] Cancellation/drain.
- [ ] QoS policies.
- [ ] Deadline support.
- [ ] Multi-source fairness tests.
- [ ] Fake latency/failure injection.

## Model / Tensor

- [ ] Static model admission validation.
- [ ] Tensor identity cached.
- [ ] Tensor pool.
- [ ] No steady-state hot-path allocation.
- [ ] Preprocess golden tests.
- [ ] Decoder compatibility checks.

## Perception / Feature

- [ ] At least one real detector decoder.
- [ ] At least one reference tracker.
- [ ] Feature/event baseline implementations.
- [ ] Secondary inference contract.
- [ ] Result correlation tests.

## Output

- [ ] Fake renderer.
- [ ] Fake encoder.
- [ ] Fake bounded ring.
- [ ] Output backpressure tests.
- [ ] Output restart/epoch tests.

## Composition / Lifecycle

- [ ] Production composition root.
- [ ] Fake production platform.
- [ ] No fixture fallback.
- [ ] Health channel.
- [ ] Recovery state machine.
- [ ] Stop/drain invariants.

## Engineering Quality

- [ ] Split CMake.
- [ ] Compiler warning baseline.
- [ ] ASan.
- [ ] UBSan.
- [ ] clang-tidy.
- [ ] Fuzz critical parsers.
- [ ] Allocation instrumentation.
- [ ] Device-free microbenchmarks.
- [ ] Documentation capability matrix truthful.

---

# 40. Sau Khi Hoàn Thành File Này, Những Việc Còn Lại Bắt Buộc Cần QCS6490

Đây là phần **không nên cố kết luận bằng host/QEMU**:

1. Camera FD/DMA-BUF behavior thật.
2. Hardware DMA completion semantics.
3. Qualcomm preprocess/FastCV/QIM zero-copy path.
4. QNN/HTP execution correctness trên artifact thật.
5. QNN async/shared-memory qualification.
6. HTP concurrency.
7. Memory bandwidth.
8. CPU/HTP utilization.
9. Thermal throttling.
10. Hardware H264/H265 encoder.
11. Real FW ring integration.
12. Camera reconnect với FW thật.
13. 1/4/8/16 camera performance.
14. 24h+ soak.
15. Power/performance tuning.
16. URM/governor/affinity optimization.

Nếu device-free checklist phía trên đã hoàn thành, giai đoạn này sẽ chủ yếu là:

```text
qualify
measure
tune
fix hardware-specific bugs
```

thay vì:

```text
redesign runtime
rewrite ownership
change scheduler contract
```

Đó mới là mục tiêu chính của việc hoàn thiện basecode trước khi có device.

---

# 41. Recommended Immediate Top 10

Nếu cần rút gọn thành thứ tự làm ngay:

1. **Camera release dispatcher**
2. **Bounded inference worker**
3. **Production composition root**
4. **Recovery + health state machine**
5. **Tensor pool**
6. **Model admission/package validation**
7. **Preprocess conformance suite**
8. **One real decoder + one reference tracker**
9. **Fake E2E output pipeline**
10. **Sanitizer/fuzz/CMake cleanup**

Sau 10 mục này, codebase đủ tốt để ngừng mở rộng foundation và chuyển trọng tâm sang qualification trên QCS6490.

---

## Kết luận

Phần device-free còn lại của LACAI hiện không cần thêm nhiều “architecture”.

Trọng tâm nên chuyển sang:

```text
correct lifecycle
    +
bounded concurrency
    +
bounded memory
    +
explicit recovery
    +
production composition
    +
strong tests
```

Khi năm trụ cột này đã ổn, phần cứng Qualcomm chỉ còn là một concrete backend cần qualification, thay vì là nơi buộc application runtime phải thay đổi kiến trúc.
