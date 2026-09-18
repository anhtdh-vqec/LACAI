# Qualcomm FastRPC adapter and mapping leases

This document defines private mapping ownership and the required model-independent DSP
boundary. It distinguishes the implemented cache from the proposed replacement protocol.

**Status:** source-delivered — mapping leases and bounded cache retirement are implemented;
the model-independent DSP protocol below is a design requirement, not an implemented ABI.
**Layer:** adapters. **Source:** `src/adapters/qualcomm/vqec_vision_dsp_buffer_cache.{hpp,cpp}`.

## Responsibility

AI APP owns the Qualcomm-private adapter behind the neutral image-processor and decoder
ports. BSP owns the approved Hexagon build/signing, allocator, cache coherency, process-domain
recovery and completion guarantee. AI Model owns preprocess/decode semantics and golden data.
Changing the wire ABI or device-completion policy needs lead and BSP owner review.

## Mapping ownership contract

`dsp_buffer_cache::vqec_vision_ai_qcom_dspbc_map` returns a `dsp_buffer_mapping`, not a naked
pointer. Its owner pins the duplicated FD, CPU mapping and optional FastRPC registration for the
whole access. Preprocessing, alignment and preview copying retain this lease until their
last synchronous access. The caller separately retains the RAW frame owner, including the
original FD, throughout acquisition/access. A mapping lease does not own camera acquisition
or establish a CPU cache-acquire fence.

- Identity is the live allocation's device/inode and mapping size, never the numeric FD.
  This cache is scoped to one unchanged cDSP process domain. Reset/reopen requires drained
  access and a new cache; legacy transport has no reset-safe generation negotiation.
- Capacity includes retired and initializing entries. Only unleased entries may be evicted.
  If every entry is leased, mapping returns `resource_exhausted`, without growing the cache.
- Clear retires pinned entries and removes unpinned ones. A retired entry cannot be acquired
  again while it has readers. Subsequent acquisition can replace released retired entries.
- Entry destruction unregisters the FastRPC association before the CPU mapping and duplicated FD.
  Moving over a cache releases its former entries; extant leases remain valid.
- Bookkeeping locks are not held across SDK mapping/unmapping calls. Concurrent acquisition
  of an initializing allocation returns `pending`, rather than waiting for RPC.
- The generated QAIC stub passes CPU pointers to `remote_handle64_invoke`, so the cache uses
  `remote_register_buf_attr2` with non-coherent cache maintenance. `fastrpc_mmap` with
  `FASTRPC_MAP_FD` is not interchangeable: that contract requires DSP code to resolve the mapping
  with `HAP_mmap_get`. The registration API has no result value; a completed DSP smoke call is
  therefore the first observable registration/import check, not proof of coherency. CPU-only tests
  explicitly disable registration.
- A memfd is not a DMA-BUF and cannot be imported through the QCS6490 SMMU. Registered production
  mode rejects a sealable memfd before RPC. `--allow-qaic-copy-input` is an explicit device-free
  fixture mode that leaves the buffer unregistered and lets the generated QAIC transport copy it.
  It still executes the remote kernel, but its CPU result is not production performance evidence.

A synchronous SDK return is completion only under its documented backend contract.
Connection reset, timeout, close or source disconnect is not DMA cancellation. A future async
adapter must attach mapping and RAW owners to an explicit completion/quarantine domain, not
release a local lease when a submit call returns. Legacy kernels do not qualify recovery or
hardware completion by themselves. Lead/BSP review and device evidence remain open.

## Legacy execution constraints

Production requires a successfully opened accelerator session. A failed open or closed
session cannot run the copied host kernels. Tests explicitly construct `reference_cpu`
sessions; those kernels are legacy regression fixtures, not the model team's golden oracle.
CPU scratch is allocated only in this explicit reference mode, not on the production cold path.

The legacy dense head is exactly 640-square, 8400 predictions, one class and channel-first
packed uint16. SCRFD is exactly 640-square with three stride levels and five landmarks.
Lengths and exact tensor roles are checked before kernel entry. Unknown shapes/kinds cannot
be reinterpreted as these envelopes. Neutral quantization uses `(q - zero_point) * scale`;
the legacy wire uses `(q + offset) * scale`, so the adapter sends `offset = -zero_point`.
Reusing compact-result workspace avoids fresh result-vector allocation after warmup.

## Model-independent protocol requirements

Do not change legacy method ordinals or call a new method against an old binary. A separate
versioned protocol negotiates ABI revision, operations, limits, scalar encodings, domain
generation and completion mode before model activation.

The transport exposes operation families, not model IDs:

- Image transform: NV12 plane offsets/strides, colour matrix/range, crop/resize,
  interpolation, placement/padding, channels, normalization, dtype and quantization.
- Dense detection: tensor-role bindings, prediction/class counts, score representation,
  box encoding, class-aware NMS, candidate/output bounds and deterministic tie order.
- Anchor-distance detection: level grids/strides/anchor multiplicity, role bindings,
  landmark ontology, quantization and inverse transform.
- ROI alignment: bounded ROI batch, exact affine/template and normalization semantics.

Descriptors come from validated immutable packages at activation. Unknown operations and
unsupported semantics are rejected before source acquisition. CPU reference is an explicitly
selected test/degraded backend, never a consequence of failed DSP opening. SDK glue, memory
registration and dispatch remain Qualcomm-private; neutral runtime has no vendor/model switch.

Use project-owned IDL and kernels with reproducible builds. Generated SDK files need exact
generator/version/input and per-file provenance/license. Link external FastCV/QNN libraries,
do not copy and rename them. A legacy binary plus a generic class name is not a generic adapter.

## Evidence boundary

Lease tests exercise capacity, clear, concurrent access, move replacement and FD cleanup with
CPU mappings. They do not prove DSP coherency, DMA completion, reset safety, numeric parity
or CPU targets. See the [optimization plan](../planning/architecture_improvement/dsp_multiplatform_optimization_plan.md).

## See also

- [Qualcomm preprocessing](qualcomm_preprocessing.md)
- [QNN registered output memory](qualcomm_qnn_ion_memory.md)
- [RAW source port](raw_source_port.md)
