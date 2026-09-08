# Implementation status — 2026-09-08

## Source delivered, not built

### Current baseline after FW compatibility and filename normalization

Input-boundary correction: deployment now exposes only mandatory unique
`raw_source_ref`; `input_kind`, RTSP endpoint and AI-side demux/decode concepts were
removed from contract, strict loader, schema, examples, tests and activation snapshot.
AI Camera and AI Box must present the same FW RAW descriptor/lease semantics. On AI Box,
RTSP discovery/credential/demux/decode and decoder memory remain FW responsibility.

Multi-source deployment contract/validator and strict bounded JSON loader source now
represent 1..16 unified FW RAW inputs for AI Camera or AI Box, exact per-source profiles,
model assignments and conservative memory ceilings. Fixed 4K/25 defaults were removed
from inference, legacy-wire and DMA-BUF limits: callers must supply an effective profile
and allocation budget. JSON Schema/example and cross-team architecture are documented.
The fixed-capacity multi-source supervisor now binds exactly 1..16 pre-composed sessions,
advances one non-stopped source per round-robin call, isolates a source fault while its
session drains, aggregates state and latches global stop. It deliberately does not resolve
FW RAW references, create per-source executors or restart epochs. FW RAW-source/catalog
authentication, global board admission and output-registry resolution remain pending. The
released FW output still limits preview to fixed detect0/detect1 identities. Source was not
built or executed, per request to defer build/test work to the Linux SDK machine.

The separate AI Model-team catalog is now represented by a neutral contract, pure
validator, strict 512 KiB/depth-16 JSON loader, reviewable schema/example and source-only
tests. Cross-validation rejects unknown/incompatible assignments, concurrency excess,
per-source tensor overflow and model-resident budget overflow. The initial inference plan
is composed transactionally from deployment profile, catalog preprocessing and a trusted
platform path binding. Signature/artifact/output-manifest resolution and live runtime
activation remain unimplemented; no build/test was run on Windows.

Activation snapshot source now converts validated deployment/catalog mappings into
fixed-capacity arrays with 16-bit indices, exact revision binding, assignment/context
counts and an actual admitted resident estimate. This is the allocation-free scheduler
lookup shape; it is not yet a source supervisor, job scheduler or board capability probe.

Neutral observation batch contract now represents model detections, per-track attributes,
quality/confidence and validity timestamps for human/traffic expansion. Validator enforces
frame/geometry identity, bounded counts, identifier syntax, confidence range and expiry
ordering. Source test covers valid person/age data and malformed cases; no decoder,
tracker, entitlement or model accuracy is implied. Tests remain unexecuted.

Combined encoder drain source now requires both backend quiescence and empty ledger;
backend success alone cannot hide a pending preparation/completion. One-step helper
does not free jobs or spin. Added regression source; no build/test run per user request
to defer execution until Linux SDK environment is available.

Bounded one-event polling helper now validates backend events before transferring them
to a reset caller destination; refuses overwrite and stops admission on poll faults.
Fake backend test source covers pending, transfer, overwrite guard and separately applied
input/no-output events. Process event loop and concrete encoder remain missing; unbuilt.

submit_backend now calls the neutral encoder port after ledger envelope/one-shot guards,
reconciles guaranteed pre-access rejection, and retains accounting on ambiguous exceptions.
Fake-backend test source checks duplicate suppression, backend-held pixels and rejection
cleanup without dropping the caller's owner. Concrete hardware backend remains missing;
C++ tests are still unexecuted.

Encoder begin_input now validates the complete envelope against ledger-retained frame,
profile and original pipeline PTS before consuming the one-shot submission marker.
Tests cover modified pipeline PTS, wrong generation and repeat attempts. No backend
call is made by this guard; C++ tests remain unexecuted.

