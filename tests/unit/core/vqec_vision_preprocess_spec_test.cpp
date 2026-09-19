// Device-free tests for the authoritative preprocess contract validator.

#include <cstdint>
#include <iostream>
#include <limits>

#include "vqec/vision/ai/contracts/inference/vqec_vision_preprocess_spec.hpp"

using namespace vqec::vision::ai;

namespace {

preprocess_spec make_valid() {
    preprocess_spec spec;
    spec.source_format_ = source_pixel_format::nv12;
    spec.matrix_ = color_matrix::bt709;
    spec.range_ = color_range::limited;
    spec.resize_ = resize_mode::letterbox;
    spec.interpolation_ = interpolation_mode::bilinear;
    spec.placement_ = image_placement::centre;
    spec.pad_value_ = {114.0F, 114.0F, 114.0F};
    spec.channels_ = channel_order::rgb;
    spec.normalization_ = normalization_formula::offset_scale;
    spec.offset_ = {0.0F, 0.0F, 0.0F};
    spec.scale_ = {1.0F / 255.0F, 1.0F / 255.0F, 1.0F / 255.0F};
    spec.coordinates_ = coordinate_convention::tensor_pixels_xywh;
    return spec;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    const auto spec = make_valid();
    check(vqec_vision_ai_core_ppspc_validate(spec).code_ == status_code::ok);

    check(vqec_vision_ai_core_ppspc_validate(
              [] { auto s = make_valid(); s.source_format_ = source_pixel_format::unknown; return s; }())
              .code_ == status_code::invalid_argument);
    check(vqec_vision_ai_core_ppspc_validate(
              [] { auto s = make_valid(); s.matrix_ = color_matrix::unspecified; return s; }())
              .code_ == status_code::invalid_argument);
    check(vqec_vision_ai_core_ppspc_validate(
              [] { auto s = make_valid(); s.range_ = color_range::unspecified; return s; }())
              .code_ == status_code::invalid_argument);
    check(vqec_vision_ai_core_ppspc_validate(
              [] { auto s = make_valid(); s.matrix_ = static_cast<color_matrix>(99); return s; }())
              .code_ == status_code::invalid_argument);
    check(vqec_vision_ai_core_ppspc_validate(
              [] { auto s = make_valid(); s.pad_value_[1] = 300.0F; return s; }())
              .code_ == status_code::invalid_argument);
    check(vqec_vision_ai_core_ppspc_validate(
              [] { auto s = make_valid(); s.pad_value_[0] = std::numeric_limits<float>::quiet_NaN();
                   return s; }())
              .code_ == status_code::invalid_argument);
    check(vqec_vision_ai_core_ppspc_validate(
              [] { auto s = make_valid(); s.scale_[2] = 0.0F; return s; }())
              .code_ == status_code::invalid_argument);
    check(vqec_vision_ai_core_ppspc_validate(
              [] { auto s = make_valid(); s.offset_[0] = std::numeric_limits<float>::infinity();
                   return s; }())
              .code_ == status_code::invalid_argument);

    std::cout << "preprocess spec failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
