# embedding

Versioned face/body embedding index boundary for recognition and retrieval workflows.

- **Status:** neutral index port and bounded exact-cosine reference backend delivered;
- **Layer:** perception
  Zvec adapter built by default, verified with the real SDK under eSDK QEMU and a
  historical QCS6490 run; current board target is `.98`.
- **Naming registry:** `embed`
- **Depends on:** detection, alignment and attribute framework

## Limits and next work

- FR identity candidates are separate from visual attributes and local track IDs.
- Search pins the exact gallery revision and embedding model version; mutations use
  compare-and-swap revisions.
- Search candidates include their opaque `subject_ref` alongside `record_id` and similarity;
  consumers may aggregate templates by subject without treating a record ID as identity.
- The model-agnostic recognition policy aggregates candidate templates by subject and emits
  configurable known/unknown/ambiguous decisions. Backend failures stay unavailable and are
  never silently converted to unknown.
- `recognition_session` owns bounded multi-template gallery mutations, revision-CAS
  enrollment/removal, correlated search and exact-frame label application. It is the
  neutral seam used by the DBus enrollment adapter and by either the reference or Zvec
  index backend.
- Zvec replaces the earlier FAISS plan; vendor code lives in `src/adapters/zvec`.
- Zvec is a derived optional backend. It is enabled only with an externally supplied,
  version-reviewed C API installation; the collection path is deployment configuration,
  never a source-tree default. Search remains pinned to the in-memory gallery revision and
  embedding model identity. Production uses explicit rebuild policy only after the
  authenticated authoritative snapshot is loaded, preventing stale matches after restart.
- `recognition_session::configure_persistent` consumes the authoritative face-gallery
  snapshot and protected-store port, rebuilds a fresh index at the durable revision and
  commits mutations before derived-index updates.
- The authoritative face-gallery snapshot and protected-store port now define bounded
  model/preprocess identity plus atomic revision CAS. The AI AES-256-GCM store and Zvec
  restart rebuild are wired into the service.
- Blacklist/attendance still require calibrated thresholds, temporal policy and event
  persistence.
- Per-attribute entitlement/privacy and gallery purge/retain policy are AI-owned; FW
  transports commands and consumes authorized results.

## See also

- [Feature catalog](../../../docs/architecture/feature_catalog.md)
- [FR gallery/index ADR](../../../docs/adr/0004_fr_gallery_and_vector_index.md)
