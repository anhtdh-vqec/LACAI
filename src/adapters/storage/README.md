# Storage adapters

Implements AI APP-owned durable adapters for the protected face gallery, transactional metadata,
version 1 sharded spatiotemporal store and application lifecycle state/content.

- **Status:** logic-tested — metadata v1 is accepted; lifecycle, evidence, identity and metadata
  implementations are physically isolated behind their neutral ports
- **Layer:** adapters
- **Naming registry:** `stor` (`eglry`, `mdsql`, `stsql`, `apinv`, `apcst`)
- **Depends on:** neutral contracts, OpenSSL 3.0 `libcrypto` and SQLite 3
- **Used by:** recognition composition and the source-delivered bounded metadata service

## Responsibility

- Load and validate one bounded encrypted snapshot; corrupt or tampered ciphertext fails
  closed before any index rebuild or identity can be served.
- Guard mutations with an interprocess lock and an on-disk revision CAS, then write
  same-directory temp + fsync + atomic rename.
- Enforce owner-only files (0600) and an owner-only directory (0700), rejecting symlinked
  or group/other-accessible paths.
- Commit each metadata revision and its bounded delivery-outbox rows atomically, then serve
  authorized prepared queries through stable snapshot/keyset paging.
- Persist high-rate trajectory points as checked packed chunks in time shards, with a small
  catalog index, explicit shard manifests, stable sequences and exact path verification.
- Keep metadata I/O outside frame, inference, DSP and renderer workers.
- Publish verified application blobs by digest through owner-only staging, file/directory fsync and
  no-replace linking; never infer inventory references by scanning content.

## Contents

| Path | Purpose |
|---|---|
| `identity/vqec_vision_encrypted_face_gallery_store.*` | AES-256-GCM gallery store, key lifecycle, lock/CAS and atomic replacement |
| `metadata/vqec_vision_sqlite_metadata_store.*` | SQLite WAL record/outbox transaction and authorized query facade |
| `metadata/vqec_vision_spatiotemporal_store*` | Catalog, packed detail shards, associations, projections and bounded queries |
| `evidence/vqec_vision_sqlite_evidence_outbox.*` | Durable deduplicated evidence command and receipt journal |
| `app_lifecycle/vqec_vision_sqlite_app_inventory.*` | Transactional app/config/authority/desired inventory and operation journal |
| `app_lifecycle/vqec_vision_app_content_store.*` | Bounded SHA-256-addressed immutable blob staging, recovery and removal |

## Key and file layout

The key is 32 random bytes created `O_EXCL | O_NOFOLLOW` at 0600 and fsynced with its
directory. Each write uses a fresh 12-byte random nonce and a 16-byte tag; the header is
authenticated as additional data (AAD). Payload size must equal `file - header - tag`.

## Limits and next work

- **AAD does not bind gallery/file identity.** The header is authenticated, but a same-UID
  actor who can write the protected directory could swap one valid ciphertext file for
  another and still pass authentication. Binding a gallery id/revision into the AAD, or
  storing the file under a hardware-rooted key, is required before treating the store as
  tamper-resistant against a same-UID adversary. This is a tracked release gate; see
  [FR validation](../../../docs/testing/face_recognition_production_validation.md).
- The key is a filesystem key, not hardware-bound (no TEE/keystore). Power-cut durability,
  backup/restore, schema migration and board qualification remain open.
- `flock` is advisory; a process that ignores it is not blocked. The directory permission
  and ownership checks are the primary defence.
- Key rotation is not implemented.
- The metadata adapter is composed through the validated production profile. Kafka delivery
  remains a Plan 3 consumer of the durable outbox/receipt API; Parquet is not a v1 edge dependency.
- Spatiotemporal shards use source PTS partitions and blocking calls behind the single metadata
  worker. Episode/contribution projections and materialized rollups are delivered. SIGKILL,
  disk-full, corruption and cancellation pass; long power/flash/thermal soak remains release work.
- The content store is wired to App Manager package ingest and inventory generation references.
  Safe garbage collection still requires the inventory owner to prove no current or rollback
  generation refers to a blob.

## See also

- [Spatiotemporal metadata](../../../docs/architecture/spatiotemporal_metadata.md)
- [Metadata transactional prototype](../../../docs/architecture/metadata_query.md)
- [Face recognition validation](../../../docs/testing/face_recognition_production_validation.md)
