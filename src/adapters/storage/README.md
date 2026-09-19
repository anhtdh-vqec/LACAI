# Storage adapters

Implements AI APP-owned durable adapters for the protected face gallery, transactional metadata
prototype, version 1 sharded spatiotemporal store and application inventory.

- **Status:** source-delivered — metadata v1 is accepted; gallery and application inventory keep
  their own release gates
- **Layer:** adapters
- **Naming registry:** `stor` (`eglry`, `mdsql`, `stsql`, `apinv`)
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

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_encrypted_face_gallery_store.cpp` | OpenSSL AES-256-GCM store, key lifecycle, lock/CAS and atomic replacement |
| `vqec_vision_encrypted_face_gallery_store.hpp` | Adapter-private configuration and neutral store implementation |
| `vqec_vision_sqlite_metadata_store.cpp` | SQLite WAL record/outbox transaction and authorized query facade |
| `vqec_vision_sqlite_metadata_store.hpp` | Adapter configuration and blocking storage API |
| `vqec_vision_spatiotemporal_store.cpp` | SQLite catalog, packed detail shards, association revisions, recovery and bounded queries |
| `vqec_vision_spatiotemporal_store.hpp` | Store configuration, quota, stats and blocking storage API |
| `vqec_vision_sqlite_app_inventory.*` | Transactional app/config/authority/desired inventory and runtime snapshot |

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

## See also

- [Spatiotemporal metadata](../../../docs/architecture/spatiotemporal_metadata.md)
- [Metadata transactional prototype](../../../docs/architecture/metadata_query.md)
- [Face recognition validation](../../../docs/testing/face_recognition_production_validation.md)
