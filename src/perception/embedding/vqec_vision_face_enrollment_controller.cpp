#include "vqec_vision_face_enrollment_controller.hpp"

#include <algorithm>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {

face_enrollment_controller::face_enrollment_controller(
    recognition_session& _session) noexcept : session_(_session) {}

status face_enrollment_controller::vqec_vision_ai_ports_fenrl_begin(
    const face_enrollment_begin_request& _request, face_enrollment_status& _status) {
    if (!_request.image_path_.empty()) {
        return {status_code::unsupported,
            "image-path enrollment requires the authorized file pipeline"};
    }
    return vqec_vision_ai_embed_fenrc_begin_request(_request, _status);
}

status face_enrollment_controller::vqec_vision_ai_ports_fenrl_begin_image(
    const face_enrollment_begin_request& _request, face_enrollment_status& _status) {
    if (_request.image_path_.empty()) {
        return {status_code::invalid_argument, "file enrollment requires an image path"};
    }
    return vqec_vision_ai_embed_fenrc_begin_request(_request, _status);
}

status face_enrollment_controller::vqec_vision_ai_embed_fenrc_begin_request(
    const face_enrollment_begin_request& _request, face_enrollment_status& _status) {
    if (!status_.request_id_.empty() && _request.request_id_ == status_.request_id_) {
        if (_request.subject_ref_ != request_.subject_ref_ ||
            _request.image_path_ != request_.image_path_ ||
            _request.source_id_ != request_.source_id_ ||
            _request.camera_id_ != request_.camera_id_ ||
            _request.channel_id_ != request_.channel_id_ ||
            _request.target_track_id_ != request_.target_track_id_ ||
            _request.expected_samples_ != request_.expected_samples_ ||
            _request.expected_gallery_revision_ != request_.expected_gallery_revision_) {
            return {status_code::invalid_argument, "enrollment request ID payload conflict"};
        }
        _status = status_;
        return {};
    }
    if (status_.state_ == face_enrollment_state::collecting) {
        return {status_code::invalid_state, "another face enrollment is collecting"};
    }
    if (_request.request_id_.empty() ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _request.request_id_, face_enrollment_limits::g_max_request_id_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _request.subject_ref_, embedding_index_limits::g_max_subject_ref_bytes) ||
        _request.image_path_.size() > face_enrollment_limits::g_max_image_path_bytes ||
        _request.image_path_.find('\0') != std::string::npos ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _request.source_id_, face_enrollment_limits::g_max_source_id_bytes) ||
        _request.expected_samples_ == 0 ||
        _request.expected_samples_ > face_enrollment_limits::g_max_samples_per_request ||
        _request.expected_gallery_revision_ == 0 ||
        _request.expected_gallery_revision_ == UINT64_MAX) {
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
    last_accepted_frame_id_ = 0;
    last_accepted_pts_ns_ = 0;
    accepted_epoch_ = 0;
    selected_track_id_ = _request.target_track_id_;
    _status = status_;
    return {};
}

status face_enrollment_controller::vqec_vision_ai_ports_fenrl_cancel(
    const std::string& _request_id, face_enrollment_status& _status) {
    if (_request_id == status_.request_id_ &&
        status_.state_ == face_enrollment_state::cancelled) {
        _status = status_;
        return {};
    }
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

status face_enrollment_controller::vqec_vision_ai_ports_fenrl_get_gallery_status(
    face_gallery_status& _status) const {
    const auto snapshot = session_.vqec_vision_ai_embed_rcses_get_snapshot();
    face_gallery_status result;
    result.gallery_revision_ = snapshot.gallery_revision_;
    result.subject_count_ = snapshot.subject_count_;
    result.template_count_ = snapshot.template_count_;
    result.is_available_ = snapshot.is_configured_ && !snapshot.is_faulted_;
    result.is_faulted_ = snapshot.is_faulted_;
    _status = result;
    return {};
}

status face_enrollment_controller::vqec_vision_ai_ports_fenrl_fail(
    const std::string& _request_id, status_code _error,
    face_enrollment_status& _status) {
    if (_request_id != status_.request_id_ ||
        status_.state_ != face_enrollment_state::collecting ||
        _error == status_code::ok || _error == status_code::pending) {
        return {status_code::invalid_state, "enrollment request cannot transition to failed"};
    }
    status_.state_ = face_enrollment_state::failed;
    status_.last_error_ = _error;
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
        (selected_track_id_ != 0 && _embedding.track_id_ != selected_track_id_)) {
        return {status_code::invalid_argument,
            "embedding does not belong to the enrollment source or track"};
    }
    if (accepted_epoch_ != 0 && _embedding.frame_.source_epoch_ != accepted_epoch_) {
        status_.state_ = face_enrollment_state::failed;
        status_.last_error_ = status_code::source_lost;
        _status = status_;
        return {status_code::source_lost, "enrollment source epoch changed"};
    }
    const auto valid = vqec_vision_ai_core_embct_validate_result(
        _embedding, _embedding.frame_);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (_embedding.frame_.frame_id_ == 0 ||
        _embedding.frame_.frame_id_ <= last_accepted_frame_id_ ||
        (last_accepted_frame_id_ != 0 &&
            _embedding.frame_.source_pts_ns_ <= last_accepted_pts_ns_)) {
        return {status_code::invalid_argument,
            "face enrollment requires one sample from each source frame"};
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
    last_accepted_frame_id_ = _embedding.frame_.frame_id_;
    last_accepted_pts_ns_ = _embedding.frame_.source_pts_ns_;
    accepted_epoch_ = _embedding.frame_.source_epoch_;
    selected_track_id_ = _embedding.track_id_;
    if (status_.accepted_samples_ == status_.expected_samples_) {
        status_.state_ = face_enrollment_state::completed;
    }
    _status = status_;
    return {};
}

status face_enrollment_controller::vqec_vision_ai_ports_fenrl_accept_batch(
    const std::string& _source_id, const std::vector<embedding_result>& _embeddings,
    std::size_t _eligible_face_count, face_enrollment_status& _status) {
    if (status_.state_ != face_enrollment_state::collecting) {
        return {status_code::invalid_state, "face enrollment is not collecting"};
    }
    if (_source_id != request_.source_id_ ||
        _embeddings.size() > observation_limits::g_max_observations ||
        _eligible_face_count > observation_limits::g_max_observations) {
        return {status_code::invalid_argument, "enrollment source or batch is invalid"};
    }
    if (request_.target_track_id_ == 0 &&
        (_eligible_face_count != 1 || _embeddings.size() != 1)) {
        return {status_code::pending, "automatic enrollment needs exactly one eligible face"};
    }
    const auto selected = std::find_if(_embeddings.begin(), _embeddings.end(),
        [this](const embedding_result& _embedding) {
            return selected_track_id_ == 0 || _embedding.track_id_ == selected_track_id_;
        });
    if (selected == _embeddings.end()) {
        return {status_code::pending, "enrollment target has no completed embedding"};
    }
    return vqec_vision_ai_ports_fenrl_accept_embedding(*selected, _status);
}

}  // namespace vqec::vision::ai
