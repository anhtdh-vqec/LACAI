# Incremental application activation validation

This record captures the exact source, emulation and QCS6490 evidence used to close the
per-application activation-delta baseline. It proves continuity for one product application and
one infrastructure fixture in a shared source session; it is not product acceptance for a second
usecase.

**Status:** accepted — D0–D6 passed for the declared fixture scope on 2026-09-20; released-FW and
second-product model-quality gates remain outside this baseline. **Layer:** test.
**Source:** service source commit `39ffe8d`, validation-test commit `cf32ca9`.

## Candidate and target

| Item | Exact value |
|---|---|
| Board alias | `lacai-home` / `192.168.0.102` |
| Machine ID | `09c89b1858f54955a3d13f2767622448` |
| Board staging root | `/opt/lacai/out/incremental_activation_39ffe8d` |
| Service SHA-256 | `e5c17d24b32b0c1eb92674b7223fd4c7e8a6394514ef944fb638e3addc6c362c` |
| Snapshot fixture SHA-256 | `cac0c36f0989b35284b3b2e8459fde960cad21601fb0eedf52a417a258fcbd92` |
| Runtime-composition native test SHA-256 | `f5b4dd7c82ed7c822b38e36539986ceec1b5e01584e6305ccef6f2f29e69bedc` |
| Async-supervisor native test SHA-256 | `db8d36b95566fc21b168b4c8bdcd37d7bf4c00e0c9d922f516faada50a6324e7` |
| Runner SHA-256 | `78a47f241fb8b08aa32372d848e536481777bdac38f6852b1d1f927a69a63234` |
| Preview URI | `rtsp://192.168.0.102:8554/live/ai/incremental-final` |

The media source was the deterministic NV12 `test_pattern` mode. It exercises the production
camera wire/ACK, QNN/HTP, feature, overlay preparation, encoder and FW v5 ring path without a
sensor dependency. Therefore this run is valid lifecycle and resource evidence, but not released
FW camera or visual detection-quality acceptance.

## Contract and logic evidence

The accepted design keeps the App Manager's complete version 1 snapshot as authority and derives
all reference counts inside AI APP. Exact dependency identity includes source/profile, artifact,
semantic and preprocess contracts, target, tensor contract and execution policy. The runtime
applies output authority and feature rebind before model lifecycle, and uses a global async-worker
quiesce barrier before mutating any bound session.

The approved eSDK cross-build and full AArch64 QEMU suite passed 172/172 CTest in 51.73 seconds.
The two race-sensitive tests then passed 30 consecutive QEMU iterations each. The same two exact
binaries passed ten consecutive native iterations each on the recorded board. Coverage includes:

- all eighteen catalog applications, stale/capacity/conflicting identities and transactional
  output preservation;
- shared `2 -> 1` retention and unique `1 -> 0` drain/unload counters;
- feature-owner replacement without replacing unrelated owners;
- in-flight lifecycle, source fault isolation and output-policy revocation;
- two-source async activation reaching one common mutation point;
- graph owner unload/reload with a fresh one-shot submission window;
- bounded shutdown waiting for real worker completion.

## Board activation stress

The cold state had S04 fire/smoke enabled and `fixture.shared_model_probe` installed but disabled.
Enabling the fixture acquired `yolov8n_person` and retained the already-running
`yolo11n_fire_smoke` graph (`1 -> 2`) without reacquiring the source. The final stress performed
twenty desired-state transitions at five-second intervals:

- five probe off/on cycles released/reacquired only the unique person graph while fire/smoke
  continued through the shared graph;
- five fire/smoke off/on cycles retained the fire/smoke graph at `2 -> 1 -> 2` because the probe
  remained a consumer;
- all twenty intervals advanced the encoded ring; the service PID remained `4448` and every
  activation commit retained source epoch `1`;
- service FDs were 83 before stress and 84 after stress; RSS was 287,136 KiB before and
  288,736 KiB after, a 1,600 KiB delta with no per-cycle upward trend;
- the service log recorded zero source-session faults, execution-loop failures, rejected
  composition progress calls or stop signals.

A configuration-only S04 update advanced the ring from 22,491 to 22,641 in five seconds with the
same service PID. It replaced the affected feature configuration without a source or model
generation restart.

The App Manager was then stopped for ten seconds and restarted from the current validated
snapshot. The manager PID changed from `4456` to `5284`; service PID `4448` remained unchanged and
the ring advanced `23,135 -> 23,460 -> 23,697`. A post-restart probe off/on cycle also completed,
and the final snapshot had both applications desired at snapshot revision 34.

## Preview and resource result

The host captured 241 H.264 packets over 8.000 seconds at 1920x1080. Packet PTS span was
7.967 seconds and effective FPS was 30.124, passing the 30 FPS ±1 FPS gate. The contact sheet was
visually checked for continuity and contains the expected deterministic grayscale gradient. No
detection boxes are expected from this synthetic input, so it cannot close a product overlay or
model-quality gate.

With both applications active after stress, a ten-second `pidstat` sample measured 6.50% user,
6.90% system, 0.30% wait and 13.40% total of one logical core. Final service state was 85 FDs,
288,736 KiB RSS and 24 threads during that sample.

## Acceptance and limits

D0–D6 are accepted for the infrastructure fixture scope. Normal compatible desired,
configuration and entitlement changes use the in-place path; capacity/artifact identity changes,
all-off source teardown and uncertain hardware recovery remain explicit replacement boundaries.

This result does not claim product acceptance for face recognition, smoking or another future
usecase, released-FW camera/RTSP/evidence conformance, long thermal soak, or eighteen-usecase
hardware capacity. Each product package still needs its own model/golden/quality and resource
admission evidence, but it must integrate through this accepted activation contract.

## See also

- [Incremental activation architecture](../architecture/incremental_app_activation.md)
- [ADR 0012](../adr/0012_incremental_app_activation.md)
- [Incremental activation plan](../planning/architecture_improvement/incremental_app_activation_plan.md)
- [QCS6490 target](qsc6490_board.md)
