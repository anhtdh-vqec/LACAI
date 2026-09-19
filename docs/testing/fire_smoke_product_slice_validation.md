# Fire and smoke product slice validation

This record captures the exact AI-owned S04 candidate exercised on the authorized QCS6490
target. It separates source/logic evidence, compatibility-board smoke and external product
acceptance.

**Status:** board-smoke — the AI-owned lifecycle, runtime, metadata and reference-evidence slice
passed; model-quality acceptance and released-FW evidence remain external gates. **Layer:** test.
**Source:** closing candidate `48c385b`, service SHA-256
`7b0d1f59a711037e0f2615fe6cb9ed24340e2109efe3ac57948e2cbf94171af4`.

## Candidate and target

| Item | Exact value |
|---|---|
| Validation date | 2026-09-20 |
| Board alias | `lacai-home` |
| Machine ID | `09c89b1858f54955a3d13f2767622448` |
| Workspace | `/opt/lacai` |
| Service digest | `7b0d1f59a711037e0f2615fe6cb9ed24340e2109efe3ac57948e2cbf94171af4` |
| App Manager digest | `d9fdc8ce316105860d55a490c18484ab59fb1482098fba87668d261504f54c97` |
| Control client digest | `a865c94fd280ade1458d2086f6d4abed57785ec80e7348596e05be843b7e3e98` |
| Runner digest | `7bb62cc3983b7cb3a4954920a32929f90a492388cdd1849f6fb97904dcc69493` |
| Catalog digest | `bef8c7066ac6a93e8360e156598f7758cd018e8128bdcc414df5a81c346afe47` |
| App | `security.fire_smoke_detection` |
| Preview | `rtsp://192.168.0.102:8556/live/ai/detect0` during isolated validation |

The runner started the inference service before App Manager and started the camera producer last.
Before the camera was started, `/libQnnHtp.so` was absent from the service process maps. This is
the required no-data gate: publishing D-Bus services, loading inventory or enabling S04 does not
open QNN/HTP. A real source frame is the first event allowed to enter graph preparation.

## Logic and native tests

- Approved eSDK cross-build with Hexagon SDK 5.5.7.0 passed.
- Expanded AArch64 suite under the eSDK QEMU runner: 170/170 passed.
- Inventory recovery kills a child with `SIGKILL` at four install and four update checkpoints;
  every reopen selected the old committed revision and then accepted a clean retry. Two app
  fixtures sharing the same components and deterministic S04 processor replay also passed.
- Nine focused binaries ran natively on the recorded board: fire/smoke incident lifecycle,
  App Manager lifecycle/restart, evidence wire, SQLite evidence recovery, UDS framing, evidence
  retry/revocation, output-runtime lifecycle, metadata runtime and feature activation.
- The SQLite inventory test binary was not counted in the native nine because that test embeds a
  host fixture path. Its logic passed under eSDK QEMU; the board App Manager lifecycle test used
  explicit `/opt/lacai/config/app_package` fixtures instead.

## Dependency-order and lifecycle test

A fresh owner-only inventory directory began with no installed association. The signed
entitlement, asynchronous signed-package submission and desired-state transactions advanced
snapshot revisions 2, 3 and 4. The install operation reached `committed` with result zero before
the runner enabled S04. Repeating the same idempotency key with a deliberately stale expected
revision returned the existing operation and did not change inventory or snapshot revision. The
runtime then acquired the camera and published preview output. Ten disable/enable cycles used a
five-second interval and all resumed the ring:

| Resource | Before | After | Gate |
|---|---:|---:|---|
| Service file descriptors | 73 | 73 | pass |
| App Manager file descriptors | 11 | 11 | pass |
| Service RSS | 192,900 KiB | 209,076 KiB | pass; +16,176 KiB under the 32 MiB gate |
| Service threads | 23 | 23 | pass in the five-minute steady run |

With S04 disabled, an asynchronous update installed package version 1.0.1 and configuration
revision 2 at snapshot revision 26. A duplicate submission made no revision change. Asynchronous
rollback restored package version 1.0.0 at revision 27. Uninstall removed the association without
purging business metadata; asynchronous reinstall committed at revision 29 with `desired=false`,
and enable advanced revision 30. The ring then advanced 150 frames in five seconds.

