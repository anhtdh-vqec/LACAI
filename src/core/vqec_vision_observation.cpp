#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"

#include "vqec/vision/ai/contracts/vqec_vision_preview_limits.hpp"

#include <cmath>

namespace vqec::vision::ai {
namespace {
bool vqec_vision_ai_core_obval_valid_identifier(const std::string& _value) noexcept {
    if (_value.empty() || _value.size() > observation_limits::g_max_identifier_bytes) {
        return false;
    }
    for (const unsigned char character : _value) {
        if (!((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == '_' || character == '-' ||
              character == '.' || character == ':')) {
            return false;
        }
    }
    return true;
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
    for (const auto& item : _batch.observations_) {
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
        for (const auto& attribute : item.attributes_) {
            if (!vqec_vision_ai_core_obval_valid_identifier(attribute.schema_id_) ||
                !vqec_vision_ai_core_obval_valid_identifier(attribute.schema_version_) ||
                !std::isfinite(attribute.confidence_) ||
                attribute.confidence_ < observation_limits::g_min_confidence ||
                attribute.confidence_ > observation_limits::g_max_confidence ||
                attribute.observed_at_ns_ > attribute.expires_at_ns_) {
                return {status_code::invalid_argument, "invalid observation attribute"};
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
