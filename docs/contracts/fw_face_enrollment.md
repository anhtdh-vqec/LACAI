# FW to AI face-enrollment contract

This document defines the implementation-neutral FW-to-AI face-enrollment contract: the
D-Bus transport is an adapter, and the runtime consumes the `face_enrollment_port` without
depending on GIO or wire names. It fixes the operations, authorization and gallery-revision
invariants that FW and AI APP must honor.

**Status:** source-delivered — the bounded neutral controller and the optional GIO D-Bus
adapter are delivered, with hardware-key qualification and peer-name provisioning still
pending. **Layer:** contracts. **Source:** `n/a`.

FW invokes enrollment only after authenticating the caller and authorizing the source.
Requests contain opaque `subject_ref` values. AI never accepts a display name as an
identity and never receives raw pixels or biometric vectors over DBus. FW supplies an
authorized local image path; the person is not required to stand in front of the camera.
AI decodes that retained file and runs the same detector/alignment/embedding contracts as
live recognition.

## Responsibility

- Defines the enrollment/removal/status operations and their authorization and
  gallery-revision invariants.
- Keeps image bytes, raw embeddings and credentials off D-Bus; only bounded opaque
  references cross the boundary.
- Must not grant authorization from a caller-supplied identity field or treat a D-Bus
  timeout as a completed mutation.

## Operations

`BeginEnrollment(request_id, subject_ref, image_path, source_id, camera_id, channel_id,
target_track_id, expected_samples, expected_gallery_revision)` starts one bounded
request. `image_path` is the required FW-authorized local path. Version 1 requires
`target_track_id=0`, `expected_samples=1` and exactly one eligible face in each image;
a file with zero or multiple eligible faces is rejected. Multiple faces for one person
are enrolled through separate idempotent requests sharing the same `subject_ref` and
advancing the gallery revision after each accepted image.
The request source/camera/channel must match the enabled FR pipeline's immutable binding.
Unknown or mismatched sources are rejected before image decode or gallery mutation.

`CancelEnrollment(request_id)` transitions a collecting request to cancelled. Samples
already committed remain gallery records; FW can remove the subject with a revision CAS
when its policy requires rollback.

`RemoveSubject(subject_ref, expected_gallery_revision)` deletes all templates for the
subject as one serialized command sequence. A revision conflict or partial backend
failure is reported; callers must reconcile status before retrying.

`GetEnrollmentStatus(request_id)` returns state, accepted/expected sample counts,
gallery revision and a typed last-error code. AI emits progress only after the mutation
has succeeded. `GetGalleryStatus()` returns revision, subject count, template count,
availability and fault state, without returning identities, vectors or biometric data.

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
`(ssuuuti)`; remove input/output are `(st)` / `(ti)` and gallery status output is
`(tuubb)`. Wire state/error codes must be
mapped explicitly by the adapter rather than exposing C++ enum ordinal values.
The adapter resolves a configured trusted FW bus name to its unique sender at startup.
All five methods require that exact sender; FW reconnection requires rebinding/restart.
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

The controller implements the bounded lifecycle and revision checks in-process. The
optional GIO D-Bus adapter implements the concrete object/interface and authenticates the
configured FW peer at startup. In Qualcomm production mode the service resolves a
non-empty path through configured roots, decodes it to an owned DMA-BUF frame and executes
dedicated SCRFD/EdgeFace graphs outside the D-Bus callback. The AI-owned encrypted store
is wired into production composition; peer-name provisioning remains deployment work.

AI APP also owns the derived vector index. Production deployment supplies a collection
path beneath an existing service-UID-owned mode-0700 tmpfs parent. Persistent plaintext
index paths fail closed. FW never reads/indexes those files; successful enrollment is
durable only through the authenticated encrypted authoritative snapshot, from which AI
rebuilds the volatile index after restart. Swap/crash-dump and hardware-key qualification
remain release requirements.

The delivered [image pipeline](../architecture/face_enrollment_image_pipeline.md) defines
bounded path authorization, decode, FD, alignment, EdgeFace and gallery mutation. Its
dedicated graphs do not share submission state with the live camera graphs.

## Limits and next work

- Peer-name provisioning, hardware-backed key qualification, swap/crash-dump isolation and
  power-loss acceptance remain release work.
- The current controller retains one request receipt; durable enrollment receipts are needed
  before restart-safe retries can be claimed.

## See also

- [FW usecase activation](fw_usecase_control.md)
- [AI-owned protected face-gallery contract](fw_face_gallery_storage.md)
- [Face enrollment image pipeline](../architecture/face_enrollment_image_pipeline.md)
