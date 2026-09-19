#include "vqec/vision/ai/contracts/inference/vqec_vision_preprocess_spec.hpp"

#include <cmath>

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_ppspc_finite_in_range(float _value, float _low, float _high) noexcept {
    return std::isfinite(_value) && _value >= _low && _value <= _high;
}

}  // namespace

status vqec_vision_ai_core_ppspc_validate(const preprocess_spec& _spec) noexcept {
    switch (_spec.source_format_) {
        case source_pixel_format::nv12:
        case source_pixel_format::rgb888:
        case source_pixel_format::bgr888:
        case source_pixel_format::rgba8888:
            break;
        default:
            return {status_code::invalid_argument, "preprocess source pixel format is unset"};
    }
    switch (_spec.matrix_) {
        case color_matrix::bt601:
        case color_matrix::bt709:
        case color_matrix::bt2020:
            break;
        default:
            return {status_code::invalid_argument, "preprocess color matrix is unspecified"};
    }
    switch (_spec.range_) {
        case color_range::limited:
        case color_range::full:
            break;
        default:
            return {status_code::invalid_argument, "preprocess color range is unspecified"};
    }
    switch (_spec.resize_) {
        case resize_mode::stretch:
        case resize_mode::letterbox:
        case resize_mode::crop:
            break;
        default:
            return {status_code::invalid_argument, "preprocess resize mode is invalid"};
    }
    switch (_spec.interpolation_) {
        case interpolation_mode::nearest:
        case interpolation_mode::bilinear:
        case interpolation_mode::area:
            break;
        default:
            return {status_code::invalid_argument, "preprocess interpolation mode is invalid"};
    }
    switch (_spec.placement_) {
        case image_placement::top_left:
        case image_placement::centre:
        case image_placement::stretch:
            break;
        default:
            return {status_code::invalid_argument, "preprocess placement is invalid"};
    }
    for (const float pad : _spec.pad_value_) {
        if (!vqec_vision_ai_core_ppspc_finite_in_range(pad, 0.0F, 255.0F)) {
            return {status_code::invalid_argument, "preprocess pad value is out of 0..255"};
        }
    }
    switch (_spec.channels_) {
        case channel_order::rgb:
        case channel_order::bgr:
            break;
        default:
            return {status_code::invalid_argument, "preprocess channel order is invalid"};
    }
    switch (_spec.normalization_) {
        case normalization_formula::offset_scale:
        case normalization_formula::mean_std:
        case normalization_formula::none:
            break;
        default:
            return {status_code::invalid_argument, "preprocess normalization formula is invalid"};
    }
    for (const float value : _spec.offset_) {
        if (!std::isfinite(value)) {
            return {status_code::invalid_argument, "preprocess offset is not finite"};
        }
    }
    for (const float value : _spec.scale_) {
        if (!std::isfinite(value) || value == 0.0F) {
            return {status_code::invalid_argument, "preprocess scale is zero or not finite"};
        }
    }
    switch (_spec.coordinates_) {
        case coordinate_convention::tensor_pixels_xywh:
        case coordinate_convention::tensor_pixels_xyxy:
        case coordinate_convention::normalized_xywh:
        case coordinate_convention::normalized_xyxy:
            break;
        default:
            return {status_code::invalid_argument, "preprocess coordinate convention is invalid"};
    }
    return {};
}

}  // namespace vqec::vision::ai
