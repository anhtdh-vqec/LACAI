# Qualcomm QNN Zero-Copy ION Memory Architecture

## Purpose

This document specifies the zero-copy and zero-allocation memory architecture for the LACAI
Qualcomm QNN engine adapter (`src/adapters/qualcomm/vqec_vision_qnn_engine.cpp`). It adopts
the proven zero-copy hardware binding principles from `ai_app` while maintaining LACAI's
strict hexagonal architecture and clean boundary contracts.

## Motivation & Bottleneck Analysis

In naive QNN client-buffer execution (`QNN_TENSORMEMTYPE_RAW`), each inference pass:
1. Reallocates `std::vector<tensor_blob>` and resizes byte vectors for each output tensor on
   the heap (`malloc` + zero-initialization `memset`).
2. Passes raw user space pointers to QNN driver, requiring internal staging/pinning by the
   driver on each frame.
3. Yields multi-megabyte heap churn at 30 FPS across concurrent models (YOLOv8, SCRFD, EdgeFace),
   wasting 30-50% CPU in memory allocation, cache eviction, and lock contention.

## Architecture

### 1. Dynamic FastRPC rpcmem Driver

The adapter dynamically probes `libcdsprpc.so` during `open()`:
- `rpcmem_alloc`: allocates non-contiguous physical pages from `RPCMEM_HEAP_ID_SYSTEM` (25)
  with cacheable flags (`RPCMEM_DEFAULT_FLAGS`), page-aligned to 4096 bytes.
- `rpcmem_to_fd`: retrieves the underlying DMA-BUF file descriptor.
- `rpcmem_free`: releases allocated physical memory.

If `libcdsprpc.so` or the required symbols are absent (e.g. host cross-compilation or QEMU
emulation), the engine gracefully falls back to heap-backed preallocated workspaces without
failing engine startup.

### 2. Zero-Copy `MEMHANDLE` Registration

During `prepare()` after graph finalization:
1. For each output tensor, an aligned ION buffer is allocated via `rpcmem_alloc`.
2. A `Qnn_MemDescriptor_t` is populated with `QNN_MEM_TYPE_ION`, `ionInfo.fd`, and tensor
   dimension metadata (`numDim`, `dimSize`).
3. `QnnMem_register` is invoked with the model context to obtain a `Qnn_MemHandle_t`.
4. Output tensor descriptors are configured once:
   ```cpp
   tensor.v2.memType = QNN_TENSORMEMTYPE_MEMHANDLE;
   tensor.v2.memHandle = handle;
   ```
5. During execution (`execute()`), the HTP writes inference results directly into the
   registered physical pages via DMA without CPU data copy or CPU involvement.

### 3. Pre-allocated Output Workspace

To eliminate per-frame dynamic heap allocations when `execute()` produces results:
- `output_workspace_` is allocated once during `prepare()` with exact tensor dimensions and
  quantization metadata.
- When results are copied or transferred to caller-provided `_outputs`, capacity is preserved
  across frames, avoiding `vector::resize` and `malloc`/`free` calls on the hot execution path.

### 4. Teardown Lifecycle Order

Resource destruction strictly follows dependency invariants:
1. Deregister all `Qnn_MemHandle_t` handles via `QnnMem_deRegister`.
2. Free physical ION memory via `rpcmem_free`.
3. Free graph metadata via `QnnModel_freeGraphsInfo`.
4. Close model shared object via `dlclose`.
5. Free QNN context via `QnnContext_free`.
6. Destroy HTP power voting infrastructure.
7. Free QNN device and backend.
8. Close `libcdsprpc.so`.
