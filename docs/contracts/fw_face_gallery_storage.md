# AI-owned protected face-gallery contract

Status: accepted ownership; AI encrypted-store adapter and restart recovery are delivered.
Hardware-backed key qualification and power-loss/device acceptance remain pending.

## Ownership

AI APP owns the complete protected gallery: schema, encryption/authentication, key
lifecycle, file permissions, quota bounds, atomic replacement, recovery, opaque
subject/template semantics, revision CAS, model compatibility, Zvec rebuild and
recognition policy. Neither side treats a Zvec collection as the authoritative gallery.

FW never opens the gallery, supplies a gallery database path or receives a gallery key.
FW sends authorized enrollment/remove commands and may supervise the AI process. The AI
deployment supplies a trusted local directory; its basename, quota and gallery identity
are validated startup configuration. Normal enrollment D-Bus requests contain an
authorized source-image path and identity metadata, never a database path or secret.

The current AI adapter creates a random AES-256 key, stores it as a mode-0600 AI-owned
file inside a mode-0700 AI-owned directory, and encrypts/authenticates snapshots with
AES-256-GCM and a fresh nonce. This provides process-account/filesystem isolation and
tamper detection. It is not evidence of a hardware-bound key. A future Qualcomm
TEE/keystore provider replaces only the adapter's key provider; FW does not become the
gallery owner.

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
Success means the complete replacement has been written, fsynced and renamed in the same
directory; a stale expected revision changes nothing. A replacement revision is strictly
greater; one atomic operation may advance once per affected template so the derived index
can reach the identical revision.
The adapter creates a missing first snapshot, then authenticates/decrypts every existing
snapshot and fails closed on truncation, corruption, unsafe ownership/mode or wrong key.

After durable commit, AI APP synchronizes a derived Zvec generation to the same revision.
Search is unavailable while revisions differ. During serialized startup AI APP loads the
authoritative snapshot, destroys only the disposable existing Zvec collection under
explicit rebuild policy, creates an empty collection, replays all records and verifies the
final revision before exposing recognition. Failure never deletes or rolls back the
durable snapshot and never falls back to an unverified old collection. Atomic generation
replacement for concurrent online rebuild remains a later requirement.

## Acceptance

- add several templates for one subject, restart, and recognize at the same revision;
- remove a subject atomically and prove no old template is searchable after publish;
- crash before/after durable rename and during index rebuild;
- reject stale CAS, corrupt ciphertext, wrong model/preprocess revision and quota overflow;
- demonstrate that logs, D-Bus replies and diagnostics contain no embedding or key;
- measure load, commit, rebuild and query latency at admitted gallery capacity;
- qualify a hardware-backed key provider before claiming device-bound protection.
