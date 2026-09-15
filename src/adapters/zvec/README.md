# Zvec embedding index adapter

Implements the neutral embedding_index_port using the external Zvec v0.7.0 C API.
Enabled by default. Run `bash tools/vqec_vision_prepare_zvec.sh` to acquire the pinned
public ARM64 SDK under third_party/zvec/sdk. An alternate reviewed SDK may be supplied
through VQEC_VISION_AI_ZVEC_ROOT. The C API links libzvec_c_api.so.
The separate target is vqec_vision_ai_zvec_embedding; neutral perception has no Zvec dependency.

Fresh derived collections only; encrypted authoritative gallery/recovery remains open.
Cosine distance is converted to similarity. Search requests return the validated opaque
subject reference stored in each document and apply stable similarity/record ordering.
Ambiguous mutation failure faults the instance.
Vendor calls are serialized and blocking; this is not yet suitable for the camera callback.
See docs/adr/0004_fr_gallery_and_vector_index.md for qualification gates.

Real library integration passed eSDK QEMU and QCS6490 .48 on 2026-09-15.
