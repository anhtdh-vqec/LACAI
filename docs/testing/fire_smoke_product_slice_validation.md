# Fire and smoke product slice validation

This record captures the exact AI-owned S04 candidate exercised on the authorized QCS6490
target. It separates source/logic evidence, compatibility-board smoke and external product
acceptance.

**Status:** board-smoke — the AI-owned lifecycle, runtime, metadata and reference-evidence slice
passed; model-quality acceptance and released-FW evidence remain external gates. **Layer:** test.
**Source:** operation baseline `1bb3ff5`, service SHA-256
`818e31a4d909981f314d96cb81caddf2df772db3d0f565edf90300b1d74531f4`.

## Candidate and target

| Item | Exact value |
|---|---|
| Validation date | 2026-09-20 |
| Board alias | `lacai-home` |
| Machine ID | `09c89b1858f54955a3d13f2767622448` |
| Workspace | `/opt/lacai` |
| Service digest | `818e31a4d909981f314d96cb81caddf2df772db3d0f565edf90300b1d74531f4` |
| App Manager digest | `4076e2f296da8d4fb374bb02eaaf34025a5a113405de1cae6bdc6644c0f8a1bc` |
| Control client digest | `722451a2c274b0638bd4b73fc711ea9f8c09171d77820e9e011b6f6397d3946e` |
| Runner digest | `d517a43bed26543fa41564360075bb7d60f07ab98f960df041a2836355d2106a` |
| App | `security.fire_smoke_detection` |
| Preview | `rtsp://192.168.0.102:8554/live/ai/detect0` |

The runner started the inference service before App Manager and started the camera producer last.
Before the camera was started, `/libQnnHtp.so` was absent from the service process maps. This is
the required no-data gate: publishing D-Bus services, loading inventory or enabling S04 does not
open QNN/HTP. A real source frame is the first event allowed to enter graph preparation.

## Logic and native tests

- Approved eSDK cross-build with Hexagon SDK 5.5.7.0 passed.
- Expanded AArch64 suite under the eSDK QEMU runner: 167/167 passed.
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

Configuration CAS advanced to revision 2 with digest
`07e72c1c0bdb8762f3c2771c0d46f5207b8ae9af9a04ef4a7832d2104930e935` while the encoded
ring continued. The AI-owned reference receiver completed one durable evidence probe with zero
retry and zero transport failure. That probe validates the AI transport path only; it is not a
natural fire/smoke event and is not released-FW acceptance.

## Performance and preview

| Measurement | Result |
|---|---:|
| Five-minute service CPU | 10.50% average of one logical core |
| Five-minute App Manager CPU | 3.83% average of one logical core |
| Combined five-minute CPU | 14.33% average of one logical core |
| Five-minute ring cadence | 8,942 frames/301 s = 29.708 FPS |
| Five-minute service RSS/HWM | 223,315/225,832 KiB |
| Five-minute App Manager RSS/HWM | 16,700/17,104 KiB |
| Startup 30-second process CPU | 13.36% average of one logical core |
| Startup one-second peak | 86% |
| RTSP packet cadence | 30.124 FPS over 8 seconds |
| Stream profile | H.264, 1920 x 1080, declared 30/1 |

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
- asynchronous conversion of entitlement/configuration/desired/uninstall mutations, backend
  conformance and deeper power-loss fault injection;
- offline QNN context-binary qualification if the cold-start peak must be reduced;
- long thermal/power-loss soak beyond the user-approved five-minute runtime gate.

## See also

- [Board workspace](board_workspace.md)
- [Application manager](../architecture/app_manager.md)
- [Event and evidence plan](../planning/architecture_improvement/event_evidence_transport_plan.md)
- [Fire/smoke product slice plan](../planning/architecture_improvement/fire_smoke_product_slice_plan.md)