Encoder ledger now has a one-shot begin_submission marker to prevent a committed
ticket being sent repeatedly. Rejected duplicate attempts preserve all accounting;
they are not permission to synthesize completion of an earlier accepted job. Unit
source covers before-commit, accepted, repeated and retired-token cases. Runtime
backend submit wiring remains pending; tests are unexecuted.

Encoded dispatch now provides one-event preflight/dispatch/ledger composition with
separate delivery outcome and terminal live-preview discard policy. Duplicate/invalid
events cannot reach the sink. Input completion remains independent. Concrete backend
polling/context storage are not implemented; C++ tests remain unexecuted.

Encoder event preflight now checks committed-token correlation and duplicate completion
without mutation, enabling rejection before H264 dispatch. apply_event reuses preflight;
encoder entries track successful commit alongside the base ledger. Runtime must serialize
preflight/handling/apply; this is not yet wired into an event pump. Tests remain unexecuted.

Encoder window now applies validated backend events with exact ledger correlation;
malformed/stale/duplicate events and backend faults latch admission closed without
freeing outstanding accounting. Later proven completions can drain retained entries.
Unit source covers duplicate detection, fault retention and completion order. No event
polling loop/device implementation or executed C++ test is claimed.

Encoder event structural validator now distinguishes completions, owned output,
terminal no-output and faults; rejects malformed tokens/status/payload combinations.
Unit-test source added for valid and invalid combinations. No runtime event pump or
hardware completion verification is supplied by this pure check; tests remain unrun.

Preparation-to-backend envelope handoff now captures frame and dispatch generation at
prepare time and emits encoder_input with the committed ticket/sealed surface. Failed
re-prepare cannot replace an occupied binding. Contract-test source checks generation
validation, rebind rejection and envelope preflight. No concrete device submission;
C++ tests remain unexecuted.

Encoder input preflight now validates independent frame/profile/ticket/generation and
exact packed NV12 ownership size using named surface limits. Portable test source covers
zero timestamps, wrong same-size shape, stale job/generation, PTS mismatch and absent/
short/oversized buffers. No backend calls or hardware claim; C++ tests remain unexecuted.

Neutral encoder_backend port now defines sealed CPU NV12 submission, explicit accepted
versus rejected ownership, independently polled input/output completion, terminal
no-output, fault and drain semantics. Architecture contract specifies bounded event
delivery and ambiguous driver acceptance retention. Declaration only: concrete adapter,
conformance tests and runtime wiring remain required; no hardware behavior is proven.

Encoder preparation test source now covers drain/deadline between prepare and handoff:
rejected commit preserves writer, destination and reservation; explicit cancellation
then reclaims never-submitted work. Existing committed-job timeout ownership behavior
is unchanged. Tests have not been compiled/executed.

Submission ledger and encoder correlation table now share a named job ceiling;
configurable timeout fallback has an explicit nanosecond owner/provenance comment.
Independent compile-time assertions pin existing values. No capacity, deadline or
FW input behavior changed. Stale submission-window integration documentation corrected.

Bound encoder_preparation facade now keeps ledger/ticket/writer together for explicit
cancel or committed sealed-owner handoff. New callers no longer need to pair a raw
writer with an external cancellation token. Source tests cover cancellation, occupied
handoff destination, transfer and retained pool ownership. This is pre-submission only;
backend completion/recovery integration and C++ test execution remain pending.

Encoder preparation cancellation now rejects an empty/sealed/moved-from writer before
mutating the ledger. Regression source checks that the real writer, occupied pool slot
and reserved bytes remain intact. Matching a nonempty writer to the supplied token still
requires caller ownership discipline; a runtime job owner is not yet implemented.
C++ test source is unexecuted; this change does not establish hardware completion.

Dispatch regression source now composes the output ID issuer and replacement fake
sink, checking stale binding rejection before and after a new-frame write. No retagging
of queued output, real ring reopen or executed C++ test is implied.

