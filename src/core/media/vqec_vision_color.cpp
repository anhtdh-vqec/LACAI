#include "vqec/vision/ai/contracts/media/vqec_vision_color.hpp"

#include "vqec/vision/ai/contracts/media/vqec_vision_nv12_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace vqec::vision::ai {
namespace {

inline constexpr std::size_t g_rgb8_value_count =
    static_cast<std::size_t>(std::numeric_limits<std::uint8_t>::max()) + 1U;
inline constexpr double g_chroma_midpoint =
    static_cast<double>(g_rgb8_value_count / 2U);

struct color_conversion_lut {
    std::array<double, g_rgb8_value_count> luma_{};
    std::array<double, g_rgb8_value_count> red_cr_{};
    std::array<double, g_rgb8_value_count> green_cb_{};
    std::array<double, g_rgb8_value_count> green_cr_{};
    std::array<double, g_rgb8_value_count> blue_cb_{};
};

std::uint8_t vqec_vision_ai_core_color_clamp(double _value) noexcept {
    const double rounded = std::round(_value);
    return static_cast<std::uint8_t>(std::clamp(rounded, 0.0, 255.0));
}

color_conversion_lut vqec_vision_ai_core_color_make_lut(
    color_matrix _matrix, color_range _range) noexcept {
    const bool limited = _range == color_range::limited;
    const double luma_scale = limited ? 1.1643835616438356 : 1.0;
    const double luma_offset = limited ? 16.0 : 0.0;
    double red_cr = 0.0;
    double green_cb = 0.0;
    double green_cr = 0.0;
    double blue_cb = 0.0;
    if (_matrix == color_matrix::bt601) {
        red_cr = limited ? 1.5960267857142858 : 1.402;
        green_cb = limited ? 0.3917622900949137 : 0.34413628620102216;
        green_cr = limited ? 0.8129676472377708 : 0.7141362862010221;
        blue_cb = limited ? 2.017232142857143 : 1.772;
    } else {
        red_cr = limited ? 1.7927410714285714 : 1.5748;
        green_cb = limited ? 0.21324861427372963 : 0.1873242729306488;
        green_cr = limited ? 0.532909328559444 : 0.4681242729306488;
        blue_cb = limited ? 2.112401785714286 : 1.8556;
    }
    color_conversion_lut lut;
    for (std::size_t value = 0; value < lut.luma_.size(); ++value) {
        const double component = static_cast<double>(value) - g_chroma_midpoint;
        lut.luma_[value] = luma_scale * (static_cast<double>(value) - luma_offset);
        lut.red_cr_[value] = red_cr * component;
        lut.green_cb_[value] = green_cb * component;
        lut.green_cr_[value] = green_cr * component;
        lut.blue_cb_[value] = blue_cb * component;
    }
    return lut;
}

const color_conversion_lut& vqec_vision_ai_core_color_get_lut(
    color_matrix _matrix, color_range _range) noexcept {
    static const color_conversion_lut g_bt601_limited =
        vqec_vision_ai_core_color_make_lut(color_matrix::bt601, color_range::limited);
    static const color_conversion_lut g_bt601_full =
        vqec_vision_ai_core_color_make_lut(color_matrix::bt601, color_range::full);
    static const color_conversion_lut g_bt709_limited =
        vqec_vision_ai_core_color_make_lut(color_matrix::bt709, color_range::limited);
    static const color_conversion_lut g_bt709_full =
        vqec_vision_ai_core_color_make_lut(color_matrix::bt709, color_range::full);
    if (_matrix == color_matrix::bt601) {
        return _range == color_range::limited ? g_bt601_limited : g_bt601_full;
    }
    return _range == color_range::limited ? g_bt709_limited : g_bt709_full;
}

}  // namespace

status vqec_vision_ai_core_color_convert_nv12_to_rgb(
    const std::uint8_t* _y_plane, std::uint32_t _y_stride,
    const std::uint8_t* _uv_plane, std::uint32_t _uv_stride,
    std::uint32_t _width, std::uint32_t _height,
    color_matrix _matrix, color_range _range, channel_order _order,
    std::uint8_t* _rgb, std::uint32_t _rgb_stride) noexcept {
    if (_y_plane == nullptr || _uv_plane == nullptr || _rgb == nullptr ||
        !vqec_vision_ai_cntr_nvgeo_is_even_nonzero(_width, _height) ||
        _y_stride < _width || _uv_stride < _width || _rgb_stride < _width * 3U) {
        return {status_code::invalid_argument, "invalid NV12 to RGB conversion arguments"};
    }
    if ((_matrix != color_matrix::bt601 && _matrix != color_matrix::bt709) ||
        (_range != color_range::limited && _range != color_range::full)) {
        return {status_code::unsupported, "unspecified color matrix or range"};
    }
    const auto& lut = vqec_vision_ai_core_color_get_lut(_matrix, _range);
    for (std::uint32_t row = 0; row < _height; ++row) {
        const std::uint8_t* y_row = _y_plane + static_cast<std::size_t>(row) * _y_stride;
        const std::uint8_t* uv_row =
            _uv_plane + static_cast<std::size_t>(row / 2U) * _uv_stride;
        std::uint8_t* out_row = _rgb + static_cast<std::size_t>(row) * _rgb_stride;
        for (std::uint32_t column = 0; column < _width; ++column) {
            const double luma = lut.luma_[y_row[column]];
            const std::size_t chroma_index =
                static_cast<std::size_t>(column / 2U) * 2U;
            const auto cb = uv_row[chroma_index];
            const auto cr = uv_row[chroma_index + 1U];
            const std::uint8_t red =
                vqec_vision_ai_core_color_clamp(luma + lut.red_cr_[cr]);
            const std::uint8_t green =
                vqec_vision_ai_core_color_clamp(
                    luma - lut.green_cb_[cb] - lut.green_cr_[cr]);
            const std::uint8_t blue =
                vqec_vision_ai_core_color_clamp(luma + lut.blue_cb_[cb]);
            std::uint8_t* pixel = out_row + static_cast<std::size_t>(column) * 3U;
            if (_order == channel_order::rgb) {
                pixel[0] = red;
                pixel[1] = green;
                pixel[2] = blue;
            } else {
                pixel[0] = blue;
                pixel[1] = green;
                pixel[2] = red;
            }
        }
    }
    return {};
}

