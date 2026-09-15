# embedding

Versioned face/body embedding index boundary for recognition and retrieval workflows.

- **Status:** neutral index port and bounded exact-cosine reference backend delivered
- **Naming registry:** `embed`
- **Depends on:** detection, alignment and attribute framework

## Limits and next work

- FR identity candidates are separate from visual attributes and local track IDs.
- Search pins the exact gallery revision and embedding model version; mutations use
  compare-and-swap revisions.
- FAISS remains a derived optional backend. Its GPU implementation requires CUDA/ROCm and
  cannot run on QCS6490 Adreno.
- Blacklist/attendance still require durable encrypted gallery storage, enrollment,
  calibrated thresholds and temporal policy.
- Per-attribute entitlement/privacy and purge/retain policy are FW-owned.

## See also

- [Feature catalog](../../../docs/architecture/feature_catalog.md)
- [FR gallery/index ADR](../../../docs/adr/0004_fr_gallery_and_vector_index.md)
