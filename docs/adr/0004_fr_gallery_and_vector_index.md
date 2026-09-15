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
version fails closed instead of returning stale candidates. Subject references are opaque
bounded identifiers; names, biometric vectors and credentials are not logged.

`embedding_index_port` is vendor-neutral. The delivered exact cosine backend establishes
the contract and is suitable only within an admitted bounded gallery. Zvec stays under
`src/adapters/zvec`; perception and FR depend only on the neutral port.

The implementation also provides an optional Zvec C API adapter. Zvec is selected only
when an externally supplied, version-reviewed installation is present and the deployment
path is validated. Its collection is a derived index: it is not the authoritative
encrypted gallery and must fail closed on a revision or embedding-model mismatch.
The current adapter creates a fresh collection only; an existing collection is rejected
because its gallery revision cannot be authenticated by the index alone. Journaled
recovery is required before restart reuse is enabled.

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

Validation for this source step: eSDK AArch64 syntax checks pass against the reviewed
v0.7.0 header, including the optional synthetic integration test. The eSDK full suite
passes 95/95 with Zvec disabled. No libzvec was found in the eSDK or temporary dependency
area; therefore linking and the Zvec integration test have NOT run. The optional test
checks same/orthogonal/opposite vectors, threshold conversion, revision rejection and
delete visibility when an approved target library is supplied.
