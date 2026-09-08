#ifndef VQEC_VISION_AI_CAMERA_LEGACY_WIRE_HPP
#define VQEC_VISION_AI_CAMERA_LEGACY_WIRE_HPP

#include <cstddef>
#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_deployment_config.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_frame_descriptor.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

// Fixed native-endian FW FrameHeader layout, not sizeof a compiler-dependent struct.
// Source snapshot: fw_camera_integration_requirements.md, baseline 139d335.
namespace legacy_wire_layout {
inline constexpr std::size_t g_header_bytes = 104;
inline constexpr std::size_t g_width_offset = 8;
inline constexpr std::size_t g_height_offset = 12;
inline constexpr std::size_t g_format_offset = 16;
inline constexpr std::size_t g_plane_count_offset = 20;
inline constexpr std::size_t g_plane_offsets_offset = 24;
inline constexpr std::size_t g_plane_strides_offset = 40;
inline constexpr std::size_t g_plane_field_bytes = 4;
inline constexpr std::size_t g_view_size_offset = 56;
inline constexpr std::size_t g_memory_offset_offset = 64;
inline constexpr std::size_t g_allocation_size_offset = 72;
inline constexpr std::size_t g_pts_offset = 80;
inline constexpr std::size_t g_dts_offset = 88;
inline constexpr std::size_t g_duration_offset = 96;
inline constexpr std::size_t g_nv12_planes = 2;
static_assert(g_duration_offset + sizeof(std::uint64_t) == g_header_bytes);
}  // namespace legacy_wire_layout

struct legacy_frame_limits {
    // Required: numeric GST_VIDEO_FORMAT_NV12 from the pinned FW/GStreamer ABI.
    std::uint32_t nv12_format_value_{0};
    // Required from validated deployment configuration; zero means unset.
    std::uint32_t max_width_{0};
    std::uint32_t max_height_{0};
    std::uint64_t max_allocation_bytes_{0};
};

// Cold-path adapter composition. _source must already belong to a validated deployment.
// Preserves _limits on failure; product transport origin is intentionally opaque here.
[[nodiscard]] status vqec_vision_ai_camer_lwire_make_limits(
    const source_deployment_config& _source, std::uint32_t _nv12_format_value,
    legacy_frame_limits& _limits);

// Same-host native endian protocol. Transactional output; no allocation or pixel access.
[[nodiscard]] status vqec_vision_ai_camer_lwire_decode_frame(
    const std::uint8_t* _packet, std::size_t _size, const legacy_frame_limits& _limits,
    frame_descriptor& _frame);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CAMERA_LEGACY_WIRE_HPP
