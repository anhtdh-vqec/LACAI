#include <cassert>
#include <iostream>
#include <vector>

#include "vqec_vision_exact_embedding_index.hpp"
#include "vqec_vision_recognition_session.hpp"

using namespace vqec::vision::ai;

namespace {

constexpr std::size_t g_dimensions = 3;
constexpr std::size_t g_gallery_capacity = 8;
constexpr std::size_t g_max_results = 4;
constexpr std::size_t g_max_templates_per_subject = 3;
constexpr float g_policy_similarity = 0.80F;
constexpr float g_policy_margin = 0.05F;
constexpr float g_search_similarity = -1.0F;
constexpr char g_model_id[] = "face_embedding";
constexpr char g_model_version[] = "1";
constexpr char g_subject_alice[] = "person_alice";
constexpr char g_subject_bob[] = "person_bob";

embedding_result vqec_vision_ai_unit_rcstst_embedding(
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

}  // namespace

int main() {
    exact_embedding_index index;
    recognition_session session;
    recognition_session_config config;
    config.index_ = {g_model_id, g_model_version, g_dimensions, g_gallery_capacity,
        g_max_results, embedding_metric::cosine_similarity, 1};
    config.policy_ = {g_policy_similarity, g_policy_margin, g_max_results};
    config.max_templates_per_subject_ = g_max_templates_per_subject;
    config.search_top_k_ = g_max_results;
    config.search_minimum_similarity_ = g_search_similarity;
    assert(session.vqec_vision_ai_embed_rcses_configure(index, config).code_ ==
        status_code::ok);

    std::uint64_t record_id = 0;
    std::uint64_t revision = 0;
    const auto alice_front = vqec_vision_ai_unit_rcstst_embedding(1, 10,
        {1.0F, 0.0F, 0.0F});
    const auto alice_added = session.vqec_vision_ai_embed_rcses_add_template(g_subject_alice,
        alice_front, 1, record_id, revision);
    if (alice_added.code_ != status_code::ok) {
        std::cerr << "alice add failed: " << static_cast<int>(alice_added.code_)
            << " " << alice_added.message_ << '\n';
    }
    assert(alice_added.code_ == status_code::ok);
    assert(record_id == 1 && revision == 2);
    const auto alice_side = vqec_vision_ai_unit_rcstst_embedding(2, 10,
        {0.98F, 0.20F, 0.0F});
    assert(session.vqec_vision_ai_embed_rcses_add_template(g_subject_alice,
        alice_side, revision, record_id, revision).code_ == status_code::ok);
    assert(record_id == 2 && revision == 3);
    const auto bob = vqec_vision_ai_unit_rcstst_embedding(3, 20,
        {0.0F, 1.0F, 0.0F});
    assert(session.vqec_vision_ai_embed_rcses_add_template(g_subject_bob,
        bob, revision, record_id, revision).code_ == status_code::ok);

    const auto query = vqec_vision_ai_unit_rcstst_embedding(4, 44,
        {1.0F, 0.0F, 0.0F});
    recognition_match_result match;
    assert(session.vqec_vision_ai_embed_rcses_recognize(query, match).code_ ==
        status_code::ok);
    assert(match.decision_ == recognition_decision::known &&
        match.subject_ref_ == g_subject_alice && match.track_id_ == query.track_id_);

    observation_batch observations;
    observations.frame_ = query.frame_;
    observations.geometry_ = {640, 480};
    observation face;
    face.frame_ = query.frame_;
    face.track_id_ = query.track_id_;
    face.class_id_ = "face";
    face.box_ = {10.0F, 20.0F, 100.0F, 120.0F, 0xffffffffU, "face"};
    face.confidence_ = 0.9F;
    observations.observations_.push_back(face);
    assert(session.vqec_vision_ai_embed_rcses_apply_labels({match}, observations).code_ ==
        status_code::ok);
    assert(observations.observations_[0].box_.label_ == g_subject_alice);

    const auto snapshot = session.vqec_vision_ai_embed_rcses_get_snapshot();
    assert(snapshot.subject_count_ == 2 && snapshot.template_count_ == 3 &&
        snapshot.gallery_revision_ == revision);
    std::uint64_t removed_revision = 0;
    assert(session.vqec_vision_ai_embed_rcses_remove_subject(g_subject_alice,
        revision, removed_revision).code_ == status_code::ok);
    const auto removed_snapshot = session.vqec_vision_ai_embed_rcses_get_snapshot();
    assert(removed_snapshot.subject_count_ == 1 &&
        removed_snapshot.template_count_ == 1 && removed_revision == revision + 2U);

    recognition_match_result after_remove;
    assert(session.vqec_vision_ai_embed_rcses_recognize(query, after_remove).code_ ==
        status_code::ok);
    assert(after_remove.decision_ == recognition_decision::unknown);

    std::cout << "recognition session passed\n";
    return 0;
}
