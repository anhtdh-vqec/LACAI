# FW to AI face-enrollment contract

Status: implementation-neutral contract for review. The DBus transport is an adapter;
the runtime consumes the `face_enrollment_port` and does not depend on GIO or wire names.

FW invokes enrollment only after authenticating the caller and authorizing the source.
Requests contain opaque `subject_ref` values. AI never accepts a display name as an
identity and never receives raw pixels or biometric vectors over DBus. The live cascade
captures embeddings from the requested source and optional tracked face; this keeps
enrollment on the same alignment and model-version path as recognition.

## Operations

`BeginEnrollment(request_id, subject_ref, image_path, source_id, camera_id, channel_id,
target_track_id, expected_samples, expected_gallery_revision)` starts one bounded
request. `image_path` is the FW-authorized local path for file enrollment; when it is
empty, `target_track_id=0` means the AI adapter must accept a sample only when the
frame has one eligible face; a frame with multiple eligible embeddings is rejected as
ambiguous. `expected_samples` allows several templates for one subject and is bounded
by the validated deployment policy.

`CancelEnrollment(request_id)` transitions a collecting request to cancelled. Samples
already committed remain gallery records; FW can remove the subject with a revision CAS
when its policy requires rollback.

`RemoveSubject(subject_ref, expected_gallery_revision)` deletes all templates for the
subject as one serialized command sequence. A revision conflict or partial backend
failure is reported; callers must reconcile status before retrying.

`GetEnrollmentStatus(request_id)` returns state, accepted/expected sample counts,
gallery revision and a typed last-error code. AI emits progress only after the mutation
has succeeded. `GetGalleryStatus()` returns the current revision and health, without
returning identities, vectors or biometric data.

## Invariants

The source adapter supplies the logical source ID and eligible-face count with each
completed embedding batch. Automatic selection requires exactly one eligible face,
then pins its track and source epoch for the request. Duplicate/backward frames are
rejected; a source epoch change fails the request. Retry with the same request ID and
payload returns the recorded status without enrolling again. Conflicting payloads with
the same ID are rejected. The current controller retains one request receipt; older
receipts need the durable store before restart-safe retries can be claimed.

DBus v1 uses bus `com.vqec.Lacai`, object `/com/vqec/Lacai/FaceEnrollment`, interface
`com.vqec.Lacai.FaceEnrollment1`. Begin input is `(ssssuutut)` and status output is
`(ssuuuti)`; remove input/output are `(st)` / `(ti)`. Wire state/error codes must be
mapped explicitly by the adapter rather than exposing C++ enum ordinal values.
The adapter resolves a configured trusted FW bus name to its unique sender at startup.
All four methods require that exact sender; FW reconnection requires rebinding/restart.
No caller-supplied identity field grants authorization. RPC timeout and callback budget
are mandatory validated deployment settings. This authenticates the configured peer;
feature entitlement and output-name authorization remain separate gates. The service
requires `--fr-feature-id` and `--fr-identity-attribute` when FR is enabled and installs
a policy rule for each deployed source. A recognized subject is labelled only after that
scope is authorized at render time; a denied identity scope leaves the detector box
without a subject label.

- request IDs and subject/source references are bounded UTF-8 identifiers; commands are
  idempotent only when the same request ID and immutable payload are repeated;
- every accepted embedding matches camera/channel, source epoch and model identity,
  and is normalized and quality-validated before index mutation;
- gallery changes use expected-revision compare-and-swap; the resulting revision is
  returned to FW and is required for subsequent mutations;
- authorization covers enrollment, deletion, and the subject identity attribute
  separately; a live box may be emitted without a name when name scope is denied;
- DBus timeouts are unknown outcomes. FW must query status/revision before retrying;
- no method carries image bytes, raw embeddings or credentials; `image_path` is a
  bounded FW-authorized reference and must be resolved inside an allow-listed adapter.

The current controller implements the bounded lifecycle and revision checks in-process.
The optional GIO DBus adapter implements the concrete object/interface and authenticates
the configured FW peer at startup. Durable encrypted storage and peer-name provisioning
remain platform integration work; file enrollment execution must be added before a
non-empty `image_path` can be accepted and must not bypass this port.

The delivered [image source adapter](../architecture/face_enrollment_image_source.md)
decodes one JPEG to a neutral owned NV12 image. It is not yet connected to FD/FR or
filesystem admission; non-empty paths still return `unsupported` from the controller.
