# Incremental application activation validation

This record captures the exact source, emulation and QCS6490 evidence used to close the
per-application activation-delta baseline. It proves continuity for one product application and
one infrastructure fixture in a shared source session; it is not product acceptance for a second
usecase.

**Status:** accepted — D0–D6 and the rebooted real-camera revalidation passed for the declared
fixture scope on 2026-09-20; released-FW and second-product model-quality gates remain outside
this baseline. **Layer:** test. **Source:** service source commit `8792c5b`, validation-test
commit `cf32ca9`.

## Candidate and target

| Item | Exact value |
|---|---|
| Board alias | `lacai-home` / `192.168.0.102` |
| Machine ID | `09c89b1858f54955a3d13f2767622448` |
| Board staging root | `/opt/lacai/out/incremental_activation_8792c5b` |
| Service SHA-256 | `f02983a22c1a5e9c9be6868dc4e1629d040fe674f595afd0a64e53d95200a4d0` |
| Snapshot fixture SHA-256 | `cac0c36f0989b35284b3b2e8459fde960cad21601fb0eedf52a417a258fcbd92` |
| Runtime-composition native test SHA-256 | `d1ee832247fc66b3f1304ba8ab20929b7cf0811f37a01c0d08ad78fd4a4d2345` |
| Sync-supervisor native test SHA-256 | `64059fa92e85b0f826924fc05338d59192b7c3efb5436caaf0d27ea8b354c98e` |
| Async-supervisor native test SHA-256 | `0186e3ba398c554221e28373f3e855c9a441e25666470746a481da7a34276bf8` |
| Runner SHA-256 | `78a47f241fb8b08aa32372d848e536481777bdac38f6852b1d1f927a69a63234` |
| Preview URI | `rtsp://192.168.0.102:8554/live/ai/incremental-final` |

The closing media source was the board's real 1920x1080 camera path. The managed runner started
the service before App Manager and camera, so the cold sequence also exercised the ownerless and
no-frame gates before QNN/HTP activation. This is compatibility-fixture evidence, not released-FW
or model-quality acceptance.

## Contract and logic evidence

The accepted design keeps the App Manager's complete version 1 snapshot as authority and derives
all reference counts inside AI APP. Exact dependency identity includes source/profile, artifact,
semantic and preprocess contracts, target, tensor contract and execution policy. The runtime
applies output authority and feature rebind before model lifecycle, and uses a global async-worker
quiesce barrier before mutating any bound session.

The approved eSDK cross-build and full AArch64 QEMU suite passed 172/172 CTest in 45.05 seconds.
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
- all twenty intervals advanced the encoded ring; the service PID remained `7010` and every
  activation commit retained source epoch `1`;
- service FDs were 84 before stress and 86 after stress; RSS was 276,312 KiB before and
  279,680 KiB after, a 3,368 KiB delta with no per-cycle upward trend;
- the service log recorded zero source-session faults, execution-loop failures, rejected
  composition progress calls or stop signals.

A configuration-only S04 update advanced the ring from 3,940 to 3,950 during reconciliation with
the same service PID. It replaced the affected feature configuration without a source or model
generation restart.

The App Manager was then stopped for ten seconds and restarted from the current validated
snapshot. The manager PID changed from `7017` to `8163`; service PID `7010` remained unchanged and
the ring advanced `4,498 -> 4,802 -> 4,957`. A post-restart probe off/on cycle also completed,
and the final snapshot had both applications desired.

## Preview and resource result

After a second clean reboot, the exact candidate started with boot ID
`4edea8d8-6999-4206-9435-69d4271b6c17`; service PID `1889` used the recorded service digest and
reported no source or execution-loop fault. The host captured 240 H.264 packets at 1920x1080.
Packet PTS span was 7.967 seconds and effective FPS was 30.000, passing the 30 FPS ±1 FPS gate.
Two contact sheets covering 8 and 30 seconds were visually checked for continuity, orientation,
colour and geometry. A person entering the frame edge received a green box that remained inside
the image boundary. This validates transport/overlay geometry, not detection accuracy.

With both applications active after the clean reboot, a ten-second `pidstat` sample measured
5.70% user, 7.70% system, 0.40% wait and 13.40% total of one logical core. Final service state was
84 FDs, 279,736 KiB RSS and 24 threads during that sample.

## Hardware-recovery boundary

An additional destructive compatibility-fixture check killed the real-camera producer for 15
seconds while both graphs were active. The final fault de-duplication emitted exactly one
`source_lost` and one `timeout` event even though those codes alternated during drain. The service
drained the old generation and composed a new one, but one FastRPC invocation did not complete and
the encoded ring did not resume within 180 seconds. A full process replacement through reboot
restored the exact candidate and the 30 FPS preview.

This negative observation does not invalidate the activation-delta gate: uncertain hardware
completion was already an explicit replacement boundary, and no buffer was released early. It
does mean automatic recovery from a mid-inference camera loss is not accepted and must not be
advertised. The board retains `camera_recovery_boundary.txt` and the matching service-log segment
under the candidate evidence directory.

## Acceptance and limits

D0–D6 are accepted for the infrastructure fixture scope. Normal compatible desired,
configuration and entitlement changes use the in-place path; capacity/artifact identity changes,
all-off source teardown and uncertain hardware recovery remain explicit full-process replacement
boundaries.

This result does not claim product acceptance for face recognition, smoking or another future
usecase, released-FW camera/RTSP/evidence conformance, long thermal soak, or eighteen-usecase
hardware capacity. Each product package still needs its own model/golden/quality and resource
admission evidence, but it must integrate through this accepted activation contract.

## See also

- [Incremental activation architecture](../architecture/incremental_app_activation.md)
- [ADR 0012](../adr/0012_incremental_app_activation.md)
- [Incremental activation plan](../planning/architecture_improvement/incremental_app_activation_plan.md)
- [QCS6490 target](qsc6490_board.md)
