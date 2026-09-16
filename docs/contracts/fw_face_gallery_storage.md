# FW protected storage contract for face gallery

Status: AI-side neutral contract delivered; FW protected-storage adapter, key provisioning
and crash/recovery acceptance remain pending.

## Ownership

AI APP owns gallery schema validation, opaque subject/template semantics, revision CAS,
model compatibility, Zvec rebuild and recognition policy. FW owns protected storage,
device-bound key provisioning, file/service permissions, quota, backup and retention.
Neither side treats a Zvec collection as the authoritative gallery.

FW exposes protected storage through an adapter implementing `face_gallery_store_port`.
The interface receives no raw remote path or key. Deployment selects the store binding;
normal enrollment D-Bus requests contain an authorized image path and identity metadata,
not a gallery database path or encryption secret.

## Snapshot

One durable snapshot contains schema and gallery revisions, next record ID, gallery ID,
embedding model/version, preprocess revision, vector dimension and bounded current
templates. Each template has an opaque record ID, opaque `subject_ref` and normalized
embedding. Multiple templates may share one subject within the configured limit.

AI APP rejects duplicate/zero record IDs, invalid subjects, non-finite or non-normalized
vectors, identity mismatches, capacity overflow and rollback revisions before storage or
index use. Biometric vectors and subject data must not enter logs or diagnostic dumps.

## Atomicity and recovery

`replace(config, expected_revision, replacement)` is a durable atomic compare-and-swap.
Success means the complete replacement survives power loss; a stale expected revision
changes nothing. A replacement revision is strictly greater; one atomic operation may
advance once per affected template so the derived index can reach the identical revision.
The adapter authenticates/decrypts on load and fails closed on missing,
truncated, corrupt, wrong-device or wrong-key data.

After durable commit, AI APP synchronizes a derived Zvec generation to the same revision.
Search is unavailable while revisions differ. On restart AI APP loads the authoritative
snapshot, builds a separate index generation, verifies record count/identity, then
publishes it atomically. Failure never deletes the durable snapshot and never falls back
to an unverified old collection.

## Acceptance

- add several templates for one subject, restart, and recognize at the same revision;
- remove a subject atomically and prove no old template is searchable after publish;
- crash before/after durable rename and during index rebuild;
- reject stale CAS, corrupt ciphertext, wrong model/preprocess revision and quota overflow;
- demonstrate that logs, D-Bus replies and diagnostics contain no embedding or key;
- measure load, commit, rebuild and query latency at admitted gallery capacity.
