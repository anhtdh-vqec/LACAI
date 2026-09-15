#include "vqec/vision/ai/contracts/vqec_vision_secondary_inference.hpp"

#include <cmath>
#include <limits>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {

status vqec_vision_ai_core_secin_validate_request(
    const secondary_inference_request& _request) {
    if (_request.task_id_ == 0 ||
        _request.source_slot_ >= secondary_inference_limits::g_max_sources ||
        _request.model_slot_ >= secondary_inference_limits::g_max_models ||
        _request.source_epoch_ == 0 || _request.source_frame_id_ == 0 ||
        _request.source_pts_ns_ == std::numeric_limits<std::uint64_t>::max()) {
        return {status_code::invalid_argument, "invalid secondary inference identity"};
    }
    if (_request.has_roi_ &&
        (_request.roi_.width_ <= 0.0F || _request.roi_.height_ <= 0.0F)) {
        return {status_code::invalid_argument, "secondary inference ROI has no extent"};
    }
    if (_request.requires_retained_frame_ && _request.retention_ticket_ == 0) {
        return {status_code::invalid_argument,
            "retained secondary task requires a frame store ticket"};
    }
    if (_request.has_landmarks_) {
        if (!vqec_vision_ai_cntr_ident_is_valid(
                _request.landmarks_.schema_id_, observation_limits::g_max_identifier_bytes) ||
            !vqec_vision_ai_cntr_ident_is_valid(_request.landmarks_.schema_version_,
                observation_limits::g_max_identifier_bytes) ||
            _request.landmarks_.points_.empty() ||
            _request.landmarks_.points_.size() > observation_limits::g_max_landmark_points) {
            return {status_code::invalid_argument, "secondary landmark set is invalid"};
        }
        for (const auto& point : _request.landmarks_.points_) {
            if (!std::isfinite(point.x_) || !std::isfinite(point.y_)) {
                return {status_code::invalid_argument, "secondary landmark is not finite"};
            }
        }
    }
    switch (_request.priority_) {
        case secondary_priority::low:
        case secondary_priority::normal:
        case secondary_priority::high:
            break;
        default:
            return {status_code::invalid_argument, "invalid secondary inference priority"};
    }
    return {};
}

status vqec_vision_ai_core_secin_validate_result(
    const secondary_inference_result& _result) {
    const auto valid = vqec_vision_ai_core_secin_validate_request(_result.request_);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (_result.payload_.size() > secondary_inference_limits::g_max_payload_bytes) {
        return {status_code::resource_exhausted, "secondary result payload exceeds the bound"};
    }
    return {};
}

}  // namespace vqec::vision::ai
