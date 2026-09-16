# FR production validation — 2026-09-16

Result: runtime switching and image-path enrollment pass against simulated FW on QCS6490
`.98`. This is integration evidence; release acceptance is still open for the gates below.
No access was made to `.48` or `.99`. No biometric fixtures, model binaries or credentials
are included in Git.

## Candidate and environment

- C++17 candidate built with `/home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux`;
  build tree `build-esdk-full`. No host C++ configuration/build was used.
- Target deployment: QCS6490 / Qualcomm Linux 1.8, artifacts under `/opt/anhtdh`.
- Live roots: configured person detector and face detector; dependent face embedding model.
  Qualcomm preprocessing/alignment, QNN, QTI overlay/H264 and FW-compatible encoded ring.
- FW camera simulation uses real camera capture and retained packed memfd copies. This
  tests control/wire/ownership integration, not released-FW DMA-BUF interoperability.
- FW usecase/enrollment simulation retains one authenticated unique D-Bus sender across
  runtime transitions. AI service PID remains the same.
- Protected-gallery baseline is an existing enrolled subject. Tests add two templates to
  a uniquely named temporary subject, then delete only that subject. Original gallery
  subject/template counts are restored; mutation revisions intentionally advance.

## Results

| Case | Evidence | Result / limit |
|---|---|---|
| eSDK build + QEMU CTest | 120/120 passed; affected checks rerun after native-fixture path fix | PASS logic/target ABI, no BSP acceptance |
| Native logic/contract binaries | 114 binaries on `.98`; decoder-package fixture root supplied on target | PASS after correcting build-machine fixture path |
| Initial enabled roots | Usecase status plus `/proc/<pid>/maps` | Person, FD and FR model libraries resident |
| Both → person only | D-Bus complete desired plan, new published generation, same PID | Person resident; FD/FR libraries absent |
| Person only → all off | Same persistent usecase D-Bus object | All three model libraries absent; control remains responsive |
| All off → face only | Status/generation + native model maps | FD/FR resident; person library absent |
| Face only → both | Status/generation + live routed results | All three resident; person and face results routed independently |
| Gallery across disable/re-enable | `GetGalleryStatus` before/after each enabled FR generation | Revision/counts preserved |
| Desired-plan retry | Same request/payload/expected revision | Original receipt returned |
| Invalid desired plans | Stale revision, unknown ID, duplicate association | Rejected without plan publication |
| Untrusted sender | Separate unique connection; all eight usecase/enrollment methods | `AccessDenied`; no mutation |
| Enrollment binding | Wrong source/camera; empty path; samples other than one; stale gallery revision | Rejected |
| Image policy/failure | Outside allowed root, missing file, synthetic no-face JPEG | Terminal failure; gallery unchanged |
| File enrollment | Existing user-authorized JPEG, separate FD/alignment/FR graphs | Completed via D-Bus without requiring live camera attendance |
| Multiple templates/person | Two independent requests for one temporary subject | One new template/revision per image |
| Terminal enrollment retry | Same immutable request after completion | Completed status; no second template |
| Enrollment payload conflict | Same request ID, changed subject | Rejected; no mutation |
| Delete CAS | Stale expected revision | Rejected |
| Delete multi-template subject | Current revision | Both templates removed; original gallery preserved |
| Cancellation, capacity, epoch/model mismatch, store tamper | Existing controller/gallery/persistence/ownership unit contracts | Logic-tested; native camera/in-flight failure matrix remains open |
| Ring replacement | Native synthetic regression: replacement, missing/partial ring, reappearance | PASS; obsolete mapping is closed |
| RTSP after final enable | ffprobe TCP: H264, 1920×1080, `30/1`; FFmpeg decoded 60 frames to null | PASS after runtime switching; inference cadence/name accuracy separate |

Model-library absence demonstrates the configured model artifacts were unloaded from this
process. It does not alone prove all DSP allocations were reclaimed, memory was scrubbed,
or a thermal target was met. `loaded/running` before initial publish is false. Active
publish waits for all source-session health phases to be running; this is session readiness,
not warmup/accuracy/permanent hardware health. Status observation during teardown and later
source faults still needs fuller runtime-health integration.

## Reproduce

For device-free service-generation testing:

```bash
source /home/a/Workspace/eSDK/environment-setup-armv8-2a-qcom-linux
cmake --build build-esdk-full -j4
ctest --test-dir build-esdk-full --output-on-failure
```

