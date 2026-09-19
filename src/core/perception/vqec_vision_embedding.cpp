#include "vqec/vision/ai/contracts/perception/vqec_vision_embedding.hpp"

#include <cmath>
#include <limits>

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {

status vqec_vision_ai_core_embct_validate_result(
    const embedding_result& _result, const preview_frame_key& _expected_frame) {
    if (_result.frame_.camera_id_ != _expected_frame.camera_id_ ||
        _result.frame_.channel_id_ != _expected_frame.channel_id_ ||
        _result.frame_.source_epoch_ == 0 ||
        _result.frame_.source_epoch_ != _expected_frame.source_epoch_ ||
        _result.frame_.frame_id_ != _expected_frame.frame_id_ ||
        _result.frame_.source_pts_ns_ == UINT64_MAX ||
        _result.frame_.source_pts_ns_ != _expected_frame.source_pts_ns_ ||
        _result.track_id_ == 0 ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _result.model_id_, embedding_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _result.model_version_, embedding_limits::g_max_identifier_bytes) ||
        _result.values_.empty() ||
        _result.values_.size() > embedding_limits::g_max_dimensions) {
        return {status_code::invalid_argument, "invalid embedding identity or dimensions"};
    }
    double squared_norm = 0.0;
    for (const float value : _result.values_) {
        if (!std::isfinite(value)) {
            return {status_code::invalid_argument, "embedding contains a non-finite value"};
        }
        squared_norm += static_cast<double>(value) * value;
    }
    if (!(squared_norm > std::numeric_limits<double>::epsilon())) {
        return {status_code::invalid_argument, "embedding norm is zero"};
    }
    if (_result.is_l2_normalized_ &&
        std::fabs(std::sqrt(squared_norm) - 1.0) >
            embedding_limits::g_normalized_tolerance) {
        return {status_code::invalid_argument, "normalized embedding norm differs from one"};
    }
    return {};
}

}  // namespace vqec::vision::ai
