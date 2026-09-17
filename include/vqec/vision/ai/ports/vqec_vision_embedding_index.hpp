#ifndef VQEC_VISION_AI_PORTS_EMBEDDING_INDEX_HPP
#define VQEC_VISION_AI_PORTS_EMBEDDING_INDEX_HPP

#include <cstddef>
#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_embedding.hpp"

namespace vqec::vision::ai {

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
