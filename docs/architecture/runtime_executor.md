# Runtime executor and device-free harness

`runtime_executor` is the serialized seam that connects the completed acquisition
orchestration to the perception/feature pipeline. Before it existed, the runtime
composition built source sessions and perception bundles but never drove the result router.
It closes that gap.

**Status:** source-delivered — source-delivered orchestration and device-free development
backends. This is a development harness, not a model, board or performance qualification.
**Layer:** app.
**Source:** `src/app/supervision/vqec_vision_runtime_executor.{hpp,cpp}`,
`src/app/service/bootstrap/vqec_vision_service_main.cpp`,
`src/app/service/bootstrap/vqec_vision_service_options.{hpp,cpp}`,
`src/adapters/reference/vqec_vision_reference_source.{hpp,cpp}`,
`src/adapters/reference/vqec_vision_reference_graph.{hpp,cpp}`.

## Responsibility

- Drive the application composition, take tensor results, rebuild a pump-shaped report and
  route results to the perception/feature pipeline.
- Hold one completed tensor and its routed output in one pending slot until `take_result`
  transfers tracked observations, feature-event batches and the report together, once.
- Must not reconstruct source identity from tensor data, authorize output or publish
  results; authorization and delivery remain downstream policy.

## Step pipeline

```text
runtime_executor.step
  -> application_composition.step            (multi-source round robin)
  -> take tensor result + source session progress
  -> rebuild multi_model_pump_report from progress (no re-derived source identity)
  -> multi_model_feature_pipeline.process_result
       -> multi_model_result_router -> decoder -> tracker   (tracked observations)
       -> feature_fanout -> feature_stage -> processor       (feature events)
```

A completed tensor and its routed output occupy one pending slot. `take_result`
transfers the tracked observations, feature-event batches and report together, once.
While occupied, further steps return `pending` so the caller cannot silently drop a
result. Taking a result is not permission to publish it.

## Source-to-model correlation

The executor never reconstructs source identity from tensor data. `source_session_progress`
already carries the routed model slot (`model_slot_`), the correlated submission ticket
(`ticket_`), the error slot and the result flag. The executor copies those into a
pump-shaped report so the neutral router and result stage can re-validate the
ticket/tensor correlation (result pipeline PTS must equal the ticket pipeline PTS).

## Owner graph

`runtime_composition_bundle` owns one `multi_model_feature_pipeline` per source, built
from that source's perception result router, and the executor that borrows the
composition and those pipelines. Feature fan-out wiring is optional and borrowed:
`runtime_feature_activation.sources_[source].fanouts_[model]` may point at a configured
`feature_fanout`, or be null when that model has no direct feature consumer. The feature
activation manager, fan-outs and package registries are caller-owned and must outlive the
bundle; the composition must reach `stopped` before they are destroyed.

## Device-free backends

`reference_raw_source` and `reference_inference_graph` implement the neutral ports with
deterministic synthetic data:

- the source advances a monotonic buffer id, epoch and source PTS and returns a
  descriptor for a packed linear NV12 frame; its native handle stands in for a FW FD and
  is never imported or read;
- the graph implements the full lifecycle and produces zero-valued tensors shaped like
  the declared outputs, with the ticket pipeline PTS.

They let composition, decode, tracking, feature fan-out, the executor and the service
harness run without a camera, QNN plugin, model artifact or board. They prove wiring and
lifecycle only: no accuracy, zero-copy, DMA completion or performance claim follows.

## Service harness

`vqec_ai_vision_applications` (source
`src/app/service/bootstrap/vqec_vision_service_main.cpp`) is the
composition root for the required external executable. It loads a validated deployment
and model catalog (and optional feature catalog), builds platform owners for every
deployment source/model slot, registers the platform's decoder/tracker/feature package
set, optionally activates `single_model` features, then runs and drains the executor loop
until `--steps` or SIGINT/SIGTERM.

The loop advances on the real `std::chrono::steady_clock`; a wall-clock step is never a
fabricated counter. The first error is latched and reflected in the process exit status,
and a result still pending at stop is consumed during drain.

