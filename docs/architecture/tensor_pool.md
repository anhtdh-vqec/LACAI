# Bounded tensor pool

Status: device-free source delivered and tested; not yet wired into the production
inference path.

`tensor_pool` preallocates a fixed set of tensor slots so the steady-state path reuses
bytes instead of resizing a vector per frame. Source:
`src/core/vqec_vision_tensor_pool.{hpp,cpp}`.

## Contract

- `configure(capacity, spec, poison_on_release)` allocates `capacity` slots once; the spec
  is immutable afterward; `capacity` is bounded by `tensor_pool_limits::g_max_slots`.
- `acquire(slot, blob)` returns a borrowed pointer to preallocated storage, or
  `resource_exhausted` when full (counter bumps; never grows).
- `release(slot)` detects double-release (`double_release_total_`) and, when poisoning is
  enabled, overwrites the bytes to expose use-after-release in debug.
- `get(slot, blob)` reads a still-acquired slot for a downstream decoder.
- `get_stats()` exposes capacity/free/live/high-watermark and acquire/release/exhausted/
  double-release counters.

The caller serializes use of an acquired slot. Byte size comes from the neutral tensor
contract (`vqec_vision_ai_core_tnctr_shape_bytes`), so unsupported or empty shapes are
rejected at configure.

## Tests

`tests/unit/vqec_vision_tensor_pool_test.cpp` covers configuration validation,
preallocation, exhaustion, double-release detection, read access, poison-on-release, a
1000-cycle steady-state reuse loop and concurrent acquire/release with four threads. Runs in
the neutral and expanded eSDK QEMU configurations.

## Remaining

- Wiring: input preprocess and the QNN/plugin output path must acquire/release pool slots
  instead of allocating per call. Until then the module is tested but not on the running
  path, and the "no steady-state allocation" claim is not yet provable.
- Alignment is not configurable yet; vector-backed storage does not guarantee a device
  alignment. Add it only with an accelerator requirement.
