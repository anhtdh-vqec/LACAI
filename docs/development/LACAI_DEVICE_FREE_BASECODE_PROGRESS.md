# Device-free basecode progress

Tracks [LACAI_DEVICE_FREE_BASECODE_PLAN.md](LACAI_DEVICE_FREE_BASECODE_PLAN.md). `done`
means source, tests and docs are in the tree and both eSDK QEMU configurations run; `module`
means the device-free unit is delivered but not yet wired onto the production path;
`partial` and `todo` are as named. See the
[review resolution log](review_codebase_1309_resolution.md) for the previous review.

Evidence: neutral and expanded eSDK QEMU both `100%` at the time of the last update.

Board (2026-09-14, QCS6490): the target came online. 81/81 test binaries pass natively; the
service harness and `--mode production --platform fake` route two sources and `--platform
qualcomm` fails closed; the QNN DSP V68 unit test passes; and the owned QNN engine executes
SCRFD/YOLOv8n on HTP with output byte-identical to `qnn-net-run` (latency seed SCRFD ~5.2 ms,
YOLOv8n ~12.1 ms). See [QCS6490 target](../testing/qsc6490_board.md). This moves §5/§6
engine execution from "module" toward qualified, but neither the worker nor the pool is
wired to the running path yet, and live FW camera/DMA/encoder remain open.

| § | Work item | Status | Commit / note |
|---|---|---|---|
| 3/§4 | Camera release dispatcher | **done** | `e84f1c1` |
| 5 | Bounded inference worker | **done** | inference worker + per-source session worker; supervisor async mode (opt-in `use_session_workers_`) wired and tested |
| 6 | Tensor pool | **module** | this change; output/input wiring pending |
| 7 | Production composition root + fake platform | **done** | `--mode production --platform fake` runs E2E; qualcomm/unset fails closed; fake owners in `vqec_vision_fake_platform` |
| 8 | Recovery/health state machine | partial | supervisor fault channel `a33a87e`; per-source backoff/retry-budget controller added (this change); reconnect wiring pending |
| 9 | Model/package admission hardening | **done** | catalog/manifest/capability/policy/model-class validators + negative tests; multi-input rejected at activation before frames |
| 10 | Scheduler QoS semantics | **done** | policy + one-slot mailbox for latest_wins/replace_pending; must_process_once/event_triggered rejected |
| 11 | Secondary/ROI inference contract | **done** | neutral contract + bounded scheduler + fake backend + correlation tests this change |
| 12 | Preprocess conformance suite | **done** | conformance suite this change (solid/gradient/stride/offset/aspect/order/norm/quant/clip/reject) |
| 13 | Real CPU decoder | **done** | configurable dense anchor-free decoder + NMS + letterbox inverse this change |
| 14 | Reference tracker | **done** | this change |
| 15 | Feature/event engine tests | **done** | reference ROI/dwell/line/count processors this change |
| 16 | Output fake pipeline | **done** | fake encoder + bounded ring sink this change |
| 17 | Metrics & tracing | partial | supervisor/worker/pool snapshots + executor counters printed by the service; histograms/transport pending |
| 18 | CMake modularization | **done** | per-module CMakeLists (core/perception/runtime/outputs/app/adapters/tests) with unchanged target names and centralized warning flags; root ~114 lines |
| 19 | Warnings/sanitizers/static analysis | partial | WERROR + SANITIZE + host-sanitizers job; advisory `.clang-tidy` + CI job added; not yet enforced |
| 20 | Wire/parser fuzzing | partial | wire + output-manifest fuzz harnesses + nightly `fuzz` CI job; result decoder pending |
| 21 | FD/resource leak tests | **done** | repeated worker start/stop + pool cycles invariant test (no FD growth here; camera FD soak is board) |
| 22 | Deterministic time abstraction | partial | steps take injected monotonic time; recovery controller is sleep-free; service main still reads the clock directly |
| 23 | Explicit epoch semantics | **done** | worker/secondary stale-epoch flags; pump drops parked inputs on epoch change |
| 24 | State machine invariants as tests | **done** | worker submitted==outcomes, pool capacity==free+live tested over cycles |
| 25 | C++ naming cleanup | deferred | AGENTS mandates current scheme; needs lead/ADR |
| 26 | Allocation instrumentation | **done** | global new counter asserts zero steady-state pool allocation this change |
| 27 | Small object/metadata optimization | deferred | plan conditions this on host profiling; staged harness prints exist, no measured hot spot yet |
| 28 | Fake latency/failure injection | **done** | latency/failure executor stress test this change |
| 29 | Device-free microbenchmarks | **done** | pool, worker and dense-decoder throughput printed by the harness/tests |
| 30 | Documentation capability matrix | **done** | `capability_matrix.md` added this change |

## Next

1. Production composition root with a fake platform owner and a no-fallback E2E test:
   `--mode production --platform fake` runs, `--platform qualcomm` fails closed.
2. Wire the bounded inference worker into the pump/session and the tensor pool into
   preprocess/output so the running path is non-blocking and steady-state allocation-free.
3. Wire the recovery controller into a source reconnect loop; add leak and invariant tests.
4. One real decoder, secondary/ROI contract, fake output pipeline.
5. Preprocess conformance suite, CMake split, sanitizers, fuzzing, benchmarks.
