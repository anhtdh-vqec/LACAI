// libFuzzer harness for the FW legacy wire decoder. Host clang only; not part of the eSDK
// target build. A crash, UB, FD leak, unbounded allocation or infinite loop is a defect.
//
// Build: -DVQEC_VISION_AI_BUILD_FUZZERS=ON with a host Clang toolchain.

#include <cstddef>
#include <cstdint>

#include "vqec_vision_legacy_wire.hpp"

namespace {

// Test-harness parse policy, not a product deployment value.
constexpr std::uint32_t g_fuzz_nv12_format_value = 23;
constexpr std::uint32_t g_fuzz_max_width = 8192;
constexpr std::uint32_t g_fuzz_max_height = 8192;
constexpr std::uint64_t g_fuzz_max_allocation_bytes = 256ULL * 1024 * 1024;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* _data, std::size_t _size) {
    if (_data == nullptr) {
        return 0;
    }
    vqec::vision::ai::legacy_frame_limits limits;
    limits.nv12_format_value_ = g_fuzz_nv12_format_value;
    limits.max_width_ = g_fuzz_max_width;
    limits.max_height_ = g_fuzz_max_height;
    limits.max_allocation_bytes_ = g_fuzz_max_allocation_bytes;
    vqec::vision::ai::frame_descriptor frame;
    (void)vqec::vision::ai::vqec_vision_ai_camer_lwire_decode_frame(
        _data, _size, limits, frame);
    return 0;
}
