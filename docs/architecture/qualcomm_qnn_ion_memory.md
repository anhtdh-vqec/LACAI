# Qualcomm QNN ION-registered output memory

This document specifies the ION-registered output and preallocated-workspace memory architecture
for the LACAI Qualcomm QNN engine adapter. It exists to bound the remaining per-frame copies and
prevent overclaiming zero-copy.

**Status:** source-delivered — the rpcmem/ION registration, preallocated workspace and teardown
order are implemented in the QNN engine; end-to-end zero-copy does not exist. **Layer:** adapters.
**Source:** `src/adapters/qualcomm/vqec_vision_qnn_engine.{hpp,cpp}`.

## Responsibility

- Avoid per-frame output heap churn with a preallocated `output_workspace_`.
- Avoid driver-side staging/pinning of raw client pointers via ION registration.
- Keep clear dependency-ordered teardown of all registered resources.
- Do not claim end-to-end zero-copy; the neutral output contract owns its bytes.

## Accuracy note (2026-09-16)

This adapter avoids the two original costs: per-frame output heap churn (preallocated
`output_workspace_`) and driver-side staging/pinning of raw client pointers (ION registration). It
is **not end-to-end zero-copy**. The HTP writes into registered physical pages, but `execute()` then
copies each output into the neutral `tensor_blob` the caller receives, because the neutral output
contract owns its bytes:

```cpp
std::memcpy(_outputs[index].bytes_.data(), source, bytes);  // qnn_engine.cpp execute()
```

So the remaining per-frame copies are: HTP DMA into ION (no CPU), then one bounded `memcpy` per
output tensor into the neutral blob. When no `memhandler` output is used, the source is the
preallocated `output_workspace_` instead. A future zero-copy path requires a neutral owner/view
contract for borrowed device output; it does not exist today. Treat "zero-copy" claims as limited
to the input/pinning side and the output allocation side.

## Motivation and bottleneck analysis

In naive QNN client-buffer execution (`QNN_TENSORMEMTYPE_RAW`), each inference pass:

1. Reallocates `std::vector<tensor_blob>` and resizes byte vectors for each output tensor on the
   heap (`malloc` + zero-initialization `memset`).
2. Passes raw user space pointers to QNN driver, requiring internal staging/pinning by the driver
   on each frame.
3. Yields multi-megabyte heap churn at 30 FPS across concurrent models (YOLOv8, SCRFD, EdgeFace),
   wasting 30-50% CPU in memory allocation, cache eviction, and lock contention.

## Architecture

### 1. Dynamic FastRPC rpcmem driver

The adapter dynamically probes `libcdsprpc.so` during `open()`:

- `rpcmem_alloc`: allocates non-contiguous physical pages from `RPCMEM_HEAP_ID_SYSTEM` (25) with
  cacheable flags (`RPCMEM_DEFAULT_FLAGS`), page-aligned to 4096 bytes.
- `rpcmem_to_fd`: retrieves the underlying DMA-BUF file descriptor.
- `rpcmem_free`: releases allocated physical memory.

If `libcdsprpc.so` or the required symbols are absent (e.g. host cross-compilation or QEMU
emulation), the engine gracefully falls back to heap-backed preallocated workspaces without failing
engine startup.

### 2. ION `MEMHANDLE` registration (not end-to-end zero-copy)

During `prepare()` after graph finalization:

1. For each output tensor, an aligned ION buffer is allocated via `rpcmem_alloc`. The
   byte count is checked before conversion to the driver's signed `int` ABI.
2. A `Qnn_MemDescriptor_t` is populated with `QNN_MEM_TYPE_ION`, `ionInfo.fd`, and tensor dimension
   metadata (`numDim`, `dimSize`).
3. `QnnMem_register` is invoked with the model context to obtain a `Qnn_MemHandle_t`.
4. Output tensor descriptors are configured once:

   ```cpp
   tensor.v2.memType = QNN_TENSORMEMTYPE_MEMHANDLE;
   tensor.v2.memHandle = handle;
   ```

5. During execution (`execute()`), the HTP writes inference results directly into the registered
   physical pages via DMA without driver-side CPU staging. `execute()` then copies those bytes into
   the neutral output blob (one bounded `memcpy` per tensor), so this is DMA-write +
   neutral-owned copy, not caller-visible zero-copy.

Registered pages are not zero-filled by AI APP before the first execution: a successful graph
execution must define every declared output byte. The heap workspace is allocated only if the
complete registered set cannot be established, so startup does not allocate and touch two full
output sets.

QNN client buffers expose packed dimensions but no tensor strides. Rank-one through rank-three
results are therefore reported as neutral `flat` layout; a batch-one rank-four image with a
three-channel trailing dimension is `nhwc`. Other rank-four layouts remain `unknown` and must be
qualified by a stronger graph/manifest contract rather than guessed by the decoder.

### 3. Pre-allocated output workspace

To eliminate per-frame dynamic heap allocations when `execute()` produces results:

- `output_workspace_` is allocated once during `prepare()` with exact tensor dimensions and
  quantization metadata only when registered output is unavailable.
- When results are copied or transferred to caller-provided `_outputs`, capacity is preserved
  across frames, avoiding `vector::resize` and `malloc`/`free` calls on the hot execution path.

### 4. Teardown lifecycle order

Resource destruction strictly follows dependency invariants:

1. Deregister all `Qnn_MemHandle_t` handles via `QnnMem_deRegister`.
2. Free physical ION memory via `rpcmem_free`.
3. Free graph metadata via `QnnModel_freeGraphsInfo`.
4. Close model shared object via `dlclose`.
5. Free QNN context via `QnnContext_free`.
6. Destroy HTP power voting infrastructure.
7. Free QNN device and backend.
8. Close `libcdsprpc.so`.

## Limits and next work

- End-to-end zero-copy does not exist; one bounded `memcpy` per output tensor into the neutral blob
  remains. A neutral owner/view contract for borrowed device output is required before removing it.
- The HTP DMA into ION path depends on `libcdsprpc.so`; when absent the engine falls back to
  heap-backed workspaces.
- Registered shared input memory and board completion evidence remain open.

## See also

- [Qualcomm adapter — implementation blueprint](qualcomm_adapter.md)
- [Neutral execution policy and the Qualcomm engine](qualcomm_execution_policy.md)
- [Qualcomm preprocessing adapter](qualcomm_preprocessing.md)
