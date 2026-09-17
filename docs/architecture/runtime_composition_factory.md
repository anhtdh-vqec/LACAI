# Runtime composition factory

`runtime_composition_factory` is the cold-path bridge from one validated deployment and
model catalog to an activation-ready application owner graph. It builds an admission
snapshot, composes every model plan, binds exact output metadata, constructs the source
perception groups and multi-model sessions, then binds those sessions into one validated
`application_composition`.

**Status:** source-delivered — composition-root construction delivered, including
source/model activation, perception groups and multi-model sessions.
**Layer:** app.
**Source:** `src/app/vqec_vision_runtime_composition_factory.{hpp,cpp}`.

## Responsibility

- Build one admission snapshot and compose every model plan from a validated deployment
  and model catalog.
- Bind exact output metadata, construct source perception groups and multi-model sessions,
  and bind those sessions into one validated `application_composition`.
- Must not perform a Camera Start, model load or frame submit during composition; no
  Qualcomm, GStreamer, Camera wire or product-origin type crosses this boundary.

## Ownership

The caller supplies platform owners through neutral ports. Each source activation borrows
one idle `raw_source_port`; each model activation borrows one empty
`inference_graph_port` and carries trusted resolved paths, the resolved output-manifest
reference, parsed output metadata, tracker contract, source binding, cycle ID and job
timeout. The bundle owns the sessions, perception stages, trackers, admission snapshot
and application composition. RAW-source and graph owners, decoder registrations and
tracker factories must outlive it. After activation, the bundle must reach stopped before
those borrowed owners or the bundle are destroyed; destruction is never cancellation.

## Transactional prerequisites

Composition is transactional. Before constructing owners it requires:

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

## Routing and execution policy

The returned application composition is already in `validating` state and can be
activated by the serialized service executor. A tensor result is routed using the source
index and model slot in its progress report to the corresponding perception bundle.
The activation descriptor also selects serialized source stepping or one bounded, joined
worker per source. Production may use the worker policy so blocking preprocess/inference
does not block control, cascade processing and preview rendering; deterministic fixtures
retain serialized stepping. This policy does not add model concurrency within one source.
The output thread takes a session preview only after it has consumed a worker completion;
a pending executor step may mean the worker is mutating the session, so direct mailbox
access is forbidden then. Cascade processing follows the same completed-result boundary.
Independently, activation may enable the per-model workers defined by
[multi-model pump](multi_model_pump.md). The two switches are separate because source-level
overlap and model-level overlap have different ownership and accelerator-contention costs.
The executable's polling interval is an independent deployment setting: shortening it
does not change source/model cadence, queue capacity or ownership, but can reduce the
accumulated delay between submit, harvest, result routing and the next receive. It must
remain positive and requires CPU/thermal measurement rather than an implicit busy loop.

## Limits and next work

- Feature activation, entitlement, artifact authentication, immutable path opening,
  platform owner creation and measured board admission remain separate prerequisites.
- Composition performs no Camera Start, model load or frame submit; those happen after
  activation.
- The two execution switches (source-level worker policy and per-model pump workers) must
  remain separate because of different ownership and accelerator-contention costs.

## See also

- [multi-model pump](multi_model_pump.md)
- [application composition](application_composition.md)
- [runtime executor](runtime_executor.md)
