# Runtime composition factory

`runtime_composition_factory` is the cold-path bridge from one validated deployment and
model catalog to an activation-ready application owner graph. It builds an admission
snapshot, composes every model plan, binds exact output metadata, constructs the source
perception groups and multi-model sessions, then binds those sessions into one validated
`application_composition`.

The caller supplies platform owners through neutral ports. Each source activation borrows
one idle `raw_source_port`; each model activation borrows one empty
`inference_graph_port` and carries trusted resolved paths, the resolved output-manifest
reference, parsed output metadata, tracker contract, source binding, cycle ID and job
timeout. The bundle owns the sessions, perception stages, trackers, admission snapshot
and application composition. RAW-source and graph owners, decoder registrations and
tracker factories must outlive it. After activation, the bundle must reach stopped before
those borrowed owners or the bundle are destroyed; destruction is never cancellation.

Composition is transactional and performs no Camera Start, model load or frame submit.
Before constructing owners it requires:

- exact deployment/catalog validation and source/model slot order;
- one source activation per deployment source and one model activation per assigned model;
- distinct RAW-source port instances across source slots: two sessions must never
  control the same source owner, even if deployment source IDs differ;
- unique graph ports and nonzero, nonsentinel cycle IDs across the whole application;
- an idle RAW source and empty, activation-capable inference graph for every slot;
- catalog/source/path agreement through the neutral inference-plan composer;
- exact catalog output reference, identity, artifact digest, decoder contract and bounded
  tensor schema;
- source binding compatible with the plan, with its preprocess contract equal to the
  catalog model's preprocess contract;
- nonzero bounded lifecycle, RPC and per-job timeouts;
- decoder-specific output validation and tracker construction through the existing
  registries.

Failure preserves the caller's previous bundle. Candidate trackers and coordinating
objects created before a later construction failure are destroyed in ownership-safe
order. Port validation may inspect owner state but must not acquire hardware resources.

The returned application composition is already in `validating` state and can be
activated by the serialized service executor. A tensor result is routed using the source
index and model slot in its progress report to the corresponding perception bundle.
Feature activation, entitlement, artifact authentication, immutable path opening,
platform owner creation and measured board admission remain separate prerequisites.
No Qualcomm, GStreamer, Camera wire or product-origin type crosses this boundary.
