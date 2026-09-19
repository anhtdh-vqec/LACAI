# QCS6490 board target

This document defines the only authorized LACAI development target by stable machine identity
and retains reproducible evidence for that target. A DHCP/LAN address change does not create a
different board.

**Status:** accepted — the authorized QCS6490 passed the declared five-minute full-workload
AI APP gate on 2026-09-18. Released-FW completion and product-release qualification remain
external gates. **Layer:** docs.
**Source:** `tools/board/`, `docs/testing/board_workspace.md`.

## Responsibility

- Authorize only machine ID `09c89b1858f54955a3d13f2767622448` for LACAI board work.
- Define evidence that must be collected from the exact staged candidate.
- Keep credentials outside Git, logs and command history.
- Separate native logic smoke, live preview correctness, throughput, resource and external
  owner acceptance.

## Target and access rules

The target is QCS6490 / Qualcomm Linux 1.8 with machine ID
`09c89b1858f54955a3d13f2767622448`. It is currently reachable through `lacai-home` at
`192.168.0.102`; `192.168.138.98` is the same board's previous LAN address and remains only in
dated evidence. Use `/opt/lacai` as the only board workspace. Resolve the alias, use
non-interactive key access first, then verify the machine ID before staging or execution:

```bash
getent hosts lacai-home
ssh -o BatchMode=yes -o ConnectTimeout=5 lacai-home \
  'test "$(cat /etc/machine-id)" = 09c89b1858f54955a3d13f2767622448'
```

An interactive password prompt may be used when key access is unavailable, but credentials
must never appear in a script, URI, repository file, copied terminal output or commit.

## Build and staging

Build only with the approved eSDK and keep generated build output outside the repository:

```bash
source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux
lacai_build_dir="$(mktemp -d /tmp/lacai-esdk.XXXXXX)"
cmake -S . -B "$lacai_build_dir" -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug \
  -DVQEC_VISION_AI_ENABLE_QUALCOMM=ON \
  -DVQEC_VISION_AI_ENABLE_CAMERA=ON \
  -DVQEC_VISION_AI_ENABLE_CAMERA_DBUS=ON \
  -DVQEC_VISION_AI_ENABLE_GST_FRAME_BRIDGE=ON
cmake --build "$lacai_build_dir" -j"$(nproc)"
ctest --test-dir "$lacai_build_dir" --output-on-failure
```

Stage the exact candidate as described in [board workspace](board_workspace.md). Record the
Git revision and SHA-256 of the service, tests and DSP skeleton before execution. Do not
replace the canonical service until the isolated candidate passes.

## Mandatory acceptance sequence

Run the following gates in order for every candidate that changes source layout, Qualcomm
adapters, memory ownership, output or DSP behavior.

1. Run all native executables through
   `tools/board/vqec_vision_board_native_tests.sh` with the model-manifest and Zvec
   fixtures. Record `PASS`, `FAIL` and the staged candidate digest; a historical count is
   not a target.
2. Start the compatibility camera, production service and FW-ring RTSP reader from
   `/opt/lacai`. Confirm `first_error=0`, no cascade failure and a clean stop/drain.
3. Run `tools/board/vqec_vision_preview_acceptance.sh` on the host. It captures the live
   stream, derives effective FPS from captured packets/duration and generates an overlay
   contact sheet.
4. Open the contact sheet and verify that boxes/labels follow the intended objects, remain
   inside the frame, have correct color/orientation and contain no stale rectangles from a
   previous frame. Record the reviewer and result; codec metadata alone cannot pass this gate.
5. Measure warm CPU, RSS/HWM, FD and thread counts for the declared workload. Run the
   duration required by the active performance gate; a short sample is only smoke evidence.
6. For DSP changes, run capability negotiation and one real system-client operation from
   the exact staged skeleton directory. Reset/restart and registered-buffer gates remain
   separate from a successful dense-operation call.

Example host-side preview capture after the board publishes RTSP:

```bash
tools/board/vqec_vision_preview_acceptance.sh \
  --uri rtsp://192.168.0.102:8554/live/ai/detect0 \
  --output-dir /tmp/lacai-preview-acceptance \
  --duration-seconds 8 --expected-width 1920 --expected-height 1080 \
  --expected-fps 30 --fps-tolerance 1.0
```

The output contains `capture.mkv`, `metrics.txt` and `overlay_contact_sheet.png`. The video
is transient test evidence and must not be committed because it may contain personal data.

## Current evidence