Output binding ID issuance source now exists: non-copyable/non-movable serialized
allocator, monotonically consumed IDs, no reset/reuse, fail-closed uint64 exhaustion.
Portable unit-test source/CMake covers normal issuance, failed-setup ID discard and
exhaustion preserving destination. Runtime-wide ownership/wiring remains pending;
no cross-process uniqueness, live reconnect proof or hardware completion is implied.

Ring startup preparation: private transactional open-options mapper now matches released
detect0/detect1 writer settings; optional SDK test source checks exact IDs/layout/flags
and unchanged output on invalid selection. No ring is opened by the helper or tests.
Reinspection found SDK implicit unlink/reinitialization paths even when explicit
replacement is disabled; documented in fw_ring_sink.md for runtime/deployment review.
Safe startup/recovery and single-writer enforcement remain unimplemented, not waived.

Output metadata limits now have one named owner in the output_gate contract;
policy evaluation and encoded dispatch share the identifier ceiling while retaining
distinct rule, attribute and rendered-scope budgets. Added independent boundary-test
source for exactly 64 rules/attributes, 128-byte identifiers and rejected oversized
attribute updates preserving the previous policy. No authorization or FW wire change.
C++ tests remain uncompiled/unexecuted. Architecture/output README stale missing-sink
claims corrected: optional wrapper exists, live runtime integration remains pending.

Camera error compatibility fix: confirmed FW 2004/9001 codes now map to resource_exhausted/
protocol_error instead of generic io_error. Added regression source checking returned
status, retained start_pending state and unchanged retry fields. Structural checker
passed; tests have not been compiled or run. No lease cleanup/retry behavior changed.

FW output constant alignment: ring sink now uses shared preview geometry/payload/slot
limits and asserts SDK parameter-set capacity against the neutral contract. Codec comes
from the FW ai_output definition; remaining stream/framing values have named local owners.
Independent release-value assertions added to portable preview test source. Not built/run.

Independent compile-time regression assertions now pin Camera endpoint/method values,
critical result codes and every named legacy header offset in existing unit-test sources.
The packet fixture still constructs bytes using independent numeric offsets. This guards
constant extraction against accidental wire changes when tests are compiled; assertions
have NOT been compiled/executed in this environment. Structural check remains separate.

Legacy frame decoder now names fixed FW header field offsets and shares the 104-byte
packet size with the receiver. Compile-time layout-end assertion added. No packet,
plane validation, ACK or lifetime semantics changed; numeric fixture layout remains
independent. Structural checker passed; C++ receiver/decoder tests are still unexecuted.

Camera control now shares protocol field/method/value constants and parser budgets
with the GIO boundary. FW numeric result codes are named with original wire values;
StopStream still treats only NotFound as successful absent-lease reconciliation.
No lifecycle behavior change intended. Existing test fixtures retain literal wire
expectations independently. Structural checker passed; C++ tests not executed.

2026-09-07 Camera RPC literal cleanup: private camera_protocol header now owns fixed
Camera bus/object/interface and supported method names plus separately labeled AI parser
budgets. GIO binding uses those constants; structural check passed (87 source/tool files).
No wire behavior changed. No compiler/build tools found in PATH or inspected standard
VS/JetBrains/MSYS locations; this is not proof none exists elsewhere. C++ tests remain unrun.

2026-09-07 literal cleanup: preview_limits now owns shared preview geometry, per-surface/
pool budgets, pool capacity, label/primitive ceilings and released encoded byte limits.
Preview validators, CPU allocation/pool and encoder admission reference these values;
boundary tests retain independent expected values. Structural checker passed; no compiler
was discovered through Get-Command cmake/clang++/g++ in this shell. No build/test claim.
This is partial literal cleanup, not completion of input/output integration or repo-wide audit.

