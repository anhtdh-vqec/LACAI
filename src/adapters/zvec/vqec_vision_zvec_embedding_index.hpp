#ifndef VQEC_VISION_AI_ZVEC_ZVEC_EMBEDDING_INDEX_HPP
#define VQEC_VISION_AI_ZVEC_ZVEC_EMBEDDING_INDEX_HPP

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "vqec/vision/ai/ports/vqec_vision_embedding_index.hpp"

struct zvec_collection_t;

namespace vqec::vision::ai {

// Zvec is a derived index. The caller remains responsible for the authoritative,
// encrypted gallery and must be able to rebuild this collection after loss.
class zvec_embedding_index final : public embedding_index_port {
public:
    explicit zvec_embedding_index(std::string _collection_path);
    ~zvec_embedding_index() override;

    zvec_embedding_index(const zvec_embedding_index&) = delete;
    zvec_embedding_index& operator=(const zvec_embedding_index&) = delete;

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
    std::string collection_path_;
    zvec_collection_t* collection_{nullptr};
    embedding_index_config config_;
    std::vector<std::uint64_t> record_ids_;
    std::uint64_t revision_{0};
    bool is_configured_{false};
    bool is_faulted_{false};
    mutable std::mutex mutex_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_ZVEC_ZVEC_EMBEDDING_INDEX_HPP
