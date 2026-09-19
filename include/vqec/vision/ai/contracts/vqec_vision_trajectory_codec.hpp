#ifndef VQEC_VISION_AI_CONTRACTS_TRAJECTORY_CODEC_HPP
#define VQEC_VISION_AI_CONTRACTS_TRAJECTORY_CODEC_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_spatiotemporal_metadata.hpp"

namespace vqec::vision::ai {

inline constexpr std::uint32_t g_trajectory_codec_schema_version =
    VQEC_VISION_AI_BASELINE_SCHEMA_VERSION;

struct encoded_trajectory_points {
    std::uint32_t schema_version_{g_trajectory_codec_schema_version};
    std::uint32_t checksum_crc32_{0};
    std::vector<std::uint8_t> bytes_;
};

[[nodiscard]] status vqec_vision_ai_cntr_trcod_encode_points(
    const trajectory_chunk& _chunk, std::size_t _maximum_encoded_bytes,
    encoded_trajectory_points& _encoded);

[[nodiscard]] status vqec_vision_ai_cntr_trcod_decode_points(
    const encoded_trajectory_points& _encoded, std::size_t _maximum_encoded_bytes,
    std::size_t _maximum_points, std::vector<trajectory_point>& _points);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_TRAJECTORY_CODEC_HPP
