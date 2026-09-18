# ADR 0007 — Versioned FastRPC operation protocol

Status: proposed — AI APP lead and BSP+FW ABI/ownership approval required before production v2.
Date: 2026-09-18.
Owner: AI APP lead; BSP+FW owns cDSP toolchain, memory and deployment sign-off.

## Context

The deployed `vqec_dsp` skeleton and host stub expose method ordinals named for person,
SCRFD and fire/smoke models. The source cannot express arbitrary model shapes, classes,
colour conversion or decoder layout. Changing the existing IDL in place would allow a
new host to invoke an old binary with incompatible marshalling. Qualcomm Hexagon SDK
5.5.7.0 is available locally; QAIC 01.00.47 regenerates the legacy stub byte-for-byte
from the historical FW IDL. That proves stub provenance, not the license of authored
algorithms or the identity of the deployed skeleton.

## Decision

- Keep `vqec_dsp` as a bounded compatibility ABI during migration. Its exact ordinals,
  tensor envelopes and deployed skeleton name remain unchanged. It is never described
  as generic. Do not add model-specific methods to it.
- Define a separate `vqec_vision_dsp_v2` interface and skeleton. `query_capabilities`
  precedes every activation and reports ABI revision, operation versions, limits,
  supported enum values, synchronous/asynchronous completion, domain generation and
  memory-registration mode. An absent or incompatible v2 skeleton fails closed;
  compatibility selection is an explicit deployment policy, not an automatic fallback.
- The v2 operations are `image_transform`, `dense_decode`, `anchor_distance_decode`
  and `roi_align`. Requests use fixed, versioned, little-endian descriptor fields with
  checked lengths and offsets. No model name or usecase ID appears in the device ABI.
  Unsupported colour/range/interpolation/quantization/shape is rejected before source
  acquisition; an approximate kernel is not selected.
- The project-owned `vqec_vision_dsp_v2.idl` is only a QAIC-parseable transport draft.
  Its `execute` method currently carries one packed input byte sequence, which may add
  an ARM copy for multiple QNN outputs. The exact descriptor layout, capability response,
  maximum sequence lengths and scatter/gather or registered-buffer transport must be
  reviewed and measured before an implementation may call it. Generating a stub and
  skeleton does not establish a usable v2 kernel or permit product activation.
- QAIC artifacts are generated at build time from project-owned IDL with a pinned
  SDK path/version/command and never hand-edited. Only authored project code and the
  reviewed IDL are committed. DSP builds use the Hexagon toolchain; ARM C++ builds and
  all LACAI CMake/tests use the approved eSDK. Neither toolchain substitutes for the
  other.
- Source-frame, QNN tensor and DSP mapping leases remain live through proven synchronous
  RPC completion. Timeout, close, disconnect and process death are not DMA completion.
  BSP must review cache/fence and reset behavior before enabling registered input in
  a released product.
- Legacy reference algorithms are isolated as compatibility test code while model
  kits migrate. Their prior location under `third_party/fastrpc_dsp` is retired; moving
  them does not grant redistribution rights. Per-file owner/license approval is required
  before release, or the algorithms must be replaced with original implementations.

## Alternatives

- Extend `vqec_dsp` ordinals in place: rejected because old skeletons cannot negotiate
  descriptor support and may misinterpret a new host.
- Rename the legacy functions to generic names: rejected because it preserves fixed
  640/8400/one-class behavior under a misleading API.
- Delete legacy support immediately: rejected because the three-model board workload
  currently uses the deployed legacy skeleton and would stop running before v2 parity.
- Use CPU reference automatically when v2 is unavailable: rejected by accelerator-required
  policy and CPU budget.

## Consequences

- Two ABIs coexist temporarily. CI must prove legacy behavior is unchanged, and v2 must
  have negative capability/descriptor tests and build provenance before activation.
- Hexagon SDK presence removes the generator/toolchain discovery blocker but does not
  provide model-team golden tensors, FastCV semantics, released-FW DMA completion, or
  eight-hour leak and 30/60-minute CPU evidence. Those gates remain open.
- ABI, ownership and entitlement changes need lead plus relevant owner review; this
  proposed ADR does not waive that review.

## Approve after (gates)

1. AI APP lead and BSP+FW approve exact v2 wire, operation limits and deployment selection.
2. AI Model approves per-model preprocess/decode semantics and M0–M4 goldens.
3. BSP+FW signs the SDK/provenance, allocator, cache/fence, completion and recovery receipt.
