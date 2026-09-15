#include "vqec/vision/ai/contracts/vqec_vision_image_alignment.hpp"

#include <cmath>
#include <limits>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_imaln_is_identifier(const std::string& _value) noexcept {
    return vqec_vision_ai_cntr_ident_is_valid(
        _value, observation_limits::g_max_identifier_bytes);
}

bool vqec_vision_ai_core_imaln_are_finite(
    const std::vector<landmark_point>& _points) noexcept {
    for (const auto& point : _points) {
        if (!std::isfinite(point.x_) || !std::isfinite(point.y_)) {
            return false;
        }
    }
    return true;
}

}  // namespace

status vqec_vision_ai_core_imaln_validate_template(const alignment_template& _template) {
    if (!vqec_vision_ai_core_imaln_is_identifier(_template.schema_id_) ||
        !vqec_vision_ai_core_imaln_is_identifier(_template.schema_version_)) {
        return {status_code::invalid_argument, "alignment template schema is invalid"};
    }
    if (_template.destination_width_ < image_alignment_limits::g_min_destination_dimension ||
        _template.destination_width_ > image_alignment_limits::g_max_destination_dimension ||
        _template.destination_height_ < image_alignment_limits::g_min_destination_dimension ||
        _template.destination_height_ > image_alignment_limits::g_max_destination_dimension) {
        return {status_code::invalid_argument, "alignment destination geometry is invalid"};
    }
    if (_template.reference_points_.empty() ||
        _template.reference_points_.size() > image_alignment_limits::g_max_points ||
        !vqec_vision_ai_core_imaln_are_finite(_template.reference_points_)) {
        return {status_code::invalid_argument, "alignment reference points are invalid"};
    }
    return {};
}

status vqec_vision_ai_core_imaln_validate_request(
    const alignment_request& _request, const alignment_template& _template) {
    const auto valid_template = vqec_vision_ai_core_imaln_validate_template(_template);
    if (valid_template.code_ != status_code::ok) {
        return valid_template;
    }
    if (_request.frame_.source_epoch_ == 0 || _request.frame_.frame_id_ == 0 ||
        _request.frame_.source_pts_ns_ == std::numeric_limits<std::uint64_t>::max() ||
        _request.deadline_ns_ == std::numeric_limits<std::uint64_t>::max()) {
        return {status_code::invalid_argument, "alignment request identity is invalid"};
    }
    if (_request.landmarks_.schema_id_ != _template.schema_id_ ||
        _request.landmarks_.schema_version_ != _template.schema_version_) {
        return {status_code::invalid_argument, "alignment landmark schema differs from template"};
    }
    if (_request.landmarks_.points_.size() != _template.reference_points_.size() ||
        !vqec_vision_ai_core_imaln_are_finite(_request.landmarks_.points_)) {
        return {status_code::invalid_argument, "alignment landmarks do not match the template"};
    }
    return {};
}

status vqec_vision_ai_core_imaln_require_capability(
    const alignment_capabilities& _capabilities, const alignment_template& _template) {
    const auto valid_template = vqec_vision_ai_core_imaln_validate_template(_template);
    if (valid_template.code_ != status_code::ok) {
        return valid_template;
    }
    if (!_capabilities.supports_similarity_) {
        return {status_code::unsupported,
            "alignment backend does not support the similarity transform"};
    }
    if (_capabilities.max_points_ < _template.reference_points_.size()) {
        return {status_code::unsupported, "alignment backend point capacity is too small"};
    }
    if (_capabilities.max_destination_dimension_ < _template.destination_width_ ||
        _capabilities.max_destination_dimension_ < _template.destination_height_) {
        return {status_code::unsupported, "alignment backend destination size is too small"};
    }
    return {};
}

}  // namespace vqec::vision::ai
