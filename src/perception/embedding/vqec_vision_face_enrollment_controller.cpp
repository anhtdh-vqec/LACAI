#include "vqec_vision_face_enrollment_controller.hpp"

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {

face_enrollment_controller::face_enrollment_controller(
    recognition_session& _session) noexcept : session_(_session) {}

status face_enrollment_controller::vqec_vision_ai_ports_fenrl_begin(
    const face_enrollment_begin_request& _request, face_enrollment_status& _status) {
    if (status_.state_ == face_enrollment_state::collecting ||
        _request.request_id_.empty() ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _request.request_id_, face_enrollment_limits::g_max_request_id_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _request.subject_ref_, embedding_index_limits::g_max_subject_ref_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _request.source_id_, face_enrollment_limits::g_max_source_id_bytes) ||
        _request.expected_samples_ == 0 ||
        _request.expected_samples_ > face_enrollment_limits::g_max_samples_per_request ||
        _request.expected_gallery_revision_ == 0 ||
        _request.expected_gallery_revision_ == UINT64_MAX ||
        _request.camera_id_ == 0 || _request.channel_id_ == 0) {
        return {status_code::invalid_argument, "face enrollment request is invalid"};
    }
    const auto snapshot = session_.vqec_vision_ai_embed_rcses_get_snapshot();
    if (!snapshot.is_configured_ || snapshot.is_faulted_ ||
        snapshot.gallery_revision_ != _request.expected_gallery_revision_) {
        return {status_code::invalid_state, "face enrollment gallery is unavailable"};
    }
    status_ = {};
    status_.request_id_ = _request.request_id_;
    status_.subject_ref_ = _request.subject_ref_;
    status_.state_ = face_enrollment_state::collecting;
    status_.expected_samples_ = _request.expected_samples_;
    status_.gallery_revision_ = snapshot.gallery_revision_;
    request_ = _request;
    _status = status_;
    return {};
}

status face_enrollment_controller::vqec_vision_ai_ports_fenrl_cancel(
    const std::string& _request_id, face_enrollment_status& _status) {
    if (status_.state_ != face_enrollment_state::collecting ||
        _request_id != status_.request_id_) {
        return {status_code::invalid_state, "face enrollment request is not collecting"};
    }
    status_.state_ = face_enrollment_state::cancelled;
    status_.last_error_ = status_code::ok;
    _status = status_;
    return {};
}

status face_enrollment_controller::vqec_vision_ai_ports_fenrl_remove_subject(
    const std::string& _subject_ref, std::uint64_t _expected_gallery_revision,
    std::uint64_t& _new_gallery_revision) {
    if (status_.state_ == face_enrollment_state::collecting) {
        return {status_code::invalid_state, "cancel enrollment before removing a subject"};
    }
    return session_.vqec_vision_ai_embed_rcses_remove_subject(
        _subject_ref, _expected_gallery_revision, _new_gallery_revision);
}

status face_enrollment_controller::vqec_vision_ai_ports_fenrl_get_status(
    const std::string& _request_id, face_enrollment_status& _status) const {
    if (_request_id != status_.request_id_ || status_.request_id_.empty()) {
        return {status_code::invalid_argument, "face enrollment request is unknown"};
    }
    _status = status_;
    return {};
}

status face_enrollment_controller::vqec_vision_ai_ports_fenrl_accept_embedding(
    const embedding_result& _embedding, face_enrollment_status& _status) {
    if (status_.state_ != face_enrollment_state::collecting) {
        return {status_code::invalid_state, "face enrollment is not collecting"};
    }
    if (_embedding.frame_.camera_id_ != request_.camera_id_ ||
        _embedding.frame_.channel_id_ != request_.channel_id_ ||
        (request_.target_track_id_ != 0 &&
            _embedding.track_id_ != request_.target_track_id_)) {
        return {status_code::invalid_argument,
            "embedding does not belong to the enrollment source or track"};
    }
    const auto added = session_.vqec_vision_ai_embed_rcses_add_template(
        status_.subject_ref_, _embedding, status_.gallery_revision_, record_id_,
        status_.gallery_revision_);
    if (added.code_ != status_code::ok) {
        status_.state_ = face_enrollment_state::failed;
        status_.last_error_ = added.code_;
        _status = status_;
        return added;
    }
    ++status_.accepted_samples_;
    if (status_.accepted_samples_ == status_.expected_samples_) {
        status_.state_ = face_enrollment_state::completed;
    }
    _status = status_;
    return {};
}

}  // namespace vqec::vision::ai
