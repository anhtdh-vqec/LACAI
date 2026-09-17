# Zvec embedding index adapter

Implements `embedding_index_port` through the pinned external Zvec v0.7.0 C API.
The collection is derived from AI's authenticated encrypted gallery and has no authority.

- **Status:** source-delivered — real-library tests pass eSDK/QEMU and QCS6490 `.98`
- **Layer:** adapters
- **Naming registry:** `zvec` (`zvidx`)
- **Depends on:** neutral core/contracts and external Zvec C API
- **Used by:** production recognition composition through `embedding_index_port`

## Responsibility

- Serialize bounded revision-pinned insert/search/remove and fail closed on ambiguous mutation.
- Convert cosine distance to similarity and return validated opaque subject references.
- Default to private volatile storage; never silently use persistent plaintext for production.
- Rebuild only after the authoritative snapshot is authenticated and model/version-validated.

## Contents

| Path | Purpose |
|---|---|
| `vqec_vision_zvec_embedding_index.cpp` | C API adapter; private directory binding, collection lifecycle and query/mutation validation |
| `vqec_vision_zvec_embedding_index.hpp` | Adapter-private policies and neutral index implementation |

Zvec is enabled by default. `bash tools/vqec_vision_prepare_zvec.sh` acquires the checksum-
verified public ARM64 SDK under `third_party/zvec/sdk`; an alternate reviewed SDK may be
provided through `VQEC_VISION_AI_ZVEC_ROOT`. Target `vqec_vision_ai_zvec_embedding` links
`libzvec_c_api.so`; perception has no Zvec dependency.

Production collection paths must be absolute and normalized, below an existing service-
UID-owned mode-0700 tmpfs parent. The adapter opens the parent without following a symlink,
validates UID/mode/filesystem, pins its FD and addresses the collection through Linux
`/proc/self/fd`. A symlink leaf or unowned collection is rejected. Created collection mode
is 0700. The private collection is destroyed before releasing the directory FD when its
owner closes; failures are reported. Disabling FR can therefore release derived tmpfs files
as well as process resources. Durable encrypted gallery/key files are never index cleanup
inputs. Persistent synthetic library tests select `synthetic_filesystem_fixture` explicitly;
production service exposes no switch for that policy.

## Limits and next work

- Vendor calls allocate and block; bounded worker/capacity/performance qualification remains.
- Tmpfs contents are plaintext in RAM; BSP must qualify swap, crash dumps, root isolation
  and hardware-backed key policy. Filesystem identity checks do not authenticate deployment.
- Same-UID/root actors and malicious vendor code remain inside the trusted execution boundary.
- There is no measured Adreno acceleration for Zvec; QNN HTP handles model inference.

## See also

- [ADR 0004](../../../docs/adr/0004_fr_gallery_and_vector_index.md)
- [FR validation](../../../docs/testing/face_recognition_production_validation.md)
