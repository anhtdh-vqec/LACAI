#ifndef VQEC_VISION_AI_EMBED_RECOGNITION_SESSION_HPP
#define VQEC_VISION_AI_EMBED_RECOGNITION_SESSION_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_recognition.hpp"
#include "vqec/vision/ai/ports/vqec_vision_embedding_index.hpp"
#include "vqec/vision/ai/ports/vqec_vision_face_gallery_store.hpp"

namespace vqec::vision::ai {

struct recognition_session_config {
    embedding_index_config index_;
    recognition_policy_config policy_;
    std::size_t max_templates_per_subject_{0};
    std::size_t search_top_k_{0};
    float search_minimum_similarity_{0.0F};
};

struct recognition_session_snapshot {
    std::uint64_t gallery_revision_{0};
    std::size_t subject_count_{0};
    std::size_t template_count_{0};
    bool is_configured_{false};
    bool is_faulted_{false};
};

// Serialized, vendor-neutral owner for gallery mutations and recognition policy. The
// backing index may be Zvec or a reference implementation. Subject references remain
// opaque here; resolving a display name and authorizing its output belong downstream.
class recognition_session final {
public:
    recognition_session() = default;
    recognition_session(const recognition_session& _other) = delete;
    recognition_session& operator=(const recognition_session& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_embed_rcses_configure(
        embedding_index_port& _index, const recognition_session_config& _config);
    [[nodiscard]] status vqec_vision_ai_embed_rcses_configure_persistent(
        embedding_index_port& _index, face_gallery_store_port& _store,
        const recognition_session_config& _config,
        const face_gallery_config& _gallery_config);
    [[nodiscard]] status vqec_vision_ai_embed_rcses_add_template(
        const std::string& _subject_ref, const embedding_result& _embedding,
        std::uint64_t _expected_revision, std::uint64_t& _record_id,
        std::uint64_t& _new_revision);
    [[nodiscard]] status vqec_vision_ai_embed_rcses_remove_subject(
        const std::string& _subject_ref, std::uint64_t _expected_revision,
        std::uint64_t& _new_revision);
    [[nodiscard]] status vqec_vision_ai_embed_rcses_recognize(
        const embedding_result& _embedding, recognition_match_result& _result) const;
    [[nodiscard]] status vqec_vision_ai_embed_rcses_recognize_batch(
        const std::vector<embedding_result>& _embeddings,
        std::vector<recognition_match_result>& _results) const;
    [[nodiscard]] status vqec_vision_ai_embed_rcses_apply_labels(
        const std::vector<recognition_match_result>& _results,
        observation_batch& _observations) const;
    [[nodiscard]] recognition_session_snapshot
    vqec_vision_ai_embed_rcses_get_snapshot() const noexcept;

private:
    struct template_metadata {
        std::uint64_t record_id_{0};
        std::string subject_ref_;
    };

    embedding_index_port* index_{nullptr};
    face_gallery_store_port* store_{nullptr};
    recognition_session_config config_;
    face_gallery_config gallery_config_;
    face_gallery_snapshot gallery_;
    std::vector<template_metadata> templates_;
    std::uint64_t next_record_id_{1};
    bool is_configured_{false};
    bool is_faulted_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_EMBED_RECOGNITION_SESSION_HPP