FW output hardening: separate SDK-local mapping_generation from runtime-unique
dispatch_generation; both required by ring_sink configuration. Neutral demand/write
use dispatch identity so SDK counter restart alone cannot validate an old queued output.
Runtime unique-ID issuance remains a caller responsibility, not yet implemented here.
Header mapper is now independently testable and transactional; optional SDK test source
checks full legacy metadata/SPS/PPS and malformed/stale input without shared-memory I/O.
Not built/test-executed; live generation/RTSP tests remain pending.

FW ring_sink wrapper source now calls the reviewed shared ring API behind the neutral
sink port. Validates binding and maps legacy H264 metadata, using direct-payload push.
Optional Linux target requires an existing pinned SDK target; no sibling code copied.
Only unopened-ring guard test source added, not compiled/executed. Ring open/supervision,
external replacement handling, encoder wiring and live RTSP/UI qualification remain pending.
See [FW ring sink](../architecture/fw_ring_sink.md).

Synchronous encoded_dispatch now composes owned H264, envelope/freshness checks,
output_gate authorization of all declared rendered scopes, demand/generation check
and one synchronous sink write with no automatic retry. Fake sink tests cover no viewers,
stale mapping, denied scopes, revoke and write failure. Not compiled/executed.
Rendered-scope extraction/binding, real FW sink and hardware encoder remain missing;
this helper is not proof the supplied scope list matches encoded pixels.
See [encoded dispatch](../architecture/encoded_dispatch.md).

Encoded output ownership: validated transactional H264 payload/SPS/PPS copies now publish
an immutable shared owner. A neutral synchronous encoded_sink port specifies demand
generation and copied-before-return semantics. No real sink implementation or output
queue is present. Unit tests/CMake source added, not compiled/executed.
See [encoded output](../architecture/encoded_output.md).

Encoder preparation app helper now composes reserve -> pool acquire -> rollback
and explicit unsubmitted cancellation. Exact width/height matching precedes reservation;
no fallback surface allocation. Portable contract-test source covers no demand,
geometry mismatch, pool exhaustion rollback and result/timeout versus input ownership.
Not compiled/executed. No real encoder push, rendering or ring integration.
See [encoder preparation](../architecture/encoder_preparation.md).

Preview pool source now preallocates 1–4 bounded CPU NV12 surfaces and loans each
through a separate lease control block into writable_preview_surface/sealed readers.
Last reader release permits reuse; old weak readers cannot revive a recycled lease.
Readers survive pool facade destruction. Unit-test source/CMake added; not built/run.
No encoder owner integration, global pool-generation admission or hardware pool claim.
See [preview pool](../architecture/preview_pool.md).

Encoder ledger source now reuses submission_window with consumer-demand admission,
fixed geometry/source binding, aggregate packed input byte accounting, exact result
correlation and independent completion. It owns no pixels; the encoder must retain
actual sealed input owners. Timeout/drain do not reclaim reservations. Unit-test source
and CMake wiring added, not compiled/executed. No pool, encoder adapter or ring sink.
See [encoder window](../architecture/encoder_window.md).

R1 ownership primitive: writable_preview_surface now owns CPU packed NV12 with an
explicit per-surface allocation budget, move-only writer and shared immutable seal.
Unit-test source covers move/seal and retained readers. No global pool/admission,
encoder completion detector or FD/hardware surface is implemented. Not built/run.
See [preview surface](../architecture/preview_surface.md).

R1 first slice: neutral preview frame/geometry/rectangle metadata and borrowed H264 AU
envelopes now have pure validators plus unit-test source/CMake wiring. Exact correlation,
revision/freshness, geometry/text and FW payload limits are checked. No full H264 parser,
authorization integration, surface pool, encoder, ring sink or demand runtime exists.
See [preview contract](../architecture/preview_contract.md). Not built/test-executed.

- Project C/C++ filenames now use vqec_vision_; existing function symbols and external
  contracts remain unchanged. Includes, CMake source paths and document references migrated.
- See [release compatibility](../contracts/fw_release_compatibility.md) for FW01–FW09
  and [execution gates](../planning/fw_compatibility_execution.md) for the next source slices.
