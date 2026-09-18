# ADR 0007 — Versioned FastRPC operation protocol

Status: proposed — AI APP lead and BSP+FW ABI/ownership approval required before production v1.
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
- Define a separate `vqec_vision_dsp_v1` interface and skeleton. `query_capabilities`
  precedes every activation and reports ABI revision, operation versions, limits,
  supported enum values, synchronous/asynchronous completion, domain generation and
  memory-registration mode. An absent or incompatible v1 skeleton fails closed;
  compatibility selection is an explicit deployment policy, not an automatic fallback.
- The v1 operations are `image_transform`, `dense_decode`, `anchor_distance_decode`
  and `roi_align`. Requests use fixed, versioned, little-endian descriptor fields with
  checked lengths and offsets. No model name or usecase ID appears in the device ABI.
  Unsupported colour/range/interpolation/quantization/shape is rejected before source
  acquisition; an approximate kernel is not selected.
- The project-owned `vqec_vision_dsp_v1.idl` is only a QAIC-parseable transport draft.
  Its `execute` method currently carries one packed input byte sequence, which may add
  an ARM copy for multiple QNN outputs. The exact descriptor layout, capability response,
  maximum sequence lengths and scatter/gather or registered-buffer transport must be
  reviewed and measured before an implementation may call it. Generating a stub and
  skeleton does not establish a usable v1 kernel or permit product activation.
- The draft envelope is 32 bytes in explicit little-endian encoding, never a C struct
  cast. A capability reply carries magic `VQ1!`, major/minor, header length, completion
  flags, operation mask, three byte limits and domain generation. A request carries the
  same magic/version/header length, operation family, exact descriptor/input lengths,
  caller output capacity, expected domain generation and zero flags. The host checks
  the reply before admitting a request; both sides reject unknown versions, flags,
  operations, lengths and stale generation. This only validates transport bounds:
  each operation must additionally validate its own descriptor before any kernel read.

Draft envelope offsets (bytes, all integers unsigned little-endian):

| Offset | Capability reply | Request descriptor |
|---:|---|---|
| 0 | `VQ1!` magic (4 bytes) | `VQ1!` magic (4 bytes) |
| 4, 6, 8 | major, minor, header bytes (`uint16` each) | same |
| 10 | synchronous-completion flag (`uint16`) | operation family (`uint16`) |
| 12 | supported-operation bitmask (`uint32`) | exact descriptor bytes (`uint32`) |
| 16 | max descriptor bytes (`uint32`) | exact input bytes (`uint32`) |
| 20 | max input bytes (`uint32`) | exact output capacity (`uint32`) |
| 24 | max output bytes (`uint32`) | expected domain generation (`uint32`) |
| 28 | domain generation (`uint32`) | reserved flags, zero (`uint32`) |

The fixed envelope codec is shared C that can compile for ARM and Hexagon. It performs
only transport validation; no operation payload, result schema or release capacity is
approved by this ADR draft.
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
  currently uses the deployed legacy skeleton and would stop running before v1 parity.
- Use CPU reference automatically when v1 is unavailable: rejected by accelerator-required
  policy and CPU budget.

## Consequences

- Two ABIs coexist temporarily. CI must prove legacy behavior is unchanged, and v1 must
  have negative capability/descriptor tests and build provenance before activation.
- Hexagon SDK presence removes the generator/toolchain discovery blocker but does not
  provide model-team golden tensors, FastCV semantics, released-FW DMA completion, or
  eight-hour leak and 30/60-minute CPU evidence. Those gates remain open.
- ABI, ownership and entitlement changes need lead plus relevant owner review; this
  proposed ADR does not waive that review.

## Approve after (gates)

1. AI APP lead and BSP+FW approve exact v1 wire, operation limits and deployment selection.
2. AI Model approves per-model preprocess/decode semantics and M0–M4 goldens.
3. BSP+FW signs the SDK/provenance, allocator, cache/fence, completion and recovery receipt.
