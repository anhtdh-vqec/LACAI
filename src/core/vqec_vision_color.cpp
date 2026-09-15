#include "vqec/vision/ai/contracts/vqec_vision_color.hpp"

#include <algorithm>
#include <cmath>

namespace vqec::vision::ai {
namespace {

std::uint8_t vqec_vision_ai_core_color_clamp(double _value) noexcept {
    const double rounded = std::round(_value);
    return static_cast<std::uint8_t>(std::clamp(rounded, 0.0, 255.0));
}

}  // namespace

status vqec_vision_ai_core_color_convert_nv12_to_rgb(
    const std::uint8_t* _y_plane, std::uint32_t _y_stride,
    const std::uint8_t* _uv_plane, std::uint32_t _uv_stride,
    std::uint32_t _width, std::uint32_t _height,
    color_matrix _matrix, color_range _range, channel_order _order,
    std::uint8_t* _rgb, std::uint32_t _rgb_stride) noexcept {
    if (_y_plane == nullptr || _uv_plane == nullptr || _rgb == nullptr ||
        _width == 0 || _height == 0 || _width % 2 != 0 || _height % 2 != 0 ||
        _y_stride < _width || _uv_stride < _width || _rgb_stride < _width * 3U) {
        return {status_code::invalid_argument, "invalid NV12 to RGB conversion arguments"};
    }
    if ((_matrix != color_matrix::bt601 && _matrix != color_matrix::bt709) ||
        (_range != color_range::limited && _range != color_range::full)) {
        return {status_code::unsupported, "unspecified color matrix or range"};
    }
    const bool limited = _range == color_range::limited;
    // Limited-range luma scale/offset; full range leaves luma as-is.
    const double luma_scale = limited ? 1.1643835616438356 : 1.0;
    const double luma_offset = limited ? 16.0 : 0.0;
    // Chroma coefficients per matrix for the limited-range form; the full-range form uses
    // the standard full-range coefficients.
    double r_v = 0.0;
    double g_u = 0.0;
    double g_v = 0.0;
    double b_u = 0.0;
    if (_matrix == color_matrix::bt601) {
        r_v = limited ? 1.5960267857142858 : 1.402;
        g_u = limited ? 0.3917622900949137 : 0.34413628620102216;
        g_v = limited ? 0.8129676472377708 : 0.7141362862010221;
        b_u = limited ? 2.017232142857143 : 1.772;
    } else {
        r_v = limited ? 1.7927410714285714 : 1.5748;
        g_u = limited ? 0.21324861427372963 : 0.1873242729306488;
        g_v = limited ? 0.532909328559444 : 0.4681242729306488;
        b_u = limited ? 2.112401785714286 : 1.8556;
    }
    for (std::uint32_t row = 0; row < _height; ++row) {
        const std::uint8_t* y_row = _y_plane + static_cast<std::size_t>(row) * _y_stride;
        const std::uint8_t* uv_row =
            _uv_plane + static_cast<std::size_t>(row / 2U) * _uv_stride;
        std::uint8_t* out_row = _rgb + static_cast<std::size_t>(row) * _rgb_stride;
        for (std::uint32_t column = 0; column < _width; ++column) {
            const double luma = luma_scale * (static_cast<double>(y_row[column]) - luma_offset);
            const std::size_t chroma_index =
                static_cast<std::size_t>(column / 2U) * 2U;
            const double cb = static_cast<double>(uv_row[chroma_index]) - 128.0;
            const double cr = static_cast<double>(uv_row[chroma_index + 1U]) - 128.0;
            const std::uint8_t red = vqec_vision_ai_core_color_clamp(luma + r_v * cr);
            const std::uint8_t green =
                vqec_vision_ai_core_color_clamp(luma - g_u * cb - g_v * cr);
            const std::uint8_t blue = vqec_vision_ai_core_color_clamp(luma + b_u * cb);
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
    for (std::uint32_t row = 0; row < _height; ++row) {
        const std::uint8_t* input_row = _rgb + static_cast<std::size_t>(row) * _rgb_stride;
        std::uint16_t* output_row =
            _output + static_cast<std::size_t>(row) * _width * 3U;
        for (std::uint32_t column = 0; column < _width; ++column) {
            const std::uint8_t* pixel = input_row + static_cast<std::size_t>(column) * 3U;
            std::uint16_t* out = output_row + static_cast<std::size_t>(column) * 3U;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const double real =
                    static_cast<double>(pixel[channel]) * _scale[channel] + _offset[channel];
                const double stored =
                    std::round(real / static_cast<double>(_quant_scale)) +
                    static_cast<double>(_quant_zero_point);
                out[channel] = static_cast<std::uint16_t>(std::clamp(stored, 0.0, 65535.0));
            }
        }
    }
    return {};
}

}  // namespace vqec::vision::ai
