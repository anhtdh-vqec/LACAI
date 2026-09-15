# ADR 0004: FR gallery authority and vector-index backends

Status: accepted for implementation, 2026-09-15.

## Context

Face recognition needs enrollment storage, embedding-version isolation, search, matching
policy and attendance state. A vector index alone is not an authoritative gallery: an
index may be rebuilt, partially updated or unavailable while identity metadata and
retention rules must remain consistent.

The selected backend is Zvec, replacing the earlier FAISS plan at the AI lead's
request. The adapter targets the reviewed upstream v0.7.0 C API. QNN HTP remains
the inference backend; Zvec search has no measured Adreno acceleration evidence.

## Decision

The FR usecase owns embedding validation, model/version compatibility, gallery revision,
index synchronization, search semantics, calibrated match policy and attendance temporal
logic. Durable encrypted identity/gallery persistence remains behind a neutral storage
port and the FW-owned protected-storage integration boundary. The Zvec collection is a
derived accelerator and is never the sole stored identity record.

All mutations use compare-and-swap gallery revisions. Search requests pin the required
revision and exact embedding model identity. An index with a different revision or model
version fails closed instead of returning stale candidates. Search candidates carry the
opaque bounded `subject_ref` stored with each record so matching can aggregate templates
without guessing identity from record IDs. Names, biometric vectors and credentials are
not logged.

`embedding_index_port` is vendor-neutral. The delivered exact cosine backend establishes
the contract and is suitable only within an admitted bounded gallery. Zvec stays under
`src/adapters/zvec`; perception and FR depend only on the neutral port.

Zvec v0.7.0 is enabled by default; its pinned public ARM64 SDK is acquired under
third_party/zvec/sdk. The adapter uses the C API behind embedding_index_port. Existing
collections are rejected because revision/record bookkeeping is currently in memory;
journaled authoritative-store recovery is required before restart reuse is enabled.

## Consequences

Other vector-index adapters can implement the same neutral port.
QCS6490 remains portable and correct, while its gallery capacity/latency must be measured
before choosing exact CPU search or another Qualcomm-supported accelerator. Durable store
commit and derived-index update need a journaled recovery protocol before enrollment is
production-ready.
Zvec provides an in-process ARM64-capable candidate backend, but this repository does not
vendor its source or claim QCS6490 performance/availability without a target build and
measurement.


## Adapter qualification gates

The v0.7.0 cosine result is a distance: convert it to similarity with
`1 - distance` before applying the neutral threshold. Failed mutations invalidate
the running adapter because the backend may have committed despite returning an error.
Reconfiguration of that instance is prohibited; recovery rebuilds a fresh collection.
No existing collection is deleted automatically.

C API query/document allocation and blocking vendor calls still require a bounded worker
and measured allocation/latency budgets before production activation. The optional source
is not yet an end-to-end FR persistence implementation.

## Real-library validation

The pinned public Linux ARM64 v0.7.0 SDK is now acquired under third_party/zvec/sdk
using the checksum-verified bootstrap. CMake enables the adapter by default and links
libzvec_c_api.so. On 2026-09-15 the eSDK-built integration executable passed both QEMU
and native QCS6490 .48 execution (zero failed checks). Existing collection recovery,
authoritative encrypted enrollment and live FD-to-FR composition remain open.
The upstream library is a release binary; only LACAI was compiled with the eSDK.
