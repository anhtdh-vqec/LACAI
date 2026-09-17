# Multi-source configuration and memory architecture v1

This document is the normative contract for 1..16 logical sources in one AI APP process:
the deployment shape, the independent configuration authorities, the deployment document,
conservative memory admission and the copy/zero-copy ledger. It is normative for new
runtime work.

**Status:** source-delivered — source-level contract, bounded loader, activation snapshot
and bounded session supervisor, multi-model session/fan-out and bounded RAW-reference
resolver delivered; composition-root construction is delivered; authenticated FW registry
RPC and board qualification remain pending. **Layer:** runtime.
**Source:** `src/runtime/lifecycle/vqec_vision_deployment_config.{hpp,cpp}`,
`src/runtime/admission/vqec_vision_activation_snapshot.{hpp,cpp}`,
`src/app/vqec_vision_multi_source_supervisor.{hpp,cpp}`,
`config/schemas/deployment.schema.json`.

## Responsibility

- Define the 1..16 logical-source deployment shape and the one-FW-RAW-source rule for AI
  Camera and AI Box.
- Own the deployment document contract/loader, the activation snapshot and conservative
  memory admission.
- Keep transport origin (RTSP/codec/credential) and product-type inference out of AI APP;
  `raw_source_ref` resolves only to the agreed FW RAW frame contract.

## Deployment shape

One AI APP process supports **1..16 logical sources**. This is a configuration ceiling,
not a claim that one board can run 16 sources, 16 previews or every model concurrently.
Admission uses the selected board, effective profiles, model catalog, encode/record load,
thermal policy and measured memory/latency data.

Each source owns a serialized lifecycle/state executor, epoch, bounded in-flight window,
tracker and temporal state. Models may share an immutable artifact/context only when the
backend proves compatibility and thread safety. A model assignment never creates another
Camera Service acquisition for the same logical source.

```text
AI Camera: sensor/ISP/Camera Service ----+
                                         +-> FW unified RAW source contract
AI Box: RTSP -> FW demux/decode ---------+           |
                                                    v
                         raw_source_ref + NV12/FD descriptor/lease
                                                    |
                                      AI source adapter -> scheduler -> model graph(s)
```

For AI APP, AI Camera and AI Box inputs are identical. RTSP discovery, credentials,
demux, decode, decoder surfaces and reconnect-before-RAW belong entirely to FW. AI APP
does not contain an RTSP adapter or codec-decoder lifecycle and must not infer product
type from a source. `raw_source_ref` resolves only to the agreed FW RAW frame contract.

## Three independent configuration authorities

| Document | Owner | Contains | Must not contain |
|---|---|---|---|
| Deployment configuration | AI APP schema; FW/product supplies effective values | 1..16 `raw_source_ref` values, logical camera/channel, exact width/height/FPS, preview references, model assignments, memory ceilings, revision | transport origin, RTSP/codec/credential data, model tensor guesses, purchased-feature decision |
| Model catalog/integration package | AI Model; AI APP accepts | artifact identity/hash, target compatibility, input/preprocess, output tensors, model-output decoder, cadence/quality constraints and measured resource envelope | FW RAW source, ring identity, product entitlement |
| Feature/entitlement policy | FW/backend commercial control | desired features/attributes, license revision, privacy/output rights | model binary paths supplied by remote callers, raw secrets |

Startup loads each document from an authenticated local store, resolves references, then
cross-validates them. Source profile is authoritative for acquisition; the model catalog
is authoritative for tensor/pre/postprocess. If a model cannot accept the source through
an approved transform, admission fails with a reason. The runtime never invents a fallback
resolution, FPS, tensor shape, colorimetry or budget.

The model catalog contract, loader and deployment cross-validation are now source-delivered;
see [model catalog](model_catalog.md). Artifact/output-manifest authentication and the
runtime resolver that activates the resulting plans remain pending.

## Deployment document

The C++ contract is `vqec_vision_deployment_config.hpp`; the strict JSON representation
is `config/schemas/deployment.schema.json`. Required properties include:

- global `schema_version`, monotonic `revision`, `model_catalog_ref`, total/model resident
  memory ceilings;
- unique source ID, unique `raw_source_ref` and unique logical `(camera_id, channel_id)`;
- one FW RAW source abstraction for both AI Camera and AI Box; no input-kind branch;
- explicit even width/height and rational FPS; no 4K/1080p default;
- bounded frame/in-flight/preview/tensor/temporal memory and 1..16 model IDs;
- a unique preview output reference whenever preview surfaces are requested.

The loader is startup-only, bounded to 128 KiB and JSON depth 16, rejects unknown or
duplicate keys, and changes outputs only after full validation. Parsing does not authenticate
the file, resolve a model/RAW source/output, compare capability data or allocate a pool.

V1 applies profile or source-set changes by controlled source replacement:

1. validate and admit the complete new revision without mutating the running revision;
2. stop new submissions for affected sources and drain real device completions;
3. close the affected preview generation and release the FW RAW source lease;
4. increment source epoch, allocate exact new pools, bind models and start;
5. publish effective revision/state. On failure, remain stopped/degraded or roll back as
   product policy specifies; never reinterpret old buffers with new geometry.

## Conservative memory admission

The current validator calculates a declared upper bound:

```text
model resident ceiling
+ sum per source (
    max_frame_allocation_bytes * max_inflight_frames
  + packed_nv12_bytes * preview_surface_count
  + max_tensor_bytes
  + max_temporal_bytes)
```

