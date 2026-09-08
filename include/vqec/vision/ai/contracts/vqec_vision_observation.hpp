#ifndef VQEC_VISION_AI_CONTRACTS_OBSERVATION_HPP
#define VQEC_VISION_AI_CONTRACTS_OBSERVATION_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace observation_limits {
inline constexpr std::size_t g_max_observations = 256;
inline constexpr std::size_t g_max_attributes = 32;
inline constexpr std::size_t g_max_identifier_bytes = 128;
inline constexpr float g_min_confidence = 0.0F;
inline constexpr float g_max_confidence = 1.0F;
}  // namespace observation_limits

enum class observation_quality { unknown, low, medium, high };

struct observation_attribute {
    std::string schema_id_;
    std::string schema_version_;
    std::string value_;
    float confidence_{0.0F};
    observation_quality quality_{observation_quality::unknown};
    std::uint64_t observed_at_ns_{0};
    std::uint64_t expires_at_ns_{0};
};

struct observation {
    preview_frame_key frame_;
    std::uint64_t track_id_{0};
    std::string class_id_;
    overlay_box box_;
    float confidence_{0.0F};
    observation_quality quality_{observation_quality::unknown};
    std::vector<observation_attribute> attributes_;
};

struct observation_batch {
    preview_frame_key frame_;
    preview_geometry geometry_;
    std::vector<observation> observations_;
};

// Validates model-independent observations; no model trust or entitlement decision.
[[nodiscard]] status vqec_vision_ai_core_obval_validate_batch(
    const observation_batch& _batch, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_OBSERVATION_HPP
