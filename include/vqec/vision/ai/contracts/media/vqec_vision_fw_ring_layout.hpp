#ifndef VQEC_VISION_AI_CONTRACTS_FW_RING_LAYOUT_HPP
#define VQEC_VISION_AI_CONTRACTS_FW_RING_LAYOUT_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace vqec::vision::ai::fw_ring_layout {

// Single AI-side definition of the released FW shared-memory encoded ring
// (`camera_ai::SharedMemoryFrameRingBuffer`). The FW RTSP/recording service is the only
// reader and AI is the only writer. Adapters MUST include this header instead of
// re-declaring offsets, slot sizes or the version. Bump g_version only with the FW contract
// and docs/architecture/fw_ring_sink.md; the static assertions below fail closed on drift.
//
// Baseline FW release revision observed 2026-09-16: version 5, 16 slots, 1 MiB payload.
inline constexpr std::uint32_t g_version = 5;
inline constexpr std::uint32_t g_magic = 0x4C414341U;
inline constexpr std::uint32_t g_slot_count = 16;
inline constexpr std::uint32_t g_payload_size = 1U << 20;
inline constexpr std::uint32_t g_header_size = 4096;
inline constexpr std::uint32_t g_slot_header_size = 1232;
inline constexpr std::uint32_t g_parameter_set_max_size = 512;
// Linux shared-memory directory used by the FW ring. Named platform constant, not policy.
inline constexpr char g_shm_directory[] = "/dev/shm";
inline constexpr char g_ring_name_prefix[] = "camera_ai_";

// SharedRingHeader scalar field offsets.
inline constexpr std::size_t g_h_magic = 0;
inline constexpr std::size_t g_h_version = 4;
inline constexpr std::size_t g_h_header_size = 8;
inline constexpr std::size_t g_h_slot_header_size = 12;
inline constexpr std::size_t g_h_slot_count = 16;
inline constexpr std::size_t g_h_payload_size = 20;
inline constexpr std::size_t g_h_write_sequence = 32;
inline constexpr std::size_t g_h_ring_id = 64;
inline constexpr std::size_t g_h_ring_id_max_bytes = 64;

// SharedSlotHeader field offsets.
inline constexpr std::size_t g_s_seqlock = 0;
inline constexpr std::size_t g_s_data_size = 8;
inline constexpr std::size_t g_s_width = 20;
inline constexpr std::size_t g_s_height = 24;
inline constexpr std::size_t g_s_stride = 28;
inline constexpr std::size_t g_s_is_keyframe = 40;
inline constexpr std::size_t g_s_h264_sps_size = 44;
inline constexpr std::size_t g_s_h264_pps_size = 48;
inline constexpr std::size_t g_s_frame_id = 56;
inline constexpr std::size_t g_s_timestamp_ns = 64;
inline constexpr std::size_t g_s_media_pts_ns = 72;
inline constexpr std::size_t g_s_sequence = 104;
inline constexpr std::size_t g_s_codec = 176;
inline constexpr std::size_t g_s_h264_sps = 208;
inline constexpr std::size_t g_s_h264_pps = 720;

// Compile-time guards against an accidental layout edit away from the FW ABI.
static_assert(g_payload_size == 1048576U, "FW ring payload must be 1 MiB");
static_assert(g_s_codec + 4U <= g_slot_header_size, "codec field must fit the slot header");
static_assert(g_s_h264_pps + g_parameter_set_max_size <= g_slot_header_size,
    "parameter sets must fit the slot header");

// Maps a configured ring ID to the FW shared-memory path. Characters outside the FW-accepted
// set become '_' exactly as the FW reader does; the AI writer must not choose its own rule.
[[nodiscard]] inline std::string vqec_vision_ai_cntr_fwrly_make_shm_path(
    const std::string& _ring_id) {
    std::string sanitized;
    sanitized.reserve(_ring_id.size());
    for (const char character : _ring_id) {
        const bool is_allowed = (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '.' ||
            character == '_' || character == '-';
        sanitized.push_back(is_allowed ? character : '_');
    }
    return std::string(g_shm_directory) + "/" + g_ring_name_prefix + sanitized;
}

}  // namespace vqec::vision::ai::fw_ring_layout

#endif  // VQEC_VISION_AI_CONTRACTS_FW_RING_LAYOUT_HPP
