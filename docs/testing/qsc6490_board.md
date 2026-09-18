# QCS6490 board target

This document defines the only authorized LACAI development target and retains the latest
reproducible evidence for that target. It is not a chronological archive of superseded boards.

**Status:** board-smoke — QCS6490 `192.168.138.98` passed the 2026-09-18 native and
canonical-deployment preview checks described below. The exact source candidate remains
blocked at the legacy DSP loading boundary. **Layer:** docs.
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
  --expected-fps 25 --fps-tolerance 1.0
```

The output contains `capture.mkv`, `metrics.txt` and `overlay_contact_sheet.png`. The video
is transient test evidence and must not be committed because it may contain personal data.

## Current evidence

The latest retained evidence on 2026-09-18 is:

- approved eSDK/QEMU CTest: **135/135**;
- isolated native candidate on `.98`: **128/128**;
- the canonical deployed person + face + fire/smoke service stopped cleanly with
  `first_error=0`, 66 cascade tasks/embeddings and no cascade failure;
- the canonical RTSP preview produced 201 packets in 8.000 seconds, or **25.125 FPS**, at
  H.264 1920x1080. The generated four-frame contact sheet was reviewed: person/face boxes
  and labels followed the intended objects, orientation and colors were correct, and no
  stale rectangles were visible;
- the canonical process 15-second warm sample was
  `10.59% usr + 15.86% sys = 26.45%` of one logical core, with average RSS about
  422520 KiB, HWM 423084 KiB, 44 threads and 160 FDs. This misses the plan's `<=12%`
  sustained-CPU target and is not a soak result;
- v68 FastRPC v1 skeleton SHA-256
  `a5e7d1c030bb110c6b493be5c9c8349ec68b905443ff93d2c0af96ee67625f5a`;
- live v1 client open/query/execute/close: 24 valid output bytes, no truncation, output box
  `15,15,25,25`; a second process returned a different nonzero domain generation.

The exact clean-build service candidate was staged separately and did not replace the
canonical service. It correctly rejected the board's stale model catalog marked schema 2;
an isolated schema-1 copy passed loading, after which production preparation failed closed
because `libvqec_dsp_skel.so` could not be opened on cDSP (`AEE_EUNABLETOLOAD`). Production
composition still opens this legacy, model-named ABI before selecting operations and does
not select the negotiated generic v1 client. Therefore the canonical preview result is not
exact-candidate service acceptance. The failure is retained as a production-composition
defect; copying a legacy skeleton is not an accepted workaround.

This evidence does not prove signed DSP deployment, released-FW DMA completion, model
quality, cold-start CPU, thermal stability or leak freedom.

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

- Make production composition select the negotiated generic DSP v1 operation path and
  remove unconditional startup dependence on the legacy model-named skeleton.
- Re-run service, preview capture and visual overlay review from that exact candidate;
  canonical-deployment evidence cannot be transferred to it.
- Complete cold-start CPU profiling, the declared sustained workload, memory soak and
  restart-cycle leak gate.
- Complete registered multi-tensor DSP transport, cache/fence/completion and reset while
  work is in flight.
- Released-FW allocator, camera, RTSP/UI and recording acceptance still require BSP+FW
  owner evidence.

## See also

- [Board workspace](board_workspace.md)
- [QNN validation](qnn_board_validation.md)
- [FR production validation](face_recognition_production_validation.md)
- [DSP optimization plan](../planning/architecture_improvement/dsp_multiplatform_optimization_plan.md)
