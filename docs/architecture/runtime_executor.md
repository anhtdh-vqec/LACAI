# Runtime executor and device-free harness

Status: source-delivered orchestration and device-free development backends. This is a
development harness, not a model, board or performance qualification.

## Purpose

`runtime_executor` is the serialized seam that connects the completed acquisition
orchestration to the perception/feature pipeline. Before it existed, the runtime
composition built source sessions and perception bundles but never drove the result
router. The executor closes that gap:

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
result. Taking a result is not permission to publish it; authorization and delivery
remain downstream policy.

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

`vqec_ai_vision_applications` (source `src/app/vqec_vision_service_main.cpp`) is the
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
`cascade_coordinator`; an unbound required coordinator fails before source acquisition.
After decode/tracking, the executor invokes the coordinator only for that primary slot and
records bounded accepted/embedded/failed counts. Embedding values remain internal and are
cleared when the routed result is taken or discarded; they are not logged or published by
this boundary. A primary decode failure still invokes the coordinator with the exact ticket
identity and an empty batch so retained-frame admission closes and shutdown cannot leak it.

## Metrics

`vqec_vision_ai_appl_rtexe_get_metrics` returns cumulative counters (steps, results routed,
events delivered/denied/failed) plus an experimental routed-result latency accumulator.
The current accumulator compares the internal pipeline PTS with executor steady time; it
is meaningful only when an adapter explicitly maps those domains. The owned QNN graph uses
an identity anchor instead, so its printed `e2e_*` values must not be interpreted as
latency. A clock-domain contract, submit/capture timestamps, per-stage histograms and a
metrics sink/transport remain open.

## Limits

- No authenticated catalog/artifact resolution, signature or TOCTOU protection; loaders
  validate structure only.
- The harness assumes one uniform source geometry for its fixture decoder.
- Feature events can be authorized and delivered through an optional bound output gate
  and feature-event sink; the reference sink is a development placeholder, not FW
  transport, durability, dedup or retry. No overlay/encoder path is driven yet and the
  encoded sink is not attached.
- No service threads, signals beyond SIGINT/SIGTERM, or supervision/IPK packaging.
