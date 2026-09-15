#ifndef VQEC_VISION_AI_CONTRACTS_EMBEDDING_HPP
#define VQEC_VISION_AI_CONTRACTS_EMBEDDING_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace embedding_limits {
inline constexpr std::size_t g_max_dimensions = 4096;
inline constexpr std::size_t g_max_identifier_bytes = 128;
inline constexpr float g_normalized_tolerance = 0.001F;
}  // namespace embedding_limits

// Sensitive model output. It remains internal until a separate entitlement-aware consumer
// accepts it; overlay/event paths must not serialize this payload implicitly.
struct embedding_result {
    preview_frame_key frame_;
    std::uint64_t track_id_{0};
    std::string model_id_;
    std::string model_version_;
    std::vector<float> values_;
    bool is_l2_normalized_{false};
};

[[nodiscard]] status vqec_vision_ai_core_embct_validate_result(
    const embedding_result& _result, const preview_frame_key& _expected_frame);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_EMBEDDING_HPP
