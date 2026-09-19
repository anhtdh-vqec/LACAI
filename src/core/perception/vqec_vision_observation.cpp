#include "vqec/vision/ai/contracts/perception/vqec_vision_observation.hpp"

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"
#include "vqec/vision/ai/contracts/media/vqec_vision_preview_limits.hpp"

#include <cmath>

namespace vqec::vision::ai {
namespace {
bool vqec_vision_ai_core_obval_valid_identifier(const std::string& _value) noexcept {
    return vqec_vision_ai_cntr_ident_is_valid(
        _value, observation_limits::g_max_identifier_bytes);
}
}  // namespace

status vqec_vision_ai_core_obval_validate_observations(
    const observation_batch& _batch, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry, bool _require_track_id) {
    const auto identity = vqec_vision_ai_core_pvctr_validate_identity(
        _batch.frame_, _expected_frame, _batch.geometry_, _expected_geometry);
    if (identity.code_ != status_code::ok) {
        return identity;
    }
    if (_batch.observations_.size() > observation_limits::g_max_observations) {
        return {status_code::resource_exhausted, "observation count exceeds safety limit"};
    }
    for (std::size_t item_index = 0; item_index < _batch.observations_.size(); ++item_index) {
        const auto& item = _batch.observations_[item_index];
        const auto item_identity = vqec_vision_ai_core_pvctr_validate_identity(
            item.frame_, _batch.frame_, _batch.geometry_, _batch.geometry_);
        if (item_identity.code_ != status_code::ok) {
            return item_identity;
        }
        const auto valid_box = vqec_vision_ai_core_pvctr_validate_box(item.box_, _batch.geometry_);
        if (valid_box.code_ != status_code::ok) {
            return valid_box;
        }
        if ((_require_track_id && item.track_id_ == 0) ||
            !vqec_vision_ai_core_obval_valid_identifier(item.class_id_) ||
            !std::isfinite(item.confidence_) ||
            item.confidence_ < observation_limits::g_min_confidence ||
            item.confidence_ > observation_limits::g_max_confidence ||
            item.attributes_.size() > observation_limits::g_max_attributes) {
            return {status_code::invalid_argument, "invalid observation identity or confidence"};
        }
        if (item.track_id_ != 0) {
            for (std::size_t previous = 0; previous < item_index; ++previous) {
                if (_batch.observations_[previous].track_id_ == item.track_id_) {
                    return {status_code::invalid_argument, "duplicate nonzero track ID"};
                }
            }
        }
        const auto& landmarks = item.landmarks_;
        if ((!landmarks.points_.empty() &&
                (!vqec_vision_ai_core_obval_valid_identifier(landmarks.schema_id_) ||
                    !vqec_vision_ai_core_obval_valid_identifier(
                        landmarks.schema_version_))) ||
            (landmarks.points_.empty() &&
                (!landmarks.schema_id_.empty() || !landmarks.schema_version_.empty())) ||
            landmarks.points_.size() > observation_limits::g_max_landmark_points) {
            return {status_code::invalid_argument, "invalid observation landmark identity"};
        }
        for (const auto& point : landmarks.points_) {
            if (!std::isfinite(point.x_) || !std::isfinite(point.y_) || point.x_ < 0.0F ||
                point.y_ < 0.0F || point.x_ >= _batch.geometry_.width_ ||
                point.y_ >= _batch.geometry_.height_) {
                return {status_code::invalid_argument,
                    "observation landmark is outside source geometry"};
            }
        }
        for (std::size_t attribute_index = 0;
             attribute_index < item.attributes_.size(); ++attribute_index) {
            const auto& attribute = item.attributes_[attribute_index];
            if (!vqec_vision_ai_core_obval_valid_identifier(attribute.schema_id_) ||
                !vqec_vision_ai_core_obval_valid_identifier(attribute.schema_version_) ||
                attribute.value_.size() > observation_limits::g_max_attribute_value_bytes ||
                !std::isfinite(attribute.confidence_) ||
                attribute.confidence_ < observation_limits::g_min_confidence ||
                attribute.confidence_ > observation_limits::g_max_confidence ||
                attribute.observed_at_ns_ > attribute.expires_at_ns_ ||
                attribute.expires_at_ns_ == UINT64_MAX) {
                return {status_code::invalid_argument, "invalid observation attribute"};
            }
            for (std::size_t previous = 0; previous < attribute_index; ++previous) {
                const auto& other = item.attributes_[previous];
                if (other.schema_id_ == attribute.schema_id_ &&
                    other.schema_version_ == attribute.schema_version_) {
                    return {status_code::invalid_argument, "duplicate observation attribute"};
                }
            }
        }
    }
    return {};
}

status vqec_vision_ai_core_obval_validate_detections(
    const observation_batch& _batch, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry) {
    return vqec_vision_ai_core_obval_validate_observations(
        _batch, _expected_frame, _expected_geometry, false);
}

status vqec_vision_ai_core_obval_validate_batch(
    const observation_batch& _batch, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry) {
    return vqec_vision_ai_core_obval_validate_observations(
        _batch, _expected_frame, _expected_geometry, true);
}

}  // namespace vqec::vision::ai
