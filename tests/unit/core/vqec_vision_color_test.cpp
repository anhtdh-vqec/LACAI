// Device-free tests for the neutral NV12 -> RGB/BGR conversion policy.

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_color.hpp"

using namespace vqec::vision::ai;

namespace {

// One 2x2 frame with a single chroma sample.
struct frame2x2 {
    std::vector<std::uint8_t> y_{16U, 16U, 16U, 16U};
    std::vector<std::uint8_t> uv_{128U, 128U};
};

std::uint8_t channel(const std::vector<std::uint8_t>& _rgb, unsigned _pixel,
    unsigned _component) {
    return _rgb[_pixel * 3U + _component];
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    // BT.601 limited black and white are exact.
    {
        frame2x2 frame;
        std::vector<std::uint8_t> rgb(12U, 0U);
        check(vqec_vision_ai_core_color_convert_nv12_to_rgb(frame.y_.data(), 2,
                  frame.uv_.data(), 2, 2, 2, color_matrix::bt601, color_range::limited,
                  channel_order::rgb, rgb.data(), 6).code_ == status_code::ok);
        check(rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 0);
        frame.y_ = {235U, 235U, 235U, 235U};
        check(vqec_vision_ai_core_color_convert_nv12_to_rgb(frame.y_.data(), 2,
                  frame.uv_.data(), 2, 2, 2, color_matrix::bt601, color_range::limited,
                  channel_order::rgb, rgb.data(), 6).code_ == status_code::ok);
        check(rgb[0] == 255 && rgb[1] == 255 && rgb[2] == 255);
    }

    // Matrix and channel order are honored for a strong red sample.
    {
        frame2x2 frame;
        frame.y_ = {81U, 81U, 81U, 81U};
        frame.uv_ = {90U, 240U};
        std::vector<std::uint8_t> rgb(12U, 0U);
        check(vqec_vision_ai_core_color_convert_nv12_to_rgb(frame.y_.data(), 2,
                  frame.uv_.data(), 2, 2, 2, color_matrix::bt601, color_range::limited,
                  channel_order::rgb, rgb.data(), 6).code_ == status_code::ok);
        check(channel(rgb, 0U, 0U) > 200U && channel(rgb, 0U, 1U) < 20U &&
            channel(rgb, 0U, 2U) < 20U);
        std::vector<std::uint8_t> bgr(12U, 0U);
        check(vqec_vision_ai_core_color_convert_nv12_to_rgb(frame.y_.data(), 2,
                  frame.uv_.data(), 2, 2, 2, color_matrix::bt601, color_range::limited,
                  channel_order::bgr, bgr.data(), 6).code_ == status_code::ok);
        check(channel(bgr, 0U, 0U) < 20U && channel(bgr, 0U, 2U) > 200U);
        std::vector<std::uint8_t> bt709(12U, 0U);
        check(vqec_vision_ai_core_color_convert_nv12_to_rgb(frame.y_.data(), 2,
                  frame.uv_.data(), 2, 2, 2, color_matrix::bt709, color_range::limited,
                  channel_order::rgb, bt709.data(), 6).code_ == status_code::ok);
        check(channel(bt709, 0U, 1U) != channel(rgb, 0U, 1U));
    }

    // The hot-path lookup implementation remains byte-exact with the normative floating
    // formula at black/range boundaries and representative interior chroma values.
    {
        const std::array<std::uint8_t, 6> samples{0U, 16U, 81U, 128U, 235U, 255U};
        for (const auto matrix : {color_matrix::bt601, color_matrix::bt709}) {
            for (const auto range : {color_range::limited, color_range::full}) {
                const bool limited = range == color_range::limited;
                const double ys = limited ? 1.1643835616438356 : 1.0;
                const double yo = limited ? 16.0 : 0.0;
                const double rv = matrix == color_matrix::bt601 ?
                    (limited ? 1.5960267857142858 : 1.402) :
                    (limited ? 1.7927410714285714 : 1.5748);
                const double gu = matrix == color_matrix::bt601 ?
                    (limited ? 0.3917622900949137 : 0.34413628620102216) :
                    (limited ? 0.21324861427372963 : 0.1873242729306488);
                const double gv = matrix == color_matrix::bt601 ?
                    (limited ? 0.8129676472377708 : 0.7141362862010221) :
                    (limited ? 0.532909328559444 : 0.4681242729306488);
                const double bu = matrix == color_matrix::bt601 ?
                    (limited ? 2.017232142857143 : 1.772) :
                    (limited ? 2.112401785714286 : 1.8556);
                for (const auto y : samples) {
                    for (const auto u : samples) {
                        for (const auto v : samples) {
                            frame2x2 frame;
                            frame.y_.assign(frame.y_.size(), y);
                            frame.uv_ = {u, v};
                            std::vector<std::uint8_t> rgb(12U, 0U);
                            check(vqec_vision_ai_core_color_convert_nv12_to_rgb(
                                      frame.y_.data(), 2, frame.uv_.data(), 2, 2, 2,
                                      matrix, range, channel_order::rgb, rgb.data(), 6)
                                      .code_ == status_code::ok);
                            const double luma = ys * (static_cast<double>(y) - yo);
                            const double cb = static_cast<double>(u) - 128.0;
                            const double cr = static_cast<double>(v) - 128.0;
                            const auto expected = [](double _value) {
                                return static_cast<std::uint8_t>(std::clamp(
                                    std::round(_value), 0.0, 255.0));
                            };
                            check(channel(rgb, 0U, 0U) == expected(luma + rv * cr));
                            check(channel(rgb, 0U, 1U) == expected(luma - gu * cb - gv * cr));
                            check(channel(rgb, 0U, 2U) == expected(luma + bu * cb));
                        }
                    }
                }
            }
        }
    }

    // Full range keeps luma and rejects unspecified policy or bad geometry.
    {
        frame2x2 frame;
        frame.y_ = {255U, 255U, 255U, 255U};
        std::vector<std::uint8_t> rgb(12U, 0U);
        check(vqec_vision_ai_core_color_convert_nv12_to_rgb(frame.y_.data(), 2,
                  frame.uv_.data(), 2, 2, 2, color_matrix::bt709, color_range::full,
                  channel_order::rgb, rgb.data(), 6).code_ == status_code::ok);
        check(rgb[0] == 255 && rgb[1] == 255 && rgb[2] == 255);
        check(vqec_vision_ai_core_color_convert_nv12_to_rgb(frame.y_.data(), 2,
                  frame.uv_.data(), 2, 2, 2, color_matrix::unspecified, color_range::limited,
                  channel_order::rgb, rgb.data(), 6).code_ == status_code::unsupported);
        check(vqec_vision_ai_core_color_convert_nv12_to_rgb(frame.y_.data(), 2,
                  frame.uv_.data(), 2, 3, 2, color_matrix::bt601, color_range::limited,
                  channel_order::rgb, rgb.data(), 6).code_ == status_code::invalid_argument);
        check(vqec_vision_ai_core_color_convert_nv12_to_rgb(nullptr, 2, frame.uv_.data(), 2,
                  2, 2, color_matrix::bt601, color_range::limited, channel_order::rgb,
                  rgb.data(), 6).code_ == status_code::invalid_argument);
    }

    // RGB uint8 to the EdgeFace uint16 quantized input tensor (BT.709 limited normalization).
    {
        const std::array<float, 3> offset = {127.5F, 127.5F, 127.5F};
        const std::array<float, 3> scale = {
            1.0F / 127.5F, 1.0F / 127.5F, 1.0F / 127.5F};
        constexpr float quant_scale = 3.05180438e-05F;
        constexpr std::int32_t quant_zero_point = 32768;
        std::vector<std::uint8_t> rgb = {0U, 0U, 0U, 255U, 255U, 255U, 128U, 128U, 128U};
        std::vector<std::uint16_t> quantized(9U, 0U);
        check(vqec_vision_ai_core_color_quantize_rgb8_to_uint16(rgb.data(), 3U, 1U, 9U,
                  offset, scale, quant_scale, quant_zero_point, quantized.data()).code_ ==
            status_code::ok);
        check(quantized[0] == 0U && quantized[3] == 65535U &&
            quantized[6] == 32897U);
        check(vqec_vision_ai_core_color_quantize_rgb8_to_uint16(rgb.data(), 3U, 1U, 9U,
                  offset, scale, 0.0F, quant_zero_point, quantized.data()).code_ ==
            status_code::invalid_argument);
    }

    // Both face models reduce to the converter's exact uint8-to-uint16 widening despite
    // their nonzero tensor zero point. A changed scale fails closed.
    {
        preprocess_spec preprocess;
        preprocess.normalization_ = normalization_formula::offset_scale;
        preprocess.offset_ = {127.5F, 127.5F, 127.5F};
        preprocess.scale_ = {0.0078125F, 0.0078125F, 0.0078125F};
        tensor_spec target;
        target.dtype_ = tensor_element_type::uint16;
        target.quantization_ = {true, 3.0398832677747123e-05F, 32768};
        check(vqec_vision_ai_core_color_validate_direct_integer_mapping(preprocess, target)
                  .code_ == status_code::ok);
        preprocess.scale_ = {0.01F, 0.01F, 0.01F};
        check(vqec_vision_ai_core_color_validate_direct_integer_mapping(preprocess, target)
                  .code_ == status_code::unsupported);
    }

    std::cout << "color conversion failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
