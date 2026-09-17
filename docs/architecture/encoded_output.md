# Owned encoded output and synchronous sink port

`owned_h264_output` publishes immutable ownership of a validated encoded access unit, and
`encoded_sink` is the internal synchronous-copy output port. This document defines that
ownership transfer, the sink demand/write contract and its generation rules.

**Status:** source-delivered — source-only; no FW ring SDK writer or GStreamer callback is
connected. **Layer:** contracts. **Source:**
`src/core/vqec_vision_encoded_output.cpp`,
`include/vqec/vision/ai/contracts/vqec_vision_encoded_sink.hpp`,
`tests/unit/vqec_vision_encoded_output_test.cpp`.

## Responsibility

- Validates a borrowed `h264_access_unit_view` against an independent expected
  frame/geometry.
- Copies payload/SPS/PPS transactionally and publishes immutable ownership.
- Exposes a synchronous-copy sink demand/write contract in a runtime-unique binding.
- Must not be used as a bounded output queue; the caller admits depth and bytes before
  copying.
- Must not retain borrowed pointers or expose mutable storage access.
- Must not be treated as an authorization receipt.

## Owned output

The existing borrowed `h264_access_unit_view` cannot safely be queued after returning
an encoder sample. `owned_h264_output` validates that view against an independent expected
frame/geometry, copies payload/SPS/PPS transactionally and publishes immutable ownership.
Limits remain <=2 MiB payload, <=512 bytes each parameter set. Caller admits queue depth
and aggregate bytes BEFORE copying; this utility alone is not a bounded output queue.
Validation checks envelope/start code only, not H264 parsing or source authenticity.

Failure leaves the prior output owner unchanged. Borrowing a view from an owned output
does not extend its lifetime; retain the owner through every consumer read. No mutable
storage access is exposed. Default construction/mutation/copying of owned outputs is
not public; construction is through the validated copy function only.

## Sink port

`encoded_sink` is an internal C++ synchronous-copy port, NOT a shared plugin ABI. Its
implementation owns ring configuration/open/close outside this minimal port. `demand`
returns a status plus transactional snapshot: nonzero mapping generation and active
consumer count. A real sink must report unavailable rather than fabricate a ready
generation. This is a runtime-unique output binding identity, not source epoch or policy
revision. An SDK-object-local mapping counter may restart at 1; never expose that counter
alone as the neutral generation across newly constructed sink instances.

`write` receives a borrowed AU and independently supplied expected mapping generation.
The sink must reject stale generation BEFORE touching the new mapping, validate its
configured source/profile and payload bounds, and synchronously copy bytes into its
ring before returning ok. It must not retain borrowed pointers. Failure must never
be reported as successful delivery; no automatic retry semantics are defined here.
Do not use ok to mean queued, network-delivered or rendered on UI. Demand is a hint:
viewers can leave after the snapshot; final scheduling and error policy belong to runtime.

The owner/payload is not an authorization receipt. Output gate/freshness/revision checks
must occur at final dispatch, including any retry. FW backend must keep exact released
ring IDs/layout/parameter sets and cold-start discovery; see
[fw_release_compatibility.md](../contracts/fw_release_compatibility.md).
Do not expose pthread/shared atomic layout in this neutral port.

## Limits and next work

- No FW ring SDK writer or GStreamer callback is connected.
- This utility alone is not a bounded output queue; the caller must admit depth and bytes.
- Validation checks envelope/start code only, not H264 parsing or source authenticity.
- Output gate/freshness/revision checks must occur at final dispatch, including any retry.

## See also

- [FW release compatibility](../contracts/fw_release_compatibility.md)
- [Synchronous authorized encoded dispatch](encoded_dispatch.md)
- [Output binding generation issuance](output_generation.md)
- [FW ring sink](fw_ring_sink.md)
