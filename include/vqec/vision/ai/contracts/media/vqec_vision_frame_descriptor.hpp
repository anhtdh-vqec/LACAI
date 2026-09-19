#ifndef VQEC_VISION_AI_CONTRACTS_FRAME_DESCRIPTOR_HPP
#define VQEC_VISION_AI_CONTRACTS_FRAME_DESCRIPTOR_HPP

#include <array>
#include <cstdint>

namespace vqec::vision::ai {

// Linear NV12 only. Offsets are relative to the valid memory view, not the FD.
// Colorimetry, modifier guarantees and synchronization need deployment sign-off.
struct frame_descriptor {
    std::uint64_t buffer_id_{0};
    std::uint64_t session_epoch_{0};
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::array<std::uint32_t, 2> offsets_{};
    std::array<std::int32_t, 2> strides_{};
    std::uint64_t view_size_bytes_{0};
    std::uint64_t memory_offset_bytes_{0};
    std::uint64_t allocation_size_bytes_{0};
    // Native source clock, not UTC. UINT64_MAX means unavailable (FW sentinel).
    std::uint64_t pts_ns_{UINT64_MAX};
    std::uint64_t dts_ns_{UINT64_MAX};
    std::uint64_t duration_ns_{UINT64_MAX};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_FRAME_DESCRIPTOR_HPP
