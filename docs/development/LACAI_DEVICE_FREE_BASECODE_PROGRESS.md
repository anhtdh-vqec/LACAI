# Device-free basecode progress

Tracks [LACAI_DEVICE_FREE_BASECODE_PLAN.md](LACAI_DEVICE_FREE_BASECODE_PLAN.md). `done`
means source, tests and docs are in the tree and both eSDK QEMU configurations run; `module`
means the device-free unit is delivered but not yet wired onto the production path;
`partial` and `todo` are as named. See the
[review resolution log](review_codebase_1309_resolution.md) for the previous review.

Evidence: neutral and expanded eSDK QEMU both `100%` at the time of the last update.

| § | Work item | Status | Commit / note |
|---|---|---|---|
| 3/§4 | Camera release dispatcher | **done** | `e84f1c1` |
| 5 | Bounded inference worker | **module** | `bee8c8e`; pump wiring pending |
| 6 | Tensor pool | **module** | this change; output/input wiring pending |
| 7 | Production composition root + fake platform | partial | `--mode production` fails closed; no fake production owner yet |
| 8 | Recovery/health state machine | partial | supervisor fault channel `a33a87e`; per-source backoff/retry-budget controller added (this change); reconnect wiring pending |
| 9 | Model/package admission hardening | partial | capability/metadata/model-class gates (`b20ae11`, `52fe276`, `595ed08`) |
| 10 | Scheduler QoS semantics | partial | explicit policy, unsupported rejected `d469495`; queue semantics pending |
| 11 | Secondary/ROI inference contract | todo | deferred; needs ADR |
| 12 | Preprocess conformance suite | partial | CPU reference processor exists; golden vectors pending |
| 13 | Real CPU decoder | todo | needs model metadata/decoder implementation |
| 14 | Reference tracker | **done** | this change |
| 15 | Feature/event engine tests | **done** | reference ROI/dwell/line/count processors this change |
| 16 | Output fake pipeline | todo | encoded/overlay helpers exist |
| 17 | Metrics & tracing | partial | counters in supervisor/worker/pool; no unified sink |
| 18 | CMake modularization | todo | root CMake still monolithic |
| 19 | Warnings/sanitizers/static analysis | partial | `VQEC_VISION_AI_WERROR`; no sanitizer job |
| 20 | Wire/parser fuzzing | todo | — |
| 21 | FD/resource leak tests | todo | planned with worker/pool |
| 22 | Deterministic time abstraction | partial | steps take injected monotonic time; recovery controller is sleep-free; service main still reads the clock directly |
| 23 | Explicit epoch semantics | partial | worker stale-epoch flag; broader audit pending |
| 24 | State machine invariants as tests | todo | — |
| 25 | C++ naming cleanup | deferred | AGENTS mandates current scheme; needs lead/ADR |
| 26 | Allocation instrumentation | todo | pool provides counters; test allocator pending |
| 27 | Small object/metadata optimization | todo | only after profiling |
| 28 | Fake latency/failure injection | partial | gate executor in worker test |
| 29 | Device-free microbenchmarks | todo | — |
| 30 | Documentation capability matrix | partial | `esdk_configuration_matrix.md`, execution-policy inventory |

## Next

1. Production composition root with a fake platform owner and a no-fallback E2E test:
   `--mode production --platform fake` runs, `--platform qualcomm` fails closed.
2. Wire the bounded inference worker into the pump/session and the tensor pool into
   preprocess/output so the running path is non-blocking and steady-state allocation-free.
3. Wire the recovery controller into a source reconnect loop; add leak and invariant tests.
4. One real decoder, secondary/ROI contract, fake output pipeline.
5. Preprocess conformance suite, CMake split, sanitizers, fuzzing, benchmarks.