`service_usecase_runtime_dbus` owns a simulated FW peer, starts the eSDK target service
through QEMU and checks both → first → off → second → both without process restart.
Python/GI is only the control runner; C++ executes through the eSDK emulator.

For native testing use `tools/vqec_vision_fr_runtime_dbus_test.py --fixture <private-json>`
on the board's same configured bus. Start it before AI binds its trusted peer; it owns
`peer_name`. The fixture must belong to an isolated test deployment and contain:

| Fixture field | Meaning |
|---|---|
| `control`, `enrollment` | Objects with deployed `service` and `object` names |
| `session_bus`, `peer_name` | Bus selection and simulated authenticated FW well-known name |
| `rpc_timeout_ms`, `transition_timeout_seconds`, `poll_seconds` | Explicit positive test budgets |
| `service_executable` | Exact target executable path, for same-PID model-map checks |
| `source_id`, `camera_id`, `channel_id`, `fr_usecase_id` | Enabled FR binding and catalog identity |
| `model_libraries`, `initial_models` | Logical key → model library substring, and key → expected resident boolean |
| `transitions` | Ordered objects containing `desired` usecase booleans and expected `models` residency |
| `image_path`, `failure_images` | Authorized enrollment JPEG and failing-image paths, kept outside Git |
| `template_samples` | Integer ≥2 within the test ceiling and admitted gallery capacity |
| `keep_peer_alive` | Optional true for a live demo; retains the trusted unique connection |

Final transition must enable FR so enrollment methods are available. Every transition must
change desired state. This tool intentionally mutates the gallery with a random temporary
subject and removes that subject after success; on failure reconcile the pending request
and test subject before another run. Do not run alongside an active customer enrollment.

Native unit test `vqec_vision_ai_decoder_package_test <manifest-root>` accepts a target
fixture root. Zvec integration receives a fresh synthetic directory argument. Never run
its rebuild/destruction test against the real gallery.

## Open release gates and next steps

1. **Biometric at-rest protection:** authoritative snapshot is authenticated/encrypted;
   `.98` confirms directory 0700 and key/gallery/lock 0600. Zvec derived files are plaintext
   and currently live in a persistent collection directory with mode 0755. This must be
   corrected: AI-owned private volatile/encrypted derived storage, safe directory identity,
   swap/crash-dump policy and deletion/rebuild tests. Filesystem key beside ciphertext is
   not hardware-bound; hardware keystore/TEE, rotation and backup/recovery need qualification.
2. **Durable control:** persist authenticated desired plans and enrollment request receipts;
   restart/retry after unknown outcomes must not silently reenroll. Today live desired
   revisions and receipts are process-local; clean restart restores the startup snapshot.
3. **Nonblocking control:** move synchronous model preparation, image decode/inference,
   index mutation and persistence behind bounded workers; measure callback latency and
   cancellation races. Full-generation replacement reloads retained workloads too.
4. **Runtime faults:** inject source disconnect, late hardware completion, model-load
   failure, candidate rollback, drain timeout and peer loss on an isolated board. Recovery
   required now blocks replacement; process exit is not proof of DMA quiescence. Native
   fault injection is not claimed by passing synthetic ownership tests.
5. **Commercial authority:** signed entitlement verification/provisioning and live revoke
   during inference/output retry. D-Bus caller can change only desired state. Trusted gate
   fixture tests do not qualify forged/expired/wrong-device grants or measured admission.
6. **Recognition acceptance:** approved golden inputs/crops/embeddings, known/unknown and
   multi-person scenes, threshold/margin calibration, false-match rates and display-name
   metadata policy. Prior user-observed name visualization remains historical evidence;
   this unattended protocol run does not independently verify the person-name image.
7. **Performance:** per-stage and end-to-end p50/p95/p99, inference-result FPS, box update
   age, CPU/RSS/FD/accelerator recovery per transition, DDR/copy counters, temperature and
   clocks with an agreed scene/gallery size. RTSP metadata 30 FPS is not AI 30 FPS or
   CPU 15–25% acceptance. Add sustained soak and repeated switching under enrollment load.
8. **Product delivery:** bounded/durable attendance events and dedup, retention/delete
   transactions, power-cut/disk-full recovery, package/update rollback, D-Bus system-bus
   policies/peer reconnection and released-FW RTSP/RAW compatibility signoff.

Lead, FW and BSP/storage owners must review lifecycle/entitlement/storage boundaries. This
report records implementation and evidence and does not waive their acceptance.
