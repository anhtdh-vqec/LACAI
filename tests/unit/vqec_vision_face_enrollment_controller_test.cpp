#include <cassert>
#include <iostream>

#include "vqec_vision_exact_embedding_index.hpp"
#include "vqec_vision_face_enrollment_controller.hpp"

using namespace vqec::vision::ai;

namespace {
constexpr char g_model_id[] = "face_embedding";
constexpr char g_model_version[] = "1";
constexpr char g_subject_ref[] = "subject_alice";
constexpr char g_source_id[] = "camera_front";

embedding_result vqec_vision_ai_unit_fenct_embedding(
    std::uint64_t _frame_id, std::uint64_t _track_id) {
    embedding_result embedding;
    embedding.frame_ = {7, 9, 4, _frame_id, _frame_id * 1000U};
    embedding.track_id_ = _track_id;
    embedding.model_id_ = g_model_id;
    embedding.model_version_ = g_model_version;
    embedding.values_ = {1.0F, 0.0F, 0.0F};
    embedding.is_l2_normalized_ = true;
    return embedding;
}
}

int main() {
    exact_embedding_index index;
    recognition_session session;
    recognition_session_config session_config;
    session_config.index_ = {g_model_id, g_model_version, 3, 8, 4,
        embedding_metric::cosine_similarity, 1};
    session_config.policy_ = {0.8F, 0.05F, 4};
    session_config.max_templates_per_subject_ = 3;
    session_config.search_top_k_ = 4;
    session_config.search_minimum_similarity_ = -1.0F;
    assert(session.vqec_vision_ai_embed_rcses_configure(index, session_config).code_ ==
        status_code::ok);
    face_enrollment_controller controller(session);
    face_enrollment_begin_request request;
    request.request_id_ = "enroll_1";
    request.subject_ref_ = g_subject_ref;
    request.source_id_ = g_source_id;
    request.camera_id_ = 7;
    request.channel_id_ = 9;
    request.target_track_id_ = 42;
    request.expected_samples_ = 2;
    request.expected_gallery_revision_ = 1;
    face_enrollment_status status;
    assert(controller.vqec_vision_ai_ports_fenrl_begin(request, status).code_ ==
        status_code::ok);
    assert(controller.vqec_vision_ai_ports_fenrl_accept_embedding(
        vqec_vision_ai_unit_fenct_embedding(1, 42), status).code_ == status_code::ok);
    assert(status.state_ == face_enrollment_state::collecting &&
        status.accepted_samples_ == 1);
    assert(controller.vqec_vision_ai_ports_fenrl_accept_embedding(
        vqec_vision_ai_unit_fenct_embedding(2, 42), status).code_ == status_code::ok);
    assert(status.state_ == face_enrollment_state::completed &&
        status.accepted_samples_ == 2 && status.gallery_revision_ == 3);
    std::uint64_t removed_revision = 0;
    assert(controller.vqec_vision_ai_ports_fenrl_remove_subject(
        g_subject_ref, status.gallery_revision_, removed_revision).code_ == status_code::ok);
    assert(removed_revision == 5);
    std::cout << "face enrollment controller passed\n";
    return 0;
}
