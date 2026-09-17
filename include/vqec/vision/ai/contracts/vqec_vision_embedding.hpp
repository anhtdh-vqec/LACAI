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

// Neutral ceilings of the embedding-index boundary. Owned here (contracts) rather than in
// the index port so contracts such as recognition can reference them without a
// contracts -> ports dependency.
namespace embedding_index_limits {
inline constexpr std::size_t g_max_subject_ref_bytes = 128;
inline constexpr std::size_t g_max_results = 64;
inline constexpr float g_similarity_tolerance = 0.001F;
}  // namespace embedding_index_limits

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

// Embedding-index data types. These are neutral boundary data (not the port interface), so
// they live in contracts and can be referenced by other contracts such as recognition
// without a contracts -> ports dependency. The index port owns only the operations.
enum class embedding_metric { cosine_similarity };

struct embedding_index_config {
    std::string model_id_;
    std::string model_version_;
    std::size_t dimensions_{0};
    std::size_t capacity_{0};
    std::size_t max_results_{0};
    embedding_metric metric_{embedding_metric::cosine_similarity};
    std::uint64_t initial_revision_{0};
};

struct embedding_gallery_record {
    std::uint64_t record_id_{0};
    std::string subject_ref_;
    std::vector<float> values_;
};

struct embedding_match {
    std::uint64_t record_id_{0};
    // Opaque gallery identity. Display names and other personal data stay behind the
    // separately authorized identity metadata boundary.
    std::string subject_ref_;
    float similarity_{0.0F};
};

struct embedding_search_result {
    preview_frame_key frame_;
    std::uint64_t track_id_{0};
    std::uint64_t gallery_revision_{0};
    std::vector<embedding_match> matches_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_EMBEDDING_HPP
