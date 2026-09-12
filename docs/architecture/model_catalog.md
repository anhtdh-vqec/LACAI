# Model catalog configuration v1

Status: neutral contract, pure validation, bounded JSON loader and source tests delivered;
artifact authentication/resolution and live graph composition are pending.

## Ownership and separation

AI Model team delivers a versioned catalog. Each entry represents one executable model
graph, not a commercial feature and not a camera. It declares:

- model/version/target and immutable artifact reference plus SHA-256 claim;
- output-manifest, decoder and preprocess contract references;
- graph name, exact tensor input and normalization;
- desired inference cadence and supported source resolution/FPS envelope;
- measured resident/tensor requirements, bounded output queue, maximum concurrent
  sources and whether a context is proven shareable across sources.

The catalog contains no absolute filesystem path, QNN library path, FW RAW-source or
transport details, FW ring identity, entitlement or credential. A trusted platform resolver maps the
catalog's `(model_id, target_id, artifact_ref)` to immutable model/backend/system paths.
The resulting plan is accepted only when all three references still match.

Deployment owns source profile and total budgets. Catalog owns model semantics and its
measured envelope. Output manifest owns ordered raw tensor metadata. The runtime must
cross-check catalog identity, model/version/digest/decoder/output reference instead of
building one input by copying values from another untrusted document.

## Validation and admission

The pure cross-validator first validates both deployment and catalog, then requires:

- every deployment `model_id` exists in the referenced catalog;
- each source width/height/FPS lies inside the model's declared envelope and can supply
  at least the requested inference cadence;
- assignment count does not exceed `max_concurrent_sources`;
- summed tensor requirement per source fits its `max_tensor_bytes`;
- resident memory counts once only when `can_share_context_across_sources=true`; otherwise
  it is multiplied by assigned source count and must fit deployment model budget.

`can_share_context_across_sources` is a capability claim requiring backend/thread-safety
and board evidence. It does not authorize concurrent calls automatically; scheduler and
adapter still serialize or parallelize according to the signed target report.

The current single-image plan composer combines source profile/budget, catalog entry and
resolved paths transactionally. It never takes source dimensions from the model catalog
and never takes tensor/preprocess values from deployment configuration.

## JSON boundary

Schema: `config/schemas/model_catalog.schema.json`.
Synthetic handoff: `manifests/models/model_catalog.example.json`.

The strict startup loader is limited to 512 KiB and depth 16, rejects unknown/duplicate
keys and preserves outputs on failure. JSON Schema is review assistance; C++ validation
remains authoritative for rational-rate comparisons, uniqueness and checked memory sums.
Parsing does not verify a signature or digest, pin an open file, load executable code or
prove resource measurements.

The example digest and memory values are synthetic. Model team must replace them with an
accepted package and board report; product deployment must not install the example.

## Model class coverage (S08)

A new model inside a supported class is added by package/decoder/config only, never a
core/pump branch. A class is `implemented` only after a conformance fixture exists; the
rest is rejected with `unsupported` at activation, before hardware acquisition, rather than
inferred from a model name or a matching total tensor byte count.

| Class | Declared | Implemented in this base | Rejection point |
|---|---|---|---|
| Single image input, fixed shape | yes | QNN engine + reference; pump preprocessing path | n/a |
| Multi-input graph | contract only | no | `multi_model_pump::resolve_targets` -> `unsupported` |
| Dynamic shape (zero/absent dim) | contract only | no | `qnn_engine::prepare` -> `unsupported` (zero-byte shape) |
| Per-tensor quantization | yes | engine maps scale/offset; graph-native typed output | n/a |
| Per-axis quantization | not modeled | no | input/output identity mismatch -> reject |
| Batch > 1 | contract only | no | graph identity/first-dim mismatch -> reject |
| Stateful / temporal sequence | contract only (`temporal_join`) | no | feature catalog rejects a required join without a bounded window |
| Artifact / LoRA update | contract only | no | `capabilities.supports_artifact_update` is false |

Dynamic shape, stateful sequence and artifact update need an explicit envelope, pool
profile, generation/drain and source-epoch/reset policy before they can be enabled; the
descriptors exist but no conformance fixture or board evidence does.
