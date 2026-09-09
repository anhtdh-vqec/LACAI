# Mandatory project instructions

Scope: this directory and all descendants. Applies to contributors and coding agents.

## Read before changing files

- docs/development/code_convention.md
- docs/development/naming_registry.md
- docs/architecture/system_architecture.md
- The relevant contract and module README.
- For Qualcomm: docs/research/qualcomm_source_review.md and
  docs/architecture/qualcomm_adapter.md.
  Also read docs/research/qualcomm_plugins_reference.md,
  docs/architecture/qualcomm_plugin_adapter_reference.md and
  docs/adr/0002_qualcomm_plugin_backend.md. The user-confirmed target is QSC6490
  / Qualcomm Linux 1.8. Do not block source implementation on missing sysroot.

## Non-negotiable

0. Project-owned source/header/executable-tool filenames use
   `vqec_vision_<logical_name>.<extension>`. Strip the physical prefix when resolving
   registered file_id; do not rename existing function symbols. Markdown, tool-defined
   build/config names, manifest data and external source references are exceptions
   specified in code_convention.md section 0. Run tools/vqec_vision_check_source_layout.ps1.
   FW compatibility baseline is docs/contracts/fw_release_compatibility.md;
   AI owns preview overlay/encode/ring production, FW owns RTSP/UI and recording.

1. C++17. All project-defined identifiers use snake_case, except documented
   language/external ABI/macro conventions.
2. Every project-defined named function, including private/static methods, uses
   vqec_vision_ai_<dir_id>_<file_id>_<verb_object>.
   Register dir_id/file_id before adding source. Overrides retain the declaring
   interface's name; do not rename per implementation directory.
3. Named parameters begin with a single underscore followed by lowercase.
   Globals begin g_. No double underscore or underscore + uppercase.
4. No OpenCV production dependency. No Gst/QNN/FastCV/vendor types in neutral
   contracts/core/perception/features/runtime.
5. Never release/reuse a buffer while submitted hardware may still access it.
   Timeout, stop request, source disconnect, and FD close are not completion.
6. Bounded queues/pools; explicit ownership, clock domains, source epochs,
   transform coordinates, tensor dtype/layout/quantization.
7. Do not claim board compatibility, zero-copy, hardware acceleration, performance,
   or feature acceptance without measured evidence.
8. Installed, entitled, desired, supported, admitted, running are separate states.
   Enforce authorization for outputs and attributes, not only UI switches.
9. Do not copy vendor code without per-file license/provenance review.
10. Preserve user work. Do not modify sibling repositories. Do not commit secrets,
    model binaries, biometric data, or private SDK libraries.
11. After each coherent LACAI source implementation step is complete, inspect the
    worktree, run the applicable checks, create a focused commit containing only
    task-owned changes, and push it to the current branch's configured upstream
    (`@{upstream}`). If no upstream is configured or push fails, report the exact
    blocker and do not describe the step as synchronized. Never stage or commit
    unrelated user changes, secrets, model binaries, biometric data or private SDKs.
12. All LACAI C++ builds, tests and CMake configuration must use the approved eSDK
    toolchain rooted at `/home/a/Workspace/eSDK`. Do not configure or report a host
    compiler build as validation. If the eSDK toolchain cannot be located, report the
    exact missing path/tool and stop before claiming build/test success.
13. Never store Git usernames, passwords, tokens or credential-bearing URLs in source,
    documentation, git config committed to the repository, command output or commit
    messages. Use the environment's credential helper, SSH agent or approved secret
    store for upstream pushes; report authentication failures without echoing secrets.

## Change workflow

- Update contract/schema/ADR before implementing a changed external boundary.
- Follow docs/development/review_checklist.md.
- Tests and docs ship with the change; report tests not run and why.
- Changes to naming, ABI, ownership/sync, entitlement need lead review plus
  relevant owner review. An agent must not silently waive these rules.