status vqec_vision_ai_core_color_quantize_rgb8_to_uint16(
    const std::uint8_t* _rgb, std::uint32_t _width, std::uint32_t _height,
    std::uint32_t _rgb_stride,
    const std::array<float, 3>& _offset, const std::array<float, 3>& _scale,
    float _quant_scale, std::int32_t _quant_zero_point,
    std::uint16_t* _output) noexcept {
    if (_rgb == nullptr || _output == nullptr || _width == 0 || _height == 0 ||
        _rgb_stride < _width * 3U || !(_quant_scale > 0.0F) || !std::isfinite(_quant_scale)) {
        return {status_code::invalid_argument, "invalid RGB to uint16 quantization arguments"};
    }
    for (std::size_t channel = 0; channel < 3; ++channel) {
        if (!std::isfinite(_offset[channel]) || !std::isfinite(_scale[channel])) {
            return {status_code::invalid_argument, "non-finite normalization coefficients"};
        }
    }
    // RGB8 has a finite domain: preserve the exact affine rounding/clipping formula
    // once per channel/value instead of evaluating it for every destination pixel.
    constexpr std::size_t g_rgb_channel_count = 3;
    std::array<std::array<std::uint16_t, g_rgb8_value_count>, g_rgb_channel_count> mapping{};
    for (std::size_t channel = 0; channel < mapping.size(); ++channel) {
        for (std::size_t value = 0; value < mapping[channel].size(); ++value) {
            const double real = (static_cast<double>(value) - _offset[channel]) * _scale[channel];
            const double stored = std::round(real / static_cast<double>(_quant_scale)) +
                static_cast<double>(_quant_zero_point);
            mapping[channel][value] = static_cast<std::uint16_t>(std::clamp(stored, 0.0, 65535.0));
        }
    }
    for (std::uint32_t row = 0; row < _height; ++row) {
        const std::uint8_t* input_row = _rgb + static_cast<std::size_t>(row) * _rgb_stride;
        std::uint16_t* output_row =
            _output + static_cast<std::size_t>(row) * _width * 3U;
        for (std::uint32_t column = 0; column < _width; ++column) {
            const std::uint8_t* pixel = input_row + static_cast<std::size_t>(column) * 3U;
            std::uint16_t* out = output_row + static_cast<std::size_t>(column) * 3U;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                out[channel] = mapping[channel][pixel[channel]];
            }
        }
    }
    return {};
}

status vqec_vision_ai_core_color_validate_direct_integer_mapping(
    const preprocess_spec& _preprocess, const tensor_spec& _target) noexcept {
    if (_preprocess.normalization_ != normalization_formula::offset_scale ||
        (_target.dtype_ != tensor_element_type::uint8 &&
            _target.dtype_ != tensor_element_type::uint16) ||
        !_target.quantization_.is_quantized_ ||
        !std::isfinite(_target.quantization_.scale_) ||
        !(_target.quantization_.scale_ > 0.0F)) {
        return {status_code::unsupported,
            "direct integer mapping requires quantized uint8/uint16 offset_scale input"};
    }
    constexpr std::uint32_t g_rgb8_max = std::numeric_limits<std::uint8_t>::max();
    constexpr std::uint32_t g_uint16_widening =
        std::numeric_limits<std::uint16_t>::max() / g_rgb8_max;
    constexpr std::int64_t g_max_quantized_mapping_error = 1;
    const std::uint32_t widening = _target.dtype_ == tensor_element_type::uint16 ?
        g_uint16_widening : 1U;
    const double output_max = _target.dtype_ == tensor_element_type::uint16 ?
        static_cast<double>(std::numeric_limits<std::uint16_t>::max()) :
        static_cast<double>(std::numeric_limits<std::uint8_t>::max());
    for (std::size_t channel = 0; channel < _preprocess.offset_.size(); ++channel) {
        if (!std::isfinite(_preprocess.offset_[channel]) ||
            !std::isfinite(_preprocess.scale_[channel])) {
            return {status_code::invalid_argument,
                "direct integer mapping has non-finite preprocessing coefficients"};
        }
        for (std::uint32_t pixel = 0; pixel <= g_rgb8_max; ++pixel) {
            const double real =
                (static_cast<double>(pixel) - _preprocess.offset_[channel]) *
                _preprocess.scale_[channel];
            const double stored = std::clamp(
                std::round(real / _target.quantization_.scale_) +
                    static_cast<double>(_target.quantization_.zero_point_),
                0.0, output_max);
            const auto quantized = static_cast<std::int64_t>(stored);
            const auto direct = static_cast<std::int64_t>(pixel * widening);
            if (std::abs(quantized - direct) > g_max_quantized_mapping_error) {
                return {status_code::unsupported,
                    "preprocess quantization differs from direct integer mapping"};
            }
        }
    }
    return {};
}

}  // namespace vqec::vision::ai
