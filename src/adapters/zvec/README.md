# Zvec embedding index adapter

Implements the neutral embedding_index_port using the external Zvec v0.7.0 C API.
Enable VQEC_VISION_AI_ENABLE_ZVEC and supply VQEC_VISION_AI_ZVEC_ROOT.
The separate target is vqec_vision_ai_zvec_embedding; neutral perception has no Zvec dependency.

Fresh derived collections only; encrypted authoritative gallery/recovery remains open.
Cosine distance is converted to similarity. Ambiguous mutation failure faults the instance.
Vendor calls are serialized and blocking; this is not yet suitable for the camera callback.
See docs/adr/0004_fr_gallery_and_vector_index.md for qualification gates.