`--mode harness` (default) selects the device-free fake platform implicitly. `--mode
production` requires an explicit `--platform` and never falls back: `fake` and `reference`
are device-free owners, `qualcomm` selects the wired Camera/QNN production owner, and any
unwired platform name exits non-zero instead of substituting the fixture package set.

For a dependency-activated secondary model, the Qualcomm production service resolves the
neutral cascade binding, starts its graph before primary activation, configures the
coordinator from package/graph metadata and binds it to the catalog-derived primary slot.
Shutdown drains the primary composition and retained tasks before the secondary graph is
drained and unloaded. Startup or stop failure remains visible as a non-zero service result.

It is built only when the JSON loaders and the reference backend are enabled. The
development fixture set is not a usecase and must be replaced during integration.

## Integration seam for model/usecase packages

A real integration replaces the harness in three places without touching the executor:

1. Platform owners: supply Camera/Qualcomm `raw_source_port` and `inference_graph_port`
   instances instead of the reference backends.
2. Package factories: register concrete `model_decoder_port`, `tracker_factory_port`
   and `feature_processor_factory_port` implementations under the catalog contracts.
3. Feature wiring: reconcile a real feature activation and pass the resulting
   `feature_fanout` pointers through `runtime_feature_activation`.

Each contract, registration key and ownership rule is unchanged from the pre-existing
model/feature/tracker contracts. `temporal_join` features remain a documented
activation-time gap: the harness wires only `single_model` features and rejects nothing,
it simply does not join across models yet.

## Cascade result routing

The composition factory derives at most one cascade-root model slot per source from exact
catalog dependencies. Before the first executor step, that source must bind one configured
`cascade_execution_worker` (production) or a synchronous coordinator (compatibility tests).
An unbound required cascade fails before source acquisition. After decode/tracking, production
schedules only that primary slot; subsequent steps poll the bounded completion queue before
source progress. A completion preserves frame identity, geometry, tracked observations and
the captured policy revision. The output gate is bound even for FR-only operation with no
feature fan-out; identity output revalidates that revision. Embedding values remain internal and are
cleared when the routed result is taken or discarded; the recognition owner may move them
through the explicit embedding take variant while keeping them behind its policy gate. They
are not logged or published by this boundary. A primary decode failure still invokes the
coordinator with the exact ticket identity and an empty batch so retained-frame admission
closes and shutdown cannot leak it.
Multiple faces from one primary result keep that same source identity; the secondary graph
uses distinct job tickets under its explicit repeated-task sequence policy.

## Metrics

`vqec_vision_ai_appl_rtexe_get_metrics` returns cumulative counters (steps, results routed,
events accepted/denied/failed and cascade accepted/embedded/failed) plus a routed-result
latency accumulator. The accumulator uses the submission ticket's `submitted_steady_ns_`
(reservation) and the executor steady clock, so it is `route_latency_*` — the steady
interval from job reservation to result routing. It is not camera-to-output latency and
excludes FW capture and preview encode. The ticket keeps `pipeline_pts_ns_` separately for
encoder correlation; the two clock domains are never mixed. Per-stage histograms and a
metrics sink/transport remain open.

## Limits and next work

- No authenticated catalog/artifact resolution, signature or TOCTOU protection; loaders
  validate structure only.
- The harness assumes one uniform source geometry for its fixture decoder.
- Feature events can be authorized and accepted by an optional bound output gate and
  feature-event sink. The production seam is bounded but not durable; shutdown discard is
  explicit. The reference sink is a development placeholder, not FW transport,
  durability, dedup or retry.
- Production cascade uses one joined worker per admitted source; secondary tasks within one
  graph are serialized. Pending, executing and completed batches share one bounded budget.
  Stop closes admission, joins completed worker work, then reconciles primary/source and
  secondary graph owners. A timed-out join never detaches or frees the worker; destruction
  of an unresolved worker is fail-stop, not a BSP reset or cancellation guarantee.
- No supervision/IPK packaging beyond SIGINT/SIGTERM handling.
- `temporal_join` features remain an activation-time gap; the harness wires only
  `single_model` features.
- Per-stage histograms and a metrics sink/transport remain open.

## See also

- [multi-model pump](multi_model_pump.md)
- [runtime feature activation](runtime_feature_activation.md)
- [reference platform](reference_platform.md)
- [perception result stage](perception_result_stage.md)
