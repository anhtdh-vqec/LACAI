# FW to AI face-enrollment contract

Status: implementation-neutral contract for review. The DBus transport is an adapter;
the runtime consumes the `face_enrollment_port` and does not depend on GIO or wire names.

FW invokes enrollment only after authenticating the caller and authorizing the source.
Requests contain opaque `subject_ref` values. AI never accepts a display name as an
identity and never receives raw pixels or biometric vectors over DBus. The live cascade
captures embeddings from the requested source and optional tracked face; this keeps
enrollment on the same alignment and model-version path as recognition.

## Operations

`BeginEnrollment(request_id, subject_ref, source_id, camera_id, channel_id,
target_track_id, expected_samples, expected_gallery_revision)` starts one bounded
request. `target_track_id=0` means the AI adapter must accept a sample only when the
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

- request IDs and subject/source references are bounded UTF-8 identifiers; commands are
  idempotent only when the same request ID and immutable payload are repeated;
- every accepted embedding matches camera/channel, source epoch and model identity,
  and is normalized and quality-validated before index mutation;
- gallery changes use expected-revision compare-and-swap; the resulting revision is
  returned to FW and is required for subsequent mutations;
- authorization covers enrollment, deletion, and the subject identity attribute
  separately; a live box may be emitted without a name when name scope is denied;
- DBus timeouts are unknown outcomes. FW must query status/revision before retrying;
- no method carries image bytes, raw embeddings, filesystem paths, or credentials.

The current controller implements the bounded lifecycle and revision checks in-process.
Durable encrypted storage, peer authentication, and the concrete DBus object/interface
remain platform integration work; they must not bypass this port.