The exact closing candidate repeated the flow from another empty isolated inventory and reached
revisions 1→2→3→4. `ListApplications(camera_front)` returned exactly S01-S18: S04 was
`supported/installed/entitled/running`; every unimplemented entry was published but fail-closed as
`unsupported`. Its ten additional five-second toggle cycles kept service FDs 74→74 and service
RSS 206,796→223,764 KiB, within the configured 32 MiB growth gate. Every lifecycle mutation in
this run used the asynchronous version 1 operation contract.

The same service process then survived a controlled camera producer outage longer than 120
seconds. The producer exited through `SIGTERM`, the encoded ring stopped, and service PID 2805
remained alive. When the producer returned, the old generation reported a completely drained
`source_lost` state, the service applied the configured 1,000 ms recovery backoff, created fresh
generation owners and resumed 150 ring frames in five seconds without changing the service PID.
No executor-step failure, non-zero final error or incomplete shutdown was observed. A separate
ten-second App Manager restart also retained the service PID and resumed 149 frames in five
seconds. Finally, starting the AI service after both dependencies were already running produced
117 frames in five seconds. These tests cover both dependency orders without making backend or
released-FW acceptance claims.

Configuration CAS advanced to revision 2 with digest
`07e72c1c0bdb8762f3c2771c0d46f5207b8ae9af9a04ef4a7832d2104930e935` while the encoded
ring continued. The AI-owned reference receiver completed one durable evidence probe with zero
retry and zero transport failure. That probe validates the AI transport path only; it is not a
natural fire/smoke event and is not released-FW acceptance.

## Performance and preview

| Measurement | Result |
|---|---:|
| Five-minute service CPU | 10.835% average of one logical core |
| Five-minute App Manager CPU | 3.759% average of one logical core |
| Combined five-minute CPU | 14.594% average of one logical core |
| Five-minute ring cadence | 8,965 frames/300.890 s = 29.795 FPS |
| Five-minute service RSS/HWM | 223,888→223,912/223,912 KiB |
| Five-minute App Manager RSS/HWM | 14,836→14,848/14,848 KiB |
| Startup 30-second process CPU | 13.36% average of one logical core |
| Startup one-second peak | 86% |
| RTSP packet cadence | 30.124 FPS over 8 seconds |
| Stream profile | H.264, 1920 x 1080, declared 30/1 |

The exact S04-only deployment was measured for 300.890 seconds. Its ring advanced 8,965 frames
(29.795 FPS); service CPU averaged 10.835% and App Manager CPU 3.759% of one logical core.
Service RSS moved 223,888→223,912 KiB with 23→23 threads and 74→74 file descriptors; App
Manager RSS moved 14,836→14,848 KiB with 5→5 threads and 12→12 file descriptors. This closes
the agreed S04 service gate of approximately 30 FPS for five minutes at no more than 12% of one
core. The combined AI service plus control plane also remained below 15% of one core.

`perf` attributes the startup peak to `qnn_engine::prepare()` and
`libQnnHtpPrepare.so` `GraphPrepare`, coincident with first-frame graph construction and the
resident-set transition from about 38 MiB to 195 MiB. It is not idle polling and does not occur
before media is available. This measured synchronous vendor preparation remains a cold-start
budget item; an offline QNN context-binary path needs separate model-package and golden evidence
before it can replace graph composition.

The preview cadence and geometry gates passed. The compatibility camera fixture was almost black,
so its contact sheet cannot prove fire/smoke box accuracy or overlay placement. Packet timestamps
were also unset by the compatibility RTSP reader. Neither limitation is reclassified as product
acceptance.

## Acceptance boundary

The following AI-owned claims are supported: startup-order independence, no-frame QNN/HTP gate,
signed asynchronous install/update/rollback with durable operation status and idempotency,
entitlement and desired-state separation, configuration reconciliation, uninstall/reinstall,
bounded repeated toggles, semantic processor logic, metadata/output composition and durable
reference evidence delivery.

The following claims remain open:

- fire/smoke model M0-M5 quality and alarm calibration on an approved real sequence;
- natural S04 event-to-metadata-to-evidence correlation using visible source footage;
- released-FW evidence receiver, media interval/gap receipt and no-viewer prebuffer behavior;
- backend implementation conformance and electrical power-cut qualification;
- offline QNN context-binary qualification if the cold-start peak must be reduced;
- long thermal/power-loss soak beyond the user-approved five-minute runtime gate.

## See also

- [Board workspace](board_workspace.md)
- [Application manager](../architecture/app_manager.md)
- [Event and evidence plan](../planning/architecture_improvement/event_evidence_transport_plan.md)
- [Fire/smoke product slice plan](../planning/architecture_improvement/fire_smoke_product_slice_plan.md)