The exact final candidate was built with the approved eSDK and Hexagon SDK 5.5.7.0. Production
used generic FastRPC v1 for image transform, dense decode and overlay compose, QNN HTP for the
models, the neutral portable SCRFD decoder and bounded EdgeFace cascade. No legacy skeleton was
present in the final runtime layout.

| Field | Result |
|---|---|
| Workload | person + SCRFD + fire/smoke primary graphs; configured EdgeFace cascade |
| Source/output | DMA-heap fixture, 1920x1080@30; H.264 preview, eight surfaces |
| DSP smoke | operation mask `19`; image transform, dense and overlay all executed |
| eSDK/QEMU | 135/135 CTest pass |
| Preview capture | 241 packets/8 s = **30.125 FPS**, H.264 1920x1080 |
| Five-minute throughput | 9006 ring frames/300.119829 s = **30.008 FPS** |
| Five-minute CPU | 6.53% usr + 6.96% sys = **13.50% average** of one logical core |
| CPU peak | 16.00% in one one-second sample; no per-second ceiling was specified |
| Memory/lifetime | VmRSS/HWM 352940 -> 353448 KiB; threads 48 -> 48; FDs 122 -> 122 |
| Model/output health | slots 0/1/2 routed; `cascade_failed=0`; render failures 0 |

The four-frame contact sheet was visually reviewed: person/face boxes and labels followed the
objects, geometry/orientation/colour were correct, and no stale overlay appeared. Labels clipped
at the extreme frame edge only when their associated box was itself clipped; this was not surface
corruption.

Exact final SHA-256 values:

- service: `9adfeabc7dd7749d78bbc8c23b76390c7df67d57674d1058b7c3b4bee4735759`;
- v68 skeleton: `a1f286c64c9e2e6d5dc20c39ee26a6e8f90db1efd9505740b90257e3d23eaec7`;
- DSP receipt: `cbe94d32de6fdf6bd6622305390e09aea3bb822f94daa19dae7942ae0026e2e6`;
- full-run script: `8016987c52e4b298a3579590649c4ed6d27c80e168fae9af7d3eb61131948ef7`.

Raw board evidence is retained under `/opt/lacai/out/acceptance_5m/`. After promotion, the
canonical `/opt/lacai/run_full.sh` path reproduced 29.997 FPS and 13.40% CPU over a ten-second
sanity sample. The workload was intentionally left running for VLC review at
`rtsp://192.168.138.98:8554/live/ai/detect0`.

The three-team v1 contract pack was also staged transiently and its dependency-free checker ran
on `.98`: `PASS: 10 contracts, 18 usecases, 20 cases, 0 receipts, schema v1`. Evidence and exact
artifact hashes are under `/opt/lacai/out/contract_scope/`; the transient input directory was
removed. This proves parser/invariant portability only. At the time of this check the canonical
runner reported camera, service and RTSP stopped, so no new live-runtime claim is attached to the
contract-only smoke.

This accepts the AI APP lead's explicit five-minute, 30 FPS, average-CPU-below-15% gate. The
508 KiB short-run RSS change is not leak-free proof. Signed DSP deployment, released-FW DMA
completion, independent model quality, reset-under-load and product thermal/long-soak evidence
remain owned release gates rather than hidden claims of this result.

The 2026-09-19 Plan 2 metadata candidate ran on the same physical board at its current `.102`
address while that full AI workload remained active. Benchmark SHA-256
`c9945697b847ae64c8bb664011579c38ef3bdf03ebede76f2418907456a22190` committed 45,081
records and completed 9,952 aggregate queries over 300 seconds with no reject, write, query or
oracle failure. Metadata p50/p95/p99 was 2.688/12.454/18.546 ms, CPU was 30.110% of one core,
maximum RSS was 37,120 KiB and the store was 32,567,296 bytes. Concurrent AI APP CPU averaged
12.88% of one core. A post-run capture measured H.264 1920x1080 at 30.000 packet-PTS FPS; its
four-frame contact sheet showed a correctly aligned current person box/label with no stale box.
This is P2 board-smoke, not retention, disk-full, power-cut or production-composition acceptance.
The updated native store test (`957c37ee26dcf77396e77d95a0fe1ab8080797a2cd93e6484978b4f9accc18ab`)
also passed expired-deadline, missing-shard/partial-coverage, index-repair and quota-rejection
cases on the board. Those controlled cases do not replace a physical disk-full or power-cut run.

