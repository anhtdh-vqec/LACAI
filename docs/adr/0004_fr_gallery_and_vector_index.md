# ADR 0004: FR gallery authority and vector-index backends

Status: accepted for implementation, 2026-09-15.

## Context

Face recognition needs enrollment storage, embedding-version isolation, search, matching
policy and attendance state. A vector index alone is not an authoritative gallery: an
index may be rebuilt, partially updated or unavailable while identity metadata and
retention rules must remain consistent.

FAISS GPU cannot execute on the QCS6490 Adreno GPU. Upstream FAISS documents GPU support
through CUDA, with newer source also offering an AMD ROCm build; its distributed GPU
package is CUDA/x86-64. Qualcomm Linux 1.8 in the approved eSDK contains neither FAISS nor
a CUDA/ROCm runtime. See the official
[FAISS GPU documentation](https://github.com/facebookresearch/faiss/wiki/Faiss-on-the-GPU)
and [installation matrix](https://github.com/facebookresearch/faiss/blob/main/INSTALL.md).

## Decision

The FR usecase owns embedding validation, model/version compatibility, gallery revision,
index synchronization, search semantics, calibrated match policy and attendance temporal
logic. Durable encrypted identity/gallery persistence remains behind a neutral storage
port and the FW-owned protected-storage integration boundary. The FAISS file/index is a
derived accelerator and is never the sole stored identity record.

All mutations use compare-and-swap gallery revisions. Search requests pin the required
revision and exact embedding model identity. An index with a different revision or model
version fails closed instead of returning stale candidates. Subject references are opaque
bounded identifiers; names, biometric vectors and credentials are not logged.

`embedding_index_port` is vendor-neutral. The delivered exact cosine backend establishes
the contract and is suitable only within an admitted bounded gallery. A FAISS adapter may
select CPU, CUDA GPU or ROCm GPU from validated deployment capability; GPU selection must
fail when the requested runtime is absent. QCS6490 uses QNN HTP for FD/FR inference and a
separately qualified search backend. No Adreno acceleration claim is made for FAISS.

## Consequences

FAISS GPU can be enabled on a supported NVIDIA/ROCm deployment without changing FR logic.
QCS6490 remains portable and correct, while its gallery capacity/latency must be measured
before choosing exact CPU search or another Qualcomm-supported accelerator. Durable store
commit and derived-index update need a journaled recovery protocol before enrollment is
production-ready.
