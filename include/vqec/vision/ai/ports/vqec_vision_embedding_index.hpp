#ifndef VQEC_VISION_AI_PORTS_EMBEDDING_INDEX_HPP
#define VQEC_VISION_AI_PORTS_EMBEDDING_INDEX_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_embedding.hpp"

namespace vqec::vision::ai {

namespace embedding_index_limits {
inline constexpr std::size_t g_max_subject_ref_bytes = 128;
inline constexpr std::size_t g_max_results = 64;
}  // namespace embedding_index_limits

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
    float similarity_{0.0F};
};

struct embedding_search_result {
    preview_frame_key frame_;
    std::uint64_t track_id_{0};
    std::uint64_t gallery_revision_{0};
    std::vector<embedding_match> matches_;
};

class embedding_index_port {
public:
    virtual ~embedding_index_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_emidx_configure(
        const embedding_index_config& _config) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_emidx_upsert(
        const embedding_gallery_record& _record, std::uint64_t _expected_revision,
        std::uint64_t _new_revision) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_emidx_remove(
        std::uint64_t _record_id, std::uint64_t _expected_revision,
        std::uint64_t _new_revision) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_emidx_search(
        const embedding_result& _query, std::uint64_t _required_revision,
        std::size_t _top_k, float _minimum_similarity,
        embedding_search_result& _result) const = 0;
    [[nodiscard]] virtual std::uint64_t vqec_vision_ai_ports_emidx_revision()
        const noexcept = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_EMBEDDING_INDEX_HPP
