#include "vqec_vision_face_enrollment_image_pipeline.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace vqec::vision::ai {

status face_enrollment_image_pipeline::vqec_vision_ai_appl_feipl_configure(
    const face_enrollment_image_pipeline_config& _config) {
    if (is_configured_ || _config.controller_ == nullptr ||
        _config.path_authorizer_ == nullptr || _config.image_source_ == nullptr ||
        _config.detector_ == nullptr || _config.cascade_ == nullptr ||
        _config.geometry_.width_ == 0 || _config.geometry_.height_ == 0 ||
        _config.source_epoch_ == 0) {
        return {status_code::invalid_argument, "invalid enrollment image pipeline config"};
    }
    config_ = _config;
    is_configured_ = true;
    return {};
}

status face_enrollment_image_pipeline::vqec_vision_ai_ports_fenrl_begin(
    const face_enrollment_begin_request& _request, face_enrollment_status& _status) {
    if (!is_configured_) {
        return {status_code::invalid_state, "enrollment image pipeline is not configured"};
    }
    if (!_request.image_path_.empty() &&
        (_request.expected_samples_ != 1 || _request.target_track_id_ != 0)) {
        return {status_code::invalid_argument,
            "one enrollment image requires one automatically selected face sample"};
    }
    if (has_pending_ && _request.request_id_ != pending_.request_id_) {
        return {status_code::resource_exhausted, "enrollment image pipeline already has a job"};
    }
    const auto begun = _request.image_path_.empty() ?
        config_.controller_->vqec_vision_ai_ports_fenrl_begin(_request, _status) :
        config_.controller_->vqec_vision_ai_ports_fenrl_begin_image(_request, _status);
    if (begun.code_ != status_code::ok || _request.image_path_.empty() || has_pending_) {
        return begun;
    }
    pending_ = _request;
    has_pending_ = true;
    return {};
}

status face_enrollment_image_pipeline::vqec_vision_ai_appl_feipl_step(
    std::uint64_t _steady_now_ns) {
    if (!is_configured_) {
        return {status_code::invalid_state, "enrollment image pipeline is not configured"};
    }
    if (!has_pending_) return {status_code::pending, "enrollment image pipeline is idle"};
    if (_steady_now_ns == std::numeric_limits<std::uint64_t>::max() ||
        next_frame_id_ == 0 || next_frame_id_ == std::numeric_limits<std::uint64_t>::max()) {
        return vqec_vision_ai_appl_feipl_fail_pending(status_code::invalid_state);
    }
    std::string authorized_path;
    auto result = config_.path_authorizer_->vqec_vision_ai_ports_ipath_authorize(
        pending_.image_path_, authorized_path);
    if (result.code_ != status_code::ok) {
        (void)vqec_vision_ai_appl_feipl_fail_pending(result.code_);
        return result;
    }
    face_enrollment_image image;
    const face_enrollment_image_request image_request{authorized_path, next_frame_id_,
        config_.source_epoch_, _steady_now_ns, config_.geometry_.width_,
        config_.geometry_.height_};
    result = config_.image_source_->vqec_vision_ai_ports_feimg_load(image_request, image);
    if (result.code_ != status_code::ok) {
        (void)vqec_vision_ai_appl_feipl_fail_pending(result.code_);
        return result;
    }
    observation_batch detections;
    result = config_.detector_->vqec_vision_ai_ports_fidet_run(
        image.frame_, _steady_now_ns, detections);
    if (result.code_ != status_code::ok) {
        (void)vqec_vision_ai_appl_feipl_fail_pending(result.code_);
        return result;
    }
    const auto eligible = static_cast<std::size_t>(std::count_if(
        detections.observations_.begin(), detections.observations_.end(),
        [](const observation& _item) {
            return !_item.landmarks_.schema_id_.empty() && !_item.landmarks_.points_.empty();
        }));
    std::vector<embedding_result> embeddings;
    std::size_t failed_tasks = 0;
    result = config_.cascade_->vqec_vision_ai_ports_ficas_run(
        image.frame_, detections, _steady_now_ns, embeddings, failed_tasks);
    if (result.code_ != status_code::ok || failed_tasks != 0 || eligible != 1 ||
        embeddings.size() != 1) {
        const auto error = result.code_ == status_code::ok ?
            status_code::invalid_argument : result.code_;
        (void)vqec_vision_ai_appl_feipl_fail_pending(error);
        return {error, "enrollment image must produce exactly one embedded face"};
    }
    face_enrollment_status enrollment_status;
    result = config_.controller_->vqec_vision_ai_ports_fenrl_accept_batch(
        pending_.source_id_, embeddings, eligible, enrollment_status);
    if (result.code_ != status_code::ok) {
        (void)vqec_vision_ai_appl_feipl_fail_pending(result.code_);
        return result;
    }
    ++next_frame_id_;
    pending_ = {};
    has_pending_ = false;
    return {};
}

