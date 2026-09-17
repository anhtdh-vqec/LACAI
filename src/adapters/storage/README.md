# Encrypted face-gallery store adapter

Implements `face_gallery_store_port` over an AI-owned AES-256-GCM authenticated snapshot.
It is the authoritative durable gallery; the Zvec index is derived from it and has no
authority.

- **Status:** source-delivered — encrypted restart recognition, stale CAS and tamper
  rejection pass eSDK/QEMU and on QCS6490 `.98`
- **Naming registry:** `stor` (`eglry`)
- **Depends on:** neutral contracts and OpenSSL 3.0 `libcrypto`
- **Used by:** production recognition composition through `face_gallery_store_port`

## Responsibility

- Load and validate one bounded encrypted snapshot; corrupt or tampered ciphertext fails
  closed before any index rebuild or identity can be served.
- Guard mutations with an interprocess lock and an on-disk revision CAS, then write
  same-directory temp + fsync + atomic rename.
- Enforce owner-only files (0600) and an owner-only directory (0700), rejecting symlinked
  or group/other-accessible paths.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_encrypted_face_gallery_store.cpp` | OpenSSL AES-256-GCM store, key lifecycle, lock/CAS and atomic replacement |
| `vqec_vision_encrypted_face_gallery_store.hpp` | Adapter-private configuration and neutral store implementation |

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
