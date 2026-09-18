# QCS6490 board target

This document defines the only authorized LACAI development target and retains the latest
reproducible evidence for that target. It is not a chronological archive of superseded boards.

**Status:** accepted — QCS6490 `192.168.138.98` passed the declared five-minute full-workload
AI APP gate on 2026-09-18. Released-FW completion and product-release qualification remain
external gates. **Layer:** docs.
**Source:** `tools/board/`, `docs/testing/board_workspace.md`.

## Responsibility

- Authorize only `192.168.138.98` for LACAI board work.
- Define evidence that must be collected from the exact staged candidate.
- Keep credentials outside Git, logs and command history.
- Separate native logic smoke, live preview correctness, throughput, resource and external
  owner acceptance.

## Target and access rules

The target is `192.168.138.98`, QCS6490 / Qualcomm Linux 1.8. Use `/opt/lacai` as the
only board workspace. Verify that any local SSH alias resolves to this exact address before
use, then try non-interactive key access first:

```bash
getent hosts 192.168.138.98
ssh -o BatchMode=yes -o ConnectTimeout=5 root@192.168.138.98 true
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
  --uri rtsp://192.168.138.98:8554/live/ai/detect0 \
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

This accepts the AI APP lead's explicit five-minute, 30 FPS, average-CPU-below-15% gate. The
508 KiB short-run RSS change is not leak-free proof. Signed DSP deployment, released-FW DMA
completion, independent model quality, reset-under-load and product thermal/long-soak evidence
remain owned release gates rather than hidden claims of this result.

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