- Source exists for Camera receiver/control/session, Qualcomm submit/result/drain,
  tensor validation, manifests/digests and pure output policy. No compiled/board claim.
- Missing: runnable cameraai_app, model-specific decoder/tracker, cadence/recovery
  supervisor, legacy AI D-Bus server, writable preview surfaces, overlay, encoder,
  FW ring SDK/writer, install/IPK and end-to-end UI qualification.
- Structural checker tools/vqec_vision_check_source_layout.ps1 is available; it does
  not compile source or enforce C++ AST conventions. No build/C++ test execution.
- Source filenames do not rename cameraai_app, D-Bus endpoints, ring IDs or RTSP mounts.

### Historical delivery log (not current missing-feature inventory)

Entries below describe successive slices. Statements such as no-submit/no-PLAYING
are historical and superseded by the current baseline above and later lifecycle docs.

Latest: output_gate is a pure deny-by-default policy evaluator for exact source/feature
pairs and attribute scopes, with bounded rules, revision CAS, validity/clock checks,
queued revision rejection and immediate invalidation. Tests cover revoke and human/
traffic scope isolation. No build/test execution. Signed grant verification, payload
scope extraction, scheduling and actual dispatch integration remain unimplemented.
See [output authorization](../architecture/output_gate.md).

Latest session hardening: intermediate successful startup/stop actions now return
pending rather than misleading ok. A read-only serialized snapshot reports session,
Camera and graph states, outstanding jobs/readers, recovery flag and first error code.
Retry failures remain diagnostic history; later startup/stop timeout does not overwrite
the first error. Tests assert snapshot has no RPC side effects and history survives
cleanup. No build/C++ execution in this slice. See camera_session architecture contract.

Latest: optional artifact_digest streams SHA-256 through OpenSSL Crypto with a
caller-supplied byte limit (<=4 GiB), 64 KiB scratch, checked EOF/read/provider failures
and transactional matched-digest receipt. Tests cover known vectors, limit/mismatch,
EOF exception masks and stream errors. Source-only: no compile/C++ execution.
This is not signature verification and is not automatically wired into QNN path load;
immutable deployment association is required. See
[artifact digest](../architecture/artifact_digest.md).

Latest: optional output_manifest JSON loader, synthetic example and parser test source
are implemented (64 KiB, duplicate/unknown fields, depth/types, transactional output).
camera_session bind_model_outputs matches independent expected identity/digest/decoder
before installing output specs. Read-only manifest_check CLI is available as an optional
build target and example CTest. No artifact authentication or decoder is inferred.
No build/test execution; dependency nlohmann_json 3.12.0 must be provided locally.
See [manifest integration](../architecture/model_output_manifest.md).

Latest preflight update: shared core tensor_contract now validates complete ordered
FLOAT32 output metadata and payload budgets before camera_session can issue StartStream.
plugin_graph reuses the same validator before PLAYING. Added pure boundary tests and
a session regression checking malformed outputs leave Camera idle with zero RPCs.
Not built/run. This is typed metadata validation, not model-kit file loading or artifact
verification; configuration loader and concrete model decoder remain pending.

Latest slice: camera_session coordinates validation, StartStream/connect, effective
profile check, graph configure/load/bind/start, pumping, EOS/drain/unload and only then
Camera stop/reconciliation. Startup/stop deadlines do not free jobs or forget leases;
cleanup continues after timeout with recovery_required exposed. No destructor RPC.
Tests added for uncertain Start/Stop identities, startup expiry, stop-before-start,
monotonic time guards and release completion, without loading Qualcomm plugins.
These tests are source-only; no build or test execution. See
[session coordinator](../architecture/camera_session.md).

Current update: plugin_graph now connects the submission primitive and ledger, with
one job per graph, internal PTS correlation, deadline-aware poll_result, separate
input/result completion and EOS-plus-job drain. Unload is blocked while jobs remain.
Armed destructor transfers to a reserved supervisor retention slot instead of NULL.
Four slots cap active/retained armed graphs within that domain; explicit restore
supports late-result reconciliation. No automatic BSP reset or safe-process-kill claim.

