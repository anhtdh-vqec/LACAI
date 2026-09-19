# FW control, outputs, entitlement and BSP handoff

This umbrella proposal defines the system FW/AI APP control plane, output envelope and BSP
handoff boundaries. Usecase application distribution is an AI APP-owned boundary: the backend
calls the AI-owned D-Bus App Manager and FW/BSP does not manage usecase packages or inventory.

**Status:** source-delivered — the usecase activation boundary is normative in
[FW usecase activation](fw_usecase_control.md); the D-Bus v1 adapter and service-owned
runtime replacement are source-delivered. **Layer:** contracts. **Source:** `n/a`.

The proposal below extends, not replaces, the released AI D-Bus and H264 preview
interfaces. See [FW release baseline](fw_release_compatibility.md) for exact legacy
methods, persistence behavior, ring ABI and RTSP/UI demand chain. AI APP owns preview
overlay/encode/ring output; FW owns persistent evidence and recording. New control
methods and signed-grant provisioning must not be assumed present in released FW.

## Responsibility

- Defines system control, output and BSP handoff boundaries and distinguishes base-system
  deployment from AI-owned usecase application lifecycle.
- Keeps AI APP ownership of preview overlay/encode/ring output and of the protected face
  gallery; FW own persistent evidence and recording.
- Must not be read as released FW: new control methods and signed-grant provisioning must
  not be assumed present.

## Ownership

| Team | Deliverable |
|---|---|
| FW BSP | board/image/sysroot/SDK + memory/import/sync/reset sample, capability limits |
| FW software | camera transport, base-service supervision, base image/runtime deployment, config and event/evidence persistence; no usecase App Manager authority |
| AI Model | full model package + golden + quality report |
| AI APP | runtime/backend integration, AI-owned D-Bus App Manager, usecase entitlement verification, private package store/inventory/install/update/rollback, feature rules, outputs/metrics, FR enrollment/matching and protected gallery/key/index persistence |

FW must not open, copy, back up or mutate the face gallery and must not provision or
receive its encryption key. AI also confines the derived Zvec collection to private
volatile storage and rebuilds it from the durable encrypted snapshot. FW enrollment/remove
methods pass bounded identity metadata and an authorized image path; AI APP validates the
request and commits its own gallery.

The BSP kit must pin headers and runtime binaries together, device dependencies,
permissions, allocator/cache/fence API, SDK threading rules, test model, profiler, thermal
budgets and redistributable scope. OSS source does not replace this kit.

## Control plane

get_capabilities, get_health, list_features, get_effective_config,
validate_config, apply_config(expected_revision, request_id), apply_desired_plan,
apply_entitlement, get_runtime_status, request_diagnostics, prepare_shutdown.

Config atomic snapshot: validate schema/license/model/resources before publish.
Revision CAS; stale revision rejected; a failed apply must not leave a half-enabled graph.
The response separates an accepted command from actual running readiness.
Status per source/feature: installed, entitled, desired, supported, compatible,
admitted, effective_state, reason, config/model/license revisions.

Auth comes from the configured backend transport identity plus AI APP policy; do not trust a
caller-written customer_id field.
Parse bounds; rate limits; a diagnostic dump contains no secrets/biometrics by default.

## Entitlement

Source update: [output_gate](../architecture/output_gate.md) now provides a pure
deny-by-default evaluator for trusted source/feature/attribute policy with revision CAS,
monotonic validity and queued-output revision checks. It is not a signed-grant verifier,
not connected to a transport/router yet, and does not implement compute admission.

A signed grant contains grant_id, issuer/key_id, revision, device/customer scope,
feature_ids, attribute scopes, source/camera limits, not_before/expires, offline policy and
signature. The backend supplies grants through the authenticated AI-owned App Manager boundary;
AI APP owns schema, verification and publication. Trust-anchor provisioning and trusted-time
primitives are deployment inputs, not FW authority over the usecase state. The runtime verifies the grant and
enforces deny-by-default for features outside scope. An installed bundle does not by itself
grant every feature in the bundle.

Enable: validate -> admission -> load deps -> start -> effective running.
Disable/revoke: block unauthorized output -> stop scheduling the feature -> drain ->
release only unused deps. A shared tracker is not reset if another consumer still uses it.
Recheck the revision when dispatching queued output; a retry spool must also follow the
revocation/retention policy and must not unconditionally export old embeddings.

A signed offline license does not know about a newer remote revocation while disconnected.
Clock rollback/key rotation/grace policy is fixed by the AI APP/backend security contract.
Root-bypass resistance may depend on platform secure-boot and trusted-time primitives, but that
dependency does not transfer App Manager or entitlement state authority to FW/BSP.

## Outputs

Envelope: schema version, event_id, source/epoch, capture time + clock mapping,
feature_id, model/config/license revision, track/entity references, quality,
typed attributes, geometry coordinate space, evidence correlation.

- Live observations/tracks: bounded lossy, drop metrics.
- Alarm: at-least-once + event_id dedup; bounded spool, retry/backoff/TTL.
- Counts: window_id/sequence/checkpoint, consumer dedup; do not double count on retry.
- Heatmap: grid/calibration version, bucket times, exposure/gap metadata.
- Retrieval: authorized embeddings/index records, model version, retention.
- Evidence request: source/time window/event id; FW returns pending/ready/failed +
  evidence reference. AI does not hold raw 4K itself to act as a clip recorder.

Disk/network unavailable: explicit degraded state, spool bounds and an agreed overflow
priority; do not call delivery durable indefinitely. FW manages storage quota and
retention. AI does not log/raw-export faces/embeddings by default.

## Package/update

Base OS/image and the LACAI runtime service remain C10 deployment artifacts integrated by FW/BSP.
Usecase applications are separate C05 artifacts managed only by AI APP: authenticated backend
D-Bus request -> bounded Unix-FD staging -> signature/digest verification -> dependency/preflight
-> atomic inventory generation -> runtime drain/activate -> health commit or rollback. Backend and
FW/BSP cannot write the private app store or set installed/running fields. No package-provided
script may kill the service before quiescence. A signed model/feature manifest does not replace
camera authorization. The target workflow is specified in
[usecase app distribution](../planning/architecture_improvement/usecase_app_distribution_plan.md).

## Acceptance

Forged/expired/wrong-device grant; config CAS conflicts; unauthorized attributes; revoke
during inference/output retry; offline/clock changes; partial install; ABI/model mismatch;
crash/restart; retry dedup; disk full; gallery embedding version mismatch; evidence time
drift. Each test has an owner and an expected reason.

## Limits and next work

- The AI-owned App Manager and signed entitlement verifier remain planned; the delivered
  output_gate is a policy evaluator only.
- Measurement and release cases are tracked in
  [FR validation](../testing/face_recognition_production_validation.md).

## See also

- [FW usecase activation](fw_usecase_control.md)
- [FW release baseline](fw_release_compatibility.md)
- [Output gate](../architecture/output_gate.md)
- [FR production validation](../testing/face_recognition_production_validation.md)
