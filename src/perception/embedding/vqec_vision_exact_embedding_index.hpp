#ifndef VQEC_VISION_AI_EMBED_EXACT_EMBEDDING_INDEX_HPP
#define VQEC_VISION_AI_EMBED_EXACT_EMBEDDING_INDEX_HPP

#include "vqec/vision/ai/ports/vqec_vision_embedding_index.hpp"

namespace vqec::vision::ai {

class exact_embedding_index final : public embedding_index_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_emidx_configure(
        const embedding_index_config& _config) override;
    [[nodiscard]] status vqec_vision_ai_ports_emidx_upsert(
        const embedding_gallery_record& _record, std::uint64_t _expected_revision,
        std::uint64_t _new_revision) override;
    [[nodiscard]] status vqec_vision_ai_ports_emidx_remove(
        std::uint64_t _record_id, std::uint64_t _expected_revision,
        std::uint64_t _new_revision) override;
    [[nodiscard]] status vqec_vision_ai_ports_emidx_search(
        const embedding_result& _query, std::uint64_t _required_revision,
        std::size_t _top_k, float _minimum_similarity,
        embedding_search_result& _result) const override;
    [[nodiscard]] std::uint64_t vqec_vision_ai_ports_emidx_revision()
        const noexcept override;

private:
    embedding_index_config config_;
    std::vector<embedding_gallery_record> records_;
    std::uint64_t revision_{0};
    bool is_configured_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_EMBED_EXACT_EMBEDDING_INDEX_HPP