Next slice also delivered: camera_graph_pump composition source connects the existing
source_lifecycle receiver to the graph, arms from the first frame epoch, retains the
received_frame owner through FD submission, and polls correlated results. It creates
no worker, per-frame RPC or extra source. Supervisor start/stop orchestration, executable
main and live end-to-end validation are not delivered. Pump guard tests are source-only.
See [camera graph pump](../architecture/camera_graph_pump.md).

Added a separate synthetic graph lifecycle test target (standard appsrc/appsink only),
covering success, timeout, retained destruction/restore and domain exhaustion/reuse.
No compiler or tests were run. See
[current graph lifecycle](../architecture/qualcomm_submission_lifecycle.md).

The incremental entries below are historical; their no-submit/no-PLAYING statements
are superseded by this update. Remaining integration is supervisor start/stop and
configuration, model decoder/output routing and board validation.

- Private frame_submission primitive now performs reserve/wrap/commit/appsrc push,
  retains a committed record on push error, forwards input completion, and checks
  result PTS before result bookkeeping. No early completion on timeout/error.
  Synthetic appsrc/appsink contract test and CMake wiring added, not executed.
  This is NOT wired into plugin_graph: production graph fault/destructor containment
  remains required. See [submission primitive](../architecture/frame_submission.md).

- Source binding now validates effective geometry/FPS, explicit linear NV12 DMA-BUF,
  implicit sync policy, colorimetry/chroma siting and bounded deployment evidence IDs.
  Graph requires bind_source in READY before start_stream and clears binding on unload.
  Caps labeling does not prove FastCV color conversion or hardware synchronization.
  Pure policy tests and empty-graph guard added; not run. See
  [source binding](../architecture/source_binding.md). Input submit is still absent.

- Tracked memory bridge now connects atomic root-owner release observation to the
  serialized submission_window input-completion event. Ticket PTS is applied to the
  wrapped buffer, original source PTS remains in the descriptor/ticket. Extended bridge
  test source covers result-before-input, child-memory retention and idempotent polling.
  This is a memory ownership signal, not hardware synchronization. Input push remains absent.

- PLAYING/EOS/drain state control and zero-wait appsink polling are now source-delivered.
  Output contract and expected job PTS are checked. No input push API exists yet.
  This supersedes earlier no-PLAYING/no-appsink notes below; input completion/admission
  wiring and hardware synchronization remain pending. See
  [stream control](../architecture/qualcomm_stream_control.md).

- FLOAT32 tensor sample extraction with model-contract shape/order matching, bounded
  owned CPU copies and transactional result; synthetic contract-test source added.
  No appsink polling or synchronization is implemented by this helper.
  See [tensor output boundary](../architecture/tensor_output.md).

- Qualcomm graph model-load lifecycle: READY/NULL requests, nonblocking bounded bus/state
  polling, fault diagnostics and reconfiguration guards. No PAUSED/PLAYING or submit API.
  State changes may synchronously block inside vendor SDK; no bounded shutdown claim.
  State-guard test source added; actual model load/unload and bus faults not tested.
  See [graph lifecycle](../architecture/qualcomm_graph_lifecycle.md).

- Pure submission_window: fixed-capacity reservation/commit, cycle/job correlation,
  checked source-to-pipeline PTS mapping, independent input/result completion,
  drain/fault/deadline bookkeeping without implicit cancellation. Unit-test source added.
  See [submission contract](../architecture/submission_window.md). Not wired to graph yet.

