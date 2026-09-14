#include "vqec/vision/ai/contracts/vqec_vision_secondary_inference.hpp"

#include <limits>

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