status face_enrollment_image_pipeline::vqec_vision_ai_appl_feipl_fail_pending(
    status_code _error) {
    face_enrollment_status ignored;
    const auto failed = config_.controller_->vqec_vision_ai_ports_fenrl_fail(
        pending_.request_id_, _error, ignored);
    pending_ = {};
    has_pending_ = false;
    return failed;
}

bool face_enrollment_image_pipeline::vqec_vision_ai_appl_feipl_has_pending() const noexcept {
    return has_pending_;
}

status face_enrollment_image_pipeline::vqec_vision_ai_ports_fenrl_cancel(
    const std::string& _request_id, face_enrollment_status& _status) {
    if (!is_configured_) return {status_code::invalid_state, "pipeline is not configured"};
    const auto result = config_.controller_->vqec_vision_ai_ports_fenrl_cancel(
        _request_id, _status);
    if (result.code_ == status_code::ok && has_pending_ &&
        pending_.request_id_ == _request_id) {
        pending_ = {};
        has_pending_ = false;
    }
    return result;
}

status face_enrollment_image_pipeline::vqec_vision_ai_ports_fenrl_remove_subject(
    const std::string& _subject_ref, std::uint64_t _expected_gallery_revision,
    std::uint64_t& _new_gallery_revision) {
    if (!is_configured_) return {status_code::invalid_state, "pipeline is not configured"};
    return config_.controller_->vqec_vision_ai_ports_fenrl_remove_subject(
        _subject_ref, _expected_gallery_revision, _new_gallery_revision);
}

status face_enrollment_image_pipeline::vqec_vision_ai_ports_fenrl_get_status(
    const std::string& _request_id, face_enrollment_status& _status) const {
    if (!is_configured_) return {status_code::invalid_state, "pipeline is not configured"};
    return config_.controller_->vqec_vision_ai_ports_fenrl_get_status(_request_id, _status);
}

status face_enrollment_image_pipeline::vqec_vision_ai_ports_fenrl_fail(
    const std::string& _request_id, status_code _error, face_enrollment_status& _status) {
    if (!is_configured_) return {status_code::invalid_state, "pipeline is not configured"};
    const auto result = config_.controller_->vqec_vision_ai_ports_fenrl_fail(
        _request_id, _error, _status);
    if (result.code_ == status_code::ok && has_pending_ &&
        pending_.request_id_ == _request_id) {
        pending_ = {};
        has_pending_ = false;
    }
    return result;
}

status face_enrollment_image_pipeline::vqec_vision_ai_ports_fenrl_accept_embedding(
    const embedding_result& _embedding, face_enrollment_status& _status) {
    if (!is_configured_) return {status_code::invalid_state, "pipeline is not configured"};
    return config_.controller_->vqec_vision_ai_ports_fenrl_accept_embedding(
        _embedding, _status);
}

status face_enrollment_image_pipeline::vqec_vision_ai_ports_fenrl_accept_batch(
    const std::string& _source_id, const std::vector<embedding_result>& _embeddings,
    std::size_t _eligible_face_count, face_enrollment_status& _status) {
    if (!is_configured_) return {status_code::invalid_state, "pipeline is not configured"};
    return config_.controller_->vqec_vision_ai_ports_fenrl_accept_batch(
        _source_id, _embeddings, _eligible_face_count, _status);
}

}  // namespace vqec::vision::ai