- Neutral status + single-image inference_plan.
- Pure validation and packed NV12 byte count.
- Private Qualcomm plugin_graph with runtime GParamSpec/enum checks.
- Explicit fcv mode, disposition/channel order/coefficients and tensor input caps.
- Transactional NULL-state graph assembly and RAII cleanup.
- Target-scoped CMake core / optional Qualcomm / unit-test targets.
- Unit-test source for valid/invalid plans.
- Neutral frame_descriptor with original NV12 plane layout and source timestamps.
- Native-endian legacy camera wire decoder, exact 104-byte payload and bounds checks.
- Optional Linux third-stream receiver: one FD, CLOEXEC, peer UID check, bounded receive,
  max four live frames across current/detached sessions per receiver, session-bound ACK.
- Decoder test source and Linux socket fixture for deferred ACK, reconnect epochs,
  original-session ACK, FD close and malformed extra-FD rejection.
- Camera Start/Stop control state machine with stable retry identities, explicit pending
  states and retained handle on invalid effective profile; injected transport unit tests.
- Optional GIO D-Bus client for FW string-valued a{sv}, bounded method calls/reply parsing,
  private bus connection and no destructor StopStream or automatic service activation.
- Combined Camera source lifecycle: acquisition, connect retry, receive, profile checks,
  cross-session reader drain and explicit release/reconciliation (one acquisition cycle).
- Extended Linux contract-test source for pending drain/no early StopStream, release timeout,
  uncertain acquisition cleanup and geometry-change rejection before downstream delivery.
- Private DMA-BUF memory bridge: duplicated CLOEXEC FD, original valid view and GstVideoMeta,
  read-only memory, root-memory shared owner and original timestamp preservation.
- Bridge contract-test source: padded planes/nonzero memory offset, malformed ranges,
  shallow buffer/memory child ownership and original/duplicate FD closure (no hardware import).

See [camera adapter boundary](../architecture/camera_legacy_adapter.md).
The receiver does not import/map pixels or prove DMA-BUF hardware interoperability.

No compiler, cmake configure, test executable or target pipeline was run by request.
Static inspection is not a substitute for compilation or board integration.
No new third-party implementation was copied.

## Supported initial source contract

NV12 linear, explicit even dimensions <=8192 per parser ceiling, batch1, NHWC 3 channels,
UINT8 identity
or FLOAT32 explicit plugin coefficients; tensor dimension policy 8..4096.
These are software bounds, not claimed board capacity. Paths are lexical-validated
but model authenticity/allowed-root resolution belongs to the caller.
Queue size here does not cap every hidden vendor buffer pool.

## Not yet delivered

Hardware synchronization/completion validation and complete service supervision,
automatic colorimetry/modifier negotiation, trusted evidence/artifact resolution,
model-load board verification, portable model decoder, feature runtime/entitlement,
IPK installation and executed golden/board/fault tests. Other module directories
remain placeholders. Synthetic source is not an end-to-end integration test result.

## Next task

Use the existing FW third-stream NV12/FD baseline documented in
[FW integration requirements](../contracts/fw_camera_integration_requirements.md).
Camera 0 defaults to 1920x1080@25 and currently permits at most 2560x1440@20;
3840x2160 is a future FW capability, not the current camera input contract.
Legacy SOCK_SEQPACKET/SCM_RIGHTS reception and session-bound ACK are source-delivered;
do not assume the proposal's version/epoch/fence fields exist on the current wire.
FW P0 memory/sync/recovery sign-off gates production, not source development.
Legacy media receiver, control client, combined source lifecycle and tests are source-delivered.
GstMemory wrapping, graph submit/results/drain and camera pump are source-delivered;
hardware completion guarantees and live FW inference validation remain pending.
All new source is still unbuilt. Live D-Bus and board integration have not been run.

Next: authenticate/resolve deployment, FW RAW sources and model catalogs, implement the
process-level multi-source supervisor, then model-output decoder/output integration and
executable service composition; BSP recovery execution remains unimplemented. Compile and execute the
synthetic tests when builds are requested; board ownership/fault tests and cross-team
contract review remain mandatory before production activation.
