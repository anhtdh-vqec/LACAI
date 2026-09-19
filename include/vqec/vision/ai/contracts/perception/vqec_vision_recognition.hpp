#ifndef VQEC_VISION_AI_CONTRACTS_RECOGNITION_HPP
#define VQEC_VISION_AI_CONTRACTS_RECOGNITION_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "vqec/vision/ai/contracts/perception/vqec_vision_embedding.hpp"
#include "vqec/vision/ai/contracts/media/vqec_vision_preview_contract.hpp"
#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace recognition_limits {
inline constexpr std::size_t g_max_subjects = embedding_index_limits::g_max_results;
}

enum class recognition_decision { known, unknown, ambiguous };

struct recognition_policy_config {
    float minimum_similarity_{0.0F};
    float minimum_subject_margin_{0.0F};
    std::size_t max_subjects_{0};
};

struct recognition_match_result {
    preview_frame_key frame_;
    std::uint64_t track_id_{0};
    std::uint64_t gallery_revision_{0};
    recognition_decision decision_{recognition_decision::unknown};
    std::string subject_ref_;
    float best_similarity_{0.0F};
    float subject_margin_{0.0F};
};

[[nodiscard]] status vqec_vision_ai_embed_rcpol_validate_config(
    const recognition_policy_config& _config);

// Evaluate a successful, revision-pinned index search. The index/backend error path must
// remain a separate unavailable state owned by the caller; it must never be mapped to
// unknown by this pure policy step.
[[nodiscard]] status vqec_vision_ai_embed_rcpol_evaluate(
    const embedding_search_result& _search, std::uint64_t _required_revision,
    const recognition_policy_config& _config, recognition_match_result& _result);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_RECOGNITION_HPP
