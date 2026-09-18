#include "vqec_vision_exact_embedding_index.hpp"
#include "vqec_vision_recognition_session.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vqec::vision::ai {
namespace {

constexpr std::size_t g_dimensions = 3;
constexpr std::size_t g_gallery_capacity = 8;
constexpr std::size_t g_max_results = 4;
constexpr std::size_t g_max_templates_per_subject = 3;
constexpr std::uint64_t g_preprocess_revision = 9;
constexpr char g_gallery_id[] = "front_gallery";
constexpr char g_model_id[] = "face_embedding";
constexpr char g_model_version[] = "1";
constexpr char g_subject_alice[] = "person_alice";
constexpr char g_subject_bob[] = "person_bob";

class memory_gallery_store final : public face_gallery_store_port {
public:
    explicit memory_gallery_store(face_gallery_snapshot _snapshot)
        : snapshot_(std::move(_snapshot)) {}

    status vqec_vision_ai_ports_fgstr_load(
        const face_gallery_config& _config,
        face_gallery_snapshot& _snapshot) override {
        const auto validated =
            vqec_vision_ai_core_fgalr_validate_snapshot(_config, snapshot_);
        if (validated.code_ != status_code::ok) {
            return validated;
        }
        _snapshot = snapshot_;
        return {};
    }

    status vqec_vision_ai_ports_fgstr_replace(
        const face_gallery_config& _config, std::uint64_t _expected_revision,
        const face_gallery_snapshot& _replacement) override {
        if (_expected_revision != snapshot_.revision_) {
            return {status_code::invalid_state, "memory gallery CAS conflict"};
        }
        const auto validated = vqec_vision_ai_core_fgalr_validate_replacement(
            _config, _expected_revision, _replacement);
        if (validated.code_ != status_code::ok) {
            return validated;
        }
        snapshot_ = _replacement;
        ++replacement_count_;
        return {};
    }

    face_gallery_snapshot snapshot_;
    std::size_t replacement_count_{0};
};

recognition_session_config vqec_vision_ai_unit_rcpst_make_config() {
    recognition_session_config config;
    config.index_ = {g_model_id, g_model_version, g_dimensions, g_gallery_capacity,
        g_max_results, embedding_metric::cosine_similarity, 1};
    config.policy_ = {0.80F, 0.05F, g_max_results};
    config.max_templates_per_subject_ = g_max_templates_per_subject;
    config.search_top_k_ = g_max_results;
    config.search_minimum_similarity_ = -1.0F;
    return config;
}

face_gallery_config vqec_vision_ai_unit_rcpst_make_gallery_config() {
    return {g_gallery_id, g_model_id, g_model_version, g_preprocess_revision,
        g_dimensions, g_gallery_capacity, g_max_templates_per_subject};
}

face_gallery_snapshot vqec_vision_ai_unit_rcpst_make_empty_gallery() {
    return {face_gallery_limits::g_schema_version, 1, 1, g_gallery_id, g_model_id,
        g_model_version, g_preprocess_revision, g_dimensions, {}};
}

embedding_result vqec_vision_ai_unit_rcpst_embedding(
    std::uint64_t _frame_id, std::uint64_t _track_id,
    std::vector<float> _values) {
    embedding_result result;
    result.frame_ = {1, 2, 3, _frame_id, _frame_id * 1000U};
    result.track_id_ = _track_id;
    result.model_id_ = g_model_id;
    result.model_version_ = g_model_version;
    result.values_ = std::move(_values);
    result.is_l2_normalized_ = true;
    return result;
}

void vqec_vision_ai_unit_rcpst_check_restart_rebuild() {
    memory_gallery_store store(vqec_vision_ai_unit_rcpst_make_empty_gallery());
    const auto config = vqec_vision_ai_unit_rcpst_make_config();
    const auto gallery_config = vqec_vision_ai_unit_rcpst_make_gallery_config();
    {
        exact_embedding_index index;
        recognition_session session;
        if (session.vqec_vision_ai_embed_rcses_configure_persistent(
                index, store, config, gallery_config).code_ != status_code::ok) {
            throw std::runtime_error("persistent recognition configure failed");
        }
        std::uint64_t record_id = 0;
        std::uint64_t revision = 0;
        const auto alice_front = vqec_vision_ai_unit_rcpst_embedding(
            1, 10, {1.0F, 0.0F, 0.0F});
        if (session.vqec_vision_ai_embed_rcses_add_template(g_subject_alice,
                alice_front, 1, record_id, revision).code_ != status_code::ok ||
            record_id != 1 || revision != 2) {
            throw std::runtime_error("first persistent template failed");
        }
        const auto alice_side = vqec_vision_ai_unit_rcpst_embedding(
            2, 10, {0.0F, 1.0F, 0.0F});
        if (session.vqec_vision_ai_embed_rcses_add_template(g_subject_alice,
                alice_side, revision, record_id, revision).code_ != status_code::ok) {
            throw std::runtime_error("second persistent template failed");
        }
        const auto bob = vqec_vision_ai_unit_rcpst_embedding(
            3, 20, {0.0F, 0.0F, 1.0F});
        if (session.vqec_vision_ai_embed_rcses_add_template(g_subject_bob,
                bob, revision, record_id, revision).code_ != status_code::ok) {
            throw std::runtime_error("persistent Bob template failed");
        }
        std::uint64_t removed_revision = 0;
        if (session.vqec_vision_ai_embed_rcses_remove_subject(g_subject_alice,
                revision, removed_revision).code_ != status_code::ok ||
            removed_revision != revision + 2U) {
            throw std::runtime_error("persistent subject removal failed");
        }
    }
    if (store.snapshot_.revision_ != 6 || store.snapshot_.templates_.size() != 1 ||
        store.snapshot_.next_record_id_ != 4 || store.replacement_count_ != 4) {
        throw std::runtime_error("authoritative gallery state is incorrect before restart");
    }

    exact_embedding_index recovered_index;
    recognition_session recovered;
    if (recovered.vqec_vision_ai_embed_rcses_configure_persistent(
            recovered_index, store, config, gallery_config).code_ != status_code::ok) {
        throw std::runtime_error("recognition restart rebuild failed");
    }
    const auto snapshot = recovered.vqec_vision_ai_embed_rcses_get_snapshot();
    if (snapshot.gallery_revision_ != 6 || snapshot.subject_count_ != 1 ||
        snapshot.template_count_ != 1) {
        throw std::runtime_error("recovered recognition metadata is incorrect");
    }
    recognition_match_result bob_match;
    const auto bob_query = vqec_vision_ai_unit_rcpst_embedding(
        4, 21, {0.0F, 0.0F, 1.0F});
    if (recovered.vqec_vision_ai_embed_rcses_recognize(
            bob_query, bob_match).code_ != status_code::ok ||
        bob_match.decision_ != recognition_decision::known ||
        bob_match.subject_ref_ != g_subject_bob) {
        throw std::runtime_error("recovered gallery did not recognize retained subject");
    }
    std::uint64_t record_id = 0;
    std::uint64_t revision = 0;
    if (recovered.vqec_vision_ai_embed_rcses_add_template(g_subject_bob,
            bob_query, 6, record_id, revision).code_ != status_code::ok ||
        record_id != 4 || revision != 7) {
        throw std::runtime_error("record identity was reused after restart");
    }
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    vqec::vision::ai::vqec_vision_ai_unit_rcpst_check_restart_rebuild();
    return 0;
}