`packed_nv12_bytes = width * height * 3 / 2` is only a lower-bound geometry check and a
preview estimate. `max_frame_allocation_bytes` must include real stride/padding/allocation.
Vendor graph pools, encoder pools, GStreamer objects, stacks and process overhead still
require board-specific accounting before admission. FW separately budgets its RTSP decoder
surfaces on AI Box. The 8 GiB parser ceiling
is a corruption guard, not a product default or a promise of available RAM.

Memory rules for implementation:

- allocate frame/tensor/preview/result pools at revision activation, not per frame;
- use move-only leases for exclusive resources and explicit shared ownership only for real
  fan-out; release a camera buffer after the last hardware reader completes;
- every queue and pool has capacity, byte ceiling, overflow policy and high-water metrics;
- keep full-resolution frames out of tracker/feature temporal history; retain compact
  metadata, embeddings or explicitly budgeted crops;
- avoid per-frame string formatting, map insertion, JSON and log allocation on the hot path;
  intern catalog identifiers at activation and reserve bounded vectors/arenas;
- do checked arithmetic before pool sizing, offset calculation or allocation.

After validation, `vqec_vision_activation_snapshot` materializes source-to-model mappings
into fixed `std::array` slots and 16-bit indices tied to exact deployment/catalog revisions.
The hot-path scheduler therefore need not compare strings or grow containers. Cold config
objects remain the owner of RAW-source/output names, artifact references and readable IDs.

`vqec_vision_multi_source_supervisor` uses those numeric source indices to advance one
already-composed session per call. It supplies bounded round-robin fairness and isolates a
source error while that session drains; it does not resolve RAW sources or create threads.
See [multi-source supervisor](multi_source_supervisor.md).

Within each source, [model cadence](model_cadence.md) compiles Model-team rational inference
rates into fixed numeric phase state and a 16-bit due mask. A skipped source-frame sequence
advances cadence without generating a backlog burst.

## Copy and zero-copy ledger

“Zero-copy” is a per-boundary result, never a label for the whole pipeline.

| Boundary | Intended v1 behavior | Current evidence/status |
|---|---|---|
| FW RAW NV12/FD -> GstMemory | duplicate/retain FD and wrap the same pixel allocation for Camera or Box | source exists for released Camera `third`; zero pixel memcpy is plausible only for real DMA-BUF with FW/BSP lifetime/sync proof; AI Box must satisfy the same contract |
| raw frame -> preprocess | import compatible DMA-BUF where possible; transform writes a pooled model input | Qualcomm graph source exists; device import/copy path needs board tracing |
| model input -> accelerator | backend-specific registered/shared buffer where supported | QNN ION/rpcmem registered buffers delivered; released-FW DMA evidence pending |
| tensor output -> decoder | prefer borrowed view during synchronous decode or a pooled result buffer | current Qualcomm helper copies mapped FLOAT32 output into owned vectors |
| camera frame -> burned-in preview | write/copy into an AI-owned surface; never modify FW read-only input | CPU preview pool and Qualcomm QTI renderer delivered; golden parity pending |
| encoded AU -> FW ring | bounded AU view into sink; ring SDK determines final copy | adapter exists for released ring API; no end-to-end zero-copy claim |

Every optimized boundary must record allocator, memory type/modifier, ownership, cache/fence
operation, completion point, fallback copy and measured DDR/latency. A retained FD or
`GstBuffer` does not by itself prove DMA-BUF import or zero-copy.

## Current FW compatibility limitation

The released FW ring adapter exposes only fixed `detect0` and `detect1` paths mapped to
camera 0/channel 0 behavior. Therefore the current code can configure up to 16 inference
sources, but it cannot promise 16 independently addressable preview outputs. Until FW signs
a versioned output registry/ring contract, `preview_output_ref` must resolve to a released
identity; unsupported or duplicate outputs fail closed. Headless inference sources set
`preview_surface_count` to zero and use an empty preview reference.

The production platform enforces this: a configured `output_ring_id` requires exactly one
deployment source, and a multi-source deployment fails `prepare` with `unsupported` before
any acquisition. Multi-source inference runs headless (`output_ring_id` empty). A future
versioned per-source output registry is required to map more than one source to preview.

See also [Camera Service contract](../contracts/camera_service.md),
[model integration](../contracts/model_integration.md), and
[FW ring sink](fw_ring_sink.md).

## Cascade retention budget

A source that hosts a cascade root (a model some secondary catalog model depends on) must
declare a `cascade` budget: `frames`, `tasks_per_frame` and `max_bytes`. Either all three are
zero (the source hosts no cascade root) or all three are nonzero and within limits; a partial
budget is rejected. The budget is counted in the deployment resident total. Composition sets
the session store sizing and `cascade_root_` from the catalog `role`/`depends_on`, and a
cascade-root model without a source budget fails composition. The runtime then retains the
exact source frame and releases the FW lease only after `store.bytes() == 0`.

## Limits and next work

- Authenticated FW registry RPC and board qualification remain pending.
- Artifact/output-manifest authentication and the runtime resolver that activates the
  resulting plans remain pending.
- Vendor graph pools, encoder pools, GStreamer objects, stacks and process overhead still
  require board-specific accounting before admission.
- The current code can configure up to 16 inference sources but cannot promise 16
  independently addressable preview outputs until FW signs a versioned output registry/ring
  contract.

## See also

- [model catalog](model_catalog.md)
- [multi-source supervisor](multi_source_supervisor.md)
- [model cadence](model_cadence.md)
- [Camera Service contract](../contracts/camera_service.md)
- [model integration](../contracts/model_integration.md)
- [FW ring sink](fw_ring_sink.md)
