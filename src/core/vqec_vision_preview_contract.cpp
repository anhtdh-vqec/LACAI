#include "vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp"

#include "vqec/vision/ai/contracts/vqec_vision_preview_limits.hpp"

#include <cmath>

namespace vqec::vision::ai {

status vqec_vision_ai_core_pvctr_validate_identity(
    const preview_frame_key& _frame, const preview_frame_key& _expected_frame,
    const preview_geometry& _geometry, const preview_geometry& _expected_geometry) {
    if (_frame.source_epoch_ == 0 || _frame.source_pts_ns_ == UINT64_MAX ||
        _geometry.width_ == 0 || _geometry.height_ == 0 ||
        _geometry.width_ > preview_limits::g_max_dimension_pixels || _geometry.height_ > preview_limits::g_max_dimension_pixels ||
        _geometry.width_ % 2 != 0 || _geometry.height_ % 2 != 0) {
        return {status_code::invalid_argument, "invalid preview identity or NV12 geometry"};
    }
    if (_frame.camera_id_ != _expected_frame.camera_id_ ||
        _frame.channel_id_ != _expected_frame.channel_id_ ||
        _frame.source_epoch_ != _expected_frame.source_epoch_ ||
        _frame.frame_id_ != _expected_frame.frame_id_ ||
        _frame.source_pts_ns_ != _expected_frame.source_pts_ns_ ||
        _geometry.width_ != _expected_geometry.width_ ||
        _geometry.height_ != _expected_geometry.height_) {
        return {status_code::invalid_state, "preview frame or geometry mismatch"};
    }
    return {};
}

status vqec_vision_ai_core_pvctr_validate_box(
    const overlay_box& _box, const preview_geometry& _geometry) {
    if (!std::isfinite(_box.x_) || !std::isfinite(_box.y_) ||
        !std::isfinite(_box.width_) || !std::isfinite(_box.height_) ||
        _box.x_ < 0 || _box.y_ < 0 || _box.width_ <= 0 || _box.height_ <= 0 ||
        static_cast<double>(_box.x_) + _box.width_ > _geometry.width_ ||
        static_cast<double>(_box.y_) + _box.height_ > _geometry.height_) {
        return {status_code::invalid_argument, "invalid overlay rectangle"};
    }
    if (_box.label_.size() > preview_limits::g_max_label_bytes) {
        return {status_code::resource_exhausted, "preview label limit exceeded"};
    }
    for (const unsigned char character : _box.label_) {
        if (character < preview_limits::g_min_label_character ||
            character > preview_limits::g_max_label_character) {
            return {status_code::unsupported, "preview labels require printable ASCII"};
        }
    }
    return {};
}

status vqec_vision_ai_core_pvctr_validate_overlay(
    const overlay_batch& _batch, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry, std::uint64_t _expected_revision,
    std::uint64_t _now_monotonic_ns, std::uint64_t _max_age_ns) {
    const auto identity = vqec_vision_ai_core_pvctr_validate_identity(
        _batch.frame_, _expected_frame, _batch.geometry_, _expected_geometry);
    if (identity.code_ != status_code::ok) {
        return identity;
    }
    if (_expected_revision == 0 || _batch.policy_revision_ != _expected_revision) {
        return {status_code::unauthorized, "preview policy revision mismatch"};
    }
    if (_max_age_ns == 0 || _batch.prepared_monotonic_ns_ > _now_monotonic_ns) {
        return {status_code::invalid_argument, "invalid preview freshness clock or budget"};
    }
    if (_now_monotonic_ns - _batch.prepared_monotonic_ns_ > _max_age_ns) {
        return {status_code::timeout, "expired preview overlay"};
    }
    if (_batch.boxes_.size() > preview_limits::g_max_overlay_boxes) {
        return {status_code::resource_exhausted, "preview primitive limit exceeded"};
    }
    std::size_t label_bytes = 0;
    for (const auto& box : _batch.boxes_) {
        const auto valid_box = vqec_vision_ai_core_pvctr_validate_box(box, _batch.geometry_);
        if (valid_box.code_ != status_code::ok) {
            return valid_box;
        }
        if (box.label_.size() > preview_limits::g_max_total_label_bytes - label_bytes) {
            return {status_code::resource_exhausted, "preview label limit exceeded"};
        }
        label_bytes += box.label_.size();
    }
    return {};
}

status vqec_vision_ai_core_pvctr_validate_access_unit(
    const h264_access_unit_view& _unit, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry) {
    const auto identity = vqec_vision_ai_core_pvctr_validate_identity(
        _unit.frame_, _expected_frame, _unit.geometry_, _expected_geometry);
    if (identity.code_ != status_code::ok) {
        return identity;
    }
    if (_unit.payload_.size_ > preview_limits::g_max_encoded_payload_bytes ||
        _unit.sps_.size_ > preview_limits::g_max_parameter_set_bytes || _unit.pps_.size_ > preview_limits::g_max_parameter_set_bytes) {
        return {status_code::resource_exhausted, "encoded preview exceeds FW ring limits"};
    }
    if (_unit.payload_.data_ == nullptr || _unit.payload_.size_ < 4 ||
        (_unit.sps_.size_ != 0 && _unit.sps_.data_ == nullptr) ||
        (_unit.pps_.size_ != 0 && _unit.pps_.data_ == nullptr)) {
        return {status_code::invalid_argument, "invalid encoded preview byte view"};
    }
    const auto* bytes = _unit.payload_.data_;
    const bool has_start_code = bytes[0] == 0 && bytes[1] == 0 &&
        (bytes[2] == 1 || (_unit.payload_.size_ >= 5 && bytes[2] == 0 && bytes[3] == 1));
    if (!has_start_code) {
        return {status_code::protocol_error, "expected Annex B access unit"};
    }
    return {};
}

}  // namespace vqec::vision::ai