- Current source includes initial plan validation, NULL-state plugin graph assembly,
  and legacy third-stream FD receiver/decoder source with session-owned ACK.
  Read docs/architecture/camera_legacy_adapter.md before changing camera ownership.
  Camera Start/Stop state machine and optional GIO D-Bus client are now source-delivered;
  read docs/architecture/camera_control_client.md before changing lease reconciliation.
  Combined source lifecycle now gates StopStream on cross-session frame-owner drain;
  read docs/architecture/camera_source_lifecycle.md before changing start/stop behavior.
  FW RAW-source resolution is activation-only and product-origin agnostic; read
  docs/architecture/raw_source_resolution.md before changing endpoint composition.
  New source-session code depends on the vendor-neutral raw_source_port, never directly on
  Camera Service wire or product origin; read docs/architecture/raw_source_port.md.
  Application orchestration depends on inference_graph_port, not Qualcomm/plugin types;
  read docs/architecture/inference_graph_port.md before changing model submission.
  Private GstMemory wrapping now retains the owner on root memory;
  read docs/architecture/dmabuf_memory_bridge.md before changing memory lifetime.
  No hardware completion validation, executable service runtime or active CI.
  src/app/camera_graph_pump now wires bounded receive/submit/result progress. Read
  docs/architecture/camera_graph_pump.md before modifying supervisor/stop ownership.
  src/app/camera_session coordinates one acquisition through start/drain/release;
  read docs/architecture/camera_session.md before changing deadlines or RPC cleanup.
  src/app/multi_source_supervisor binds 1..16 pre-composed sessions with bounded
  round-robin progress and per-source fault isolation; read
  docs/architecture/multi_source_supervisor.md before changing fairness or global stop.
  The supervisor depends only on source_session_port; never reacquire one FW RAW source
  per model. Multi-model implementations must fan out one retained frame owner.
  Per-source model cadence is fixed-capacity rational phase arithmetic; read
  docs/architecture/model_cadence.md before changing frame selection or skip policy.
  Multi-model RAW fan-out receives once, uses fixed 16-slot storage and shares one frame
  owner across accepted graphs; read docs/architecture/multi_model_pump.md before changing
  result fairness, busy-skip policy or model submission ownership.
  multi_model_session validates every graph before one FW acquisition and releases FW only
  after all graphs reconcile; read docs/architecture/multi_model_session.md before changing
  partial-start rollback, deadline or graph drain ordering.
  The optional output_manifest loader and manifest_check CLI validate metadata only.
  Read docs/architecture/model_output_manifest.md before changing schema or trust boundaries.
  The separate model_catalog loader declares model input/cadence/resource envelopes and
  is cross-validated against deployment source assignments. Read
  docs/architecture/model_catalog.md and multi_source_configuration.md before changing
  catalog, admission snapshots or source/model authority. Fixed numeric snapshot indices
  are valid only for their recorded immutable revisions.
  A JSON digest or matching selection does not prove artifact authenticity.
  artifact_digest adds bounded SHA-256 stream comparison; read
  docs/architecture/artifact_digest.md before integrating hashing or model loading.
  Do not treat a receipt as signature verification or TOCTOU-safe path authorization.
  Tracked GstMemory release now forwards input completion to submission_window;
  this does not prove hardware completion without the backend memory-retention contract.
  PLAYING/EOS/output polling now exists; read docs/architecture/qualcomm_stream_control.md.
  frame_submission is now wired to plugin_graph with one job, ticket correlation,
  completion/drain/unload gates and a supervisor-owned four-slot retention domain.
  Read docs/architecture/qualcomm_submission_lifecycle.md and frame_submission.md
  before changing ownership. Never bypass retention or replace a full domain to
  evade admission. Armed destructor retention is not BSP recovery or DMA cancellation.
  READY source binding is now mandatory before PLAYING; read
  docs/architecture/source_binding.md before changing color/sync policy.
  Tensor sample extraction now exists as a standalone helper; read
  docs/architecture/tensor_output.md before changing output layout or CPU visibility.
  Graph model loading now supports explicit READY/NULL and bus polling; read
  docs/architecture/qualcomm_graph_lifecycle.md before changing graph states.
  Pure submission_window bookkeeping/PTS mapping is source-delivered; read
  docs/architecture/submission_window.md before changing admission/completion semantics.
  The portable tracking stage validates detection input, resets on source epoch and
  faults an epoch after ambiguous tracker failure; read docs/architecture/tracking_stage.md
  before changing tracker orchestration.
  Feature algorithms use exact schema/version attribute lookup; read
  docs/architecture/attribute_reader.md before changing attribute validation or freshness.
  Feature/model dependencies and processor/resource contracts come from the feature catalog;
  read docs/architecture/feature_catalog.md before changing usecase integration metadata.
  Feature algorithms emit configuration-bounded neutral events through feature_processor_port;
  read docs/architecture/feature_event_contract.md before changing feature output semantics.
  feature_stage validates processor output and faults ambiguous epochs; read
  docs/architecture/feature_stage.md before changing feature orchestration.
  Feature-event delivery derives authorization from actual payload fields; read
  docs/architecture/feature_event_dispatch.md before changing routing or retry semantics.
  Completed model results reconstruct source identity only from retained submission tickets;
  read docs/architecture/perception_result_stage.md before changing decode/track composition.
  Per-source feature fan-out uses stable numeric slots and per-feature fault isolation; read
  docs/architecture/feature_fanout.md before changing feature scheduling.
  Multi-model results route by stable numeric slot with independent source progress; read
  docs/architecture/multi_model_result_router.md before changing result routing or gap detection.
  Single-model feature dependencies bind by immutable model slot; read
  docs/architecture/multi_model_feature_pipeline.md before changing result-to-feature composition.
  Do not describe
  directory placeholders or unbuilt source as completed production modules.
