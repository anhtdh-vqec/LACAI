#ifndef VQEC_VISION_AI_CONTRACTS_PREVIEW_LIMITS_HPP
#define VQEC_VISION_AI_CONTRACTS_PREVIEW_LIMITS_HPP

#include <cstddef>
#include <cstdint>

namespace vqec::vision::ai::preview_limits {

// AI metadata/allocation safety ceilings, NOT measured platform capabilities.
inline constexpr std::uint32_t g_max_dimension_pixels = 8192;
inline constexpr std::uint64_t g_max_surface_bytes = 64ULL * 1024 * 1024;
inline constexpr std::uint64_t g_max_pool_bytes = 256ULL * 1024 * 1024;
inline constexpr std::size_t g_max_surface_slots = 4;
inline constexpr std::size_t g_max_overlay_boxes = 128;
inline constexpr std::size_t g_max_label_bytes = 96;
inline constexpr std::size_t g_max_total_label_bytes = 4096;
inline constexpr unsigned char g_min_label_character = 32;
inline constexpr unsigned char g_max_label_character = 126;

// Released AI output profile. Changes require FW reader/encoder compatibility review.
// Source: FW shared/common/ring_buffer.hpp and AI SharedRingVideoEncoder.cpp,
// commit 139d335913e19e5a33a36fa8f8d706009892db44. No SDK layout types exposed here.
inline constexpr std::size_t g_max_encoded_payload_bytes = 2U * 1024 * 1024;
inline constexpr std::size_t g_max_parameter_set_bytes = 512;
inline constexpr std::uint32_t g_encoded_ring_slots = 16;

}  // namespace vqec::vision::ai::preview_limits

#endif  // VQEC_VISION_AI_CONTRACTS_PREVIEW_LIMITS_HPP