The closing P2 candidate at commit `7e8538b9f3e15de4fc9da102e0efbf1ed419090b` used service
SHA-256
`d02770e26610e213ec68cca55543e13378ca1d3aad8511eeb74bb58ecfc07a63` and metadata profile
SHA-256 `79cfc9848f05f8918681064836f9c5b97a014b720a22978e15ad5df976f294ce`.
Its exact 300-second interval averaged 6.06% user + 7.11% system = **13.16% CPU** of one core,
345,497 KiB RSS and 30.124 RTSP FPS at H.264 1920x1080. Metadata committed 26/26 work items,
including 18 distinct trajectory chunks, with zero rejection/failure. Direct service drain took
five 100 ms observations and reported `stopped=true`, `first_error=0`, `cascade_failed=0`.

The contact sheet was visually reviewed: orientation, color and geometry were correct and no
stale overlay remained; the scene was empty during its four frames, so this run does not add a
new box-alignment claim. The unchanged storage candidate also passed SIGKILL/reopen, a real
128 MiB tmpfs full condition and recovery, zero-overwritten detail corruption fail-closed and
query cancellation. Raw evidence is under `/opt/lacai/out/p2_candidate/fault/` and
`/opt/lacai/out/p2_acceptance_final_5m/`. This closes P2; it is not long power-loss, flash-wear,
multi-source, released-FW or product thermal qualification.

The 2026-09-20 S04 lifecycle candidate then began with an empty isolated inventory and exercised
signed asynchronous install, duplicate-idempotency replay, ten five-second desired-state cycles,
asynchronous update to 1.0.1, duplicate update, rollback to 1.0.0, uninstall and asynchronous
reinstall. Service and App Manager remained alive. Their file descriptors stayed at 73 and 11.
Over 301 seconds the ring advanced 8,942 frames; service CPU averaged 10.50% and App Manager CPU
3.83% of one logical core, with HWM 225,832 and 17,104 KiB. The runner proved that
`libQnnHtp.so` was not mapped before camera/frame availability. Exact candidate hashes and the
dark-scene preview limitation are recorded in
[S04 validation](fire_smoke_product_slice_validation.md).

The closing candidate at commit `48c385b` added bounded source-loss generation replacement to the
S01-S18 product catalog/status boundary. Exact hashes are recorded in S04 validation. A fresh
inventory advanced 1→2→3→4 through entitlement, asynchronous install and enable; catalog listing
reported S04 running and all 17 unavailable apps unsupported. Ten five-second toggle cycles passed
with service FDs 74→74. The S04-only five-minute interval advanced 8,965 frames in 300.890 seconds,
averaged 10.835% service CPU plus 3.759% App Manager CPU, and kept threads at 23/5. A clean camera
outage longer than 120 seconds retained the service PID and resumed 150 frames in five seconds
after drained generation replacement; an App Manager restart and reverse startup order also
passed. Host capture measured H.264 1920×1080 at 30.124 FPS. The scene remained almost black, so
this adds no model accuracy or overlay-placement claim.

## Evidence record

Every new board result records:

| Field | Required value |
|---|---|
| Source | Git revision and dirty-state declaration |
| Candidate | SHA-256 for service/tests/DSP artifacts used |
| Workload | source resolution/FPS, model IDs and cadence, output surfaces/viewers |
| Runtime | board image, QAIRT/QNN, DSP architecture and relevant plugin versions |
| Correctness | native result, service exit/error, model/golden result where applicable |
| Preview | capture duration, packet count, effective FPS, dimensions, contact-sheet reviewer |
| Resources | sampling interval, CPU convention, RSS/HWM, FDs, threads, temperature |
| Lifetime | start/stop/restart/drain result and any uncertain hardware completion |
| Limits | compatibility fixture versus released FW, unrun gates and owner sign-offs |

Board-local logs live under `/opt/lacai/out`; they are not evidence until tied to this
record and the exact candidate digest. Remove temporary uploads after results are secured.

## Limits and next work

- Production source selects negotiated DSP v1 for preprocessing, dense packages and cDSP preview
  composition; the accepted runtime layout contains no legacy DSP artifact.
- Cold-start still has a high QNN graph-preparation peak. Phase profiling and staged activation
  remain optimization work, but are not part of the accepted steady-state gate.
- Complete product-policy thermal/long-soak and restart-cycle qualification when required; do not
  infer leak freedom from this five-minute result.
- Complete registered multi-tensor DSP transport, cache/fence/completion and reset while
  work is in flight.
- Released-FW allocator, camera, RTSP/UI and recording acceptance still require BSP+FW
  owner evidence.

## See also

- [Board workspace](board_workspace.md)
- [QNN validation](qnn_board_validation.md)
- [FR production validation](face_recognition_production_validation.md)
- [DSP optimization plan](../planning/architecture_improvement/dsp_multiplatform_optimization_plan.md)
