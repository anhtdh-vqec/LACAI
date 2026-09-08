# fw_output — optional SDK wrapper source

Configuration now requires distinct SDK mapping_generation and runtime-unique
dispatch_generation. Never use SDK counter 1 as identity across replacement instances.
vqec_vision_ai_fwout_rgsnk_map_header is a private transactional metadata translator,
covered by optional SDK test source without ring creation. No test execution yet.

vqec_vision_ring_sink.cpp now implements encoded_sink over an already-open borrowed
FW SharedMemoryFrameRingBuffer. Checks known layout/ring ID/source/generation and maps
H264 headers before synchronous direct payload push. No open/create/unlink or SDK copy.
Enable VQEC_VISION_AI_ENABLE_FW_RING with an externally supplied pinned SDK target.
See docs/architecture/fw_ring_sink.md for lifetime and remapping limitations.
Not built/tested; closed-ring guard test source does not prove live reader compatibility.

Neutral port is now include/vqec/vision/ai/contracts/vqec_vision_encoded_sink.hpp.
Implement query_demand and synchronous write using a pinned FW SDK; retain the declaring
interface function names. Reject stale mapping generation, do not store borrowed AU
pointers, and never report queued work as copied-to-ring success. The owned encoded
output helper is available for bounded runtime queues, not yet wired to this adapter.
See docs/architecture/encoded_output.md. No FW shared-memory layout was copied here.

Private adapter for the released FW shared-memory encoded AI ring SDK. Own ring
writer and consumer-demand access; do not own RTSP, Web UI or recording. Portable
outputs/contracts must not expose pthread, std::atomic shared layouts or FW types.
Use version-pinned SDK, not relative includes into a sibling repository. SDK packaging
and ABI qualification require FW agreement before production integration.
See docs/contracts/fw_release_compatibility.md (FW04–FW06).
