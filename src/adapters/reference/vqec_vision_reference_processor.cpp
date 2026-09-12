#include "vqec_vision_reference_processor.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_refer_rfprc_is_nhwc_rgb(const tensor_spec& _target) noexcept {
    return _target.dimensions_.size() == 4 && _target.dimensions_[0] == 1 &&
        _target.dimensions_[3] == 3;
}

std::uint8_t vqec_vision_ai_refer_rfprc_clamp_byte(float _value) noexcept {
    return static_cast<std::uint8_t>(std::clamp(_value, 0.0F, 255.0F));
}

float vqec_vision_ai_refer_rfprc_quantize(float _real, const tensor_spec& _target) {
    if (!_target.quantization_.is_quantized_) {
        return _real;
    }
    return _real / _target.quantization_.scale_ +
        static_cast<float>(_target.quantization_.zero_point_);
}

void vqec_vision_ai_refer_rfprc_store(
    std::uint8_t* _destination, tensor_element_type _dtype, float _value) {
    switch (_dtype) {
        case tensor_element_type::float32: {
            const float typed = _value;
            std::memcpy(_destination, &typed, sizeof(typed));
            break;
        }
        case tensor_element_type::float16: {
            // Reference slice does not implement a half conversion; callers should use a
            // dtype-specific processor. Store zero to keep the buffer well-defined.
            const std::uint16_t zero = 0;
            std::memcpy(_destination, &zero, sizeof(zero));
            break;
        }
        case tensor_element_type::uint8: {
            const auto typed = static_cast<std::uint8_t>(std::clamp(_value, 0.0F, 255.0F));
            std::memcpy(_destination, &typed, sizeof(typed));
            break;
        }
        case tensor_element_type::int8: {
            const auto typed = static_cast<std::int8_t>(std::clamp(_value, -128.0F, 127.0F));
            std::memcpy(_destination, &typed, sizeof(typed));
            break;
        }
        case tensor_element_type::uint16: {
            const auto typed = static_cast<std::uint16_t>(std::clamp(_value, 0.0F, 65535.0F));
            std::memcpy(_destination, &typed, sizeof(typed));
            break;
        }
        case tensor_element_type::int16: {
            const auto typed = static_cast<std::int16_t>(std::clamp(_value, -32768.0F, 32767.0F));
            std::memcpy(_destination, &typed, sizeof(typed));
            break;
        }
        default:
            break;
    }
}

}  // namespace

status reference_image_processor::vqec_vision_ai_ports_imgpr_validate(
    const nv12_frame_view& _view, const inference_plan& _plan,
    const tensor_spec& _target) const {
    if (_view.y_data_ == nullptr || _view.uv_data_ == nullptr || _view.width_ == 0 ||
        _view.height_ == 0 || _view.width_ % 2 != 0 || _view.height_ % 2 != 0 ||
        _view.y_stride_ < static_cast<std::int32_t>(_view.width_) ||
        _view.uv_stride_ < static_cast<std::int32_t>(_view.width_)) {
        return {status_code::invalid_argument, "invalid NV12 frame view"};
    }
    const auto valid_plan = vqec_vision_ai_core_infpl_validate_plan(_plan);
    if (valid_plan.code_ != status_code::ok) {
        return valid_plan;
    }
    if (!vqec_vision_ai_refer_rfprc_is_nhwc_rgb(_target) ||
        vqec_vision_ai_core_tnctr_element_size(_target.dtype_) == 0) {
        return {status_code::unsupported,
            "reference processor requires a rank-4 NHWC RGB target tensor"};
    }
    return {};
}

status reference_image_processor::vqec_vision_ai_ports_imgpr_preprocess(
    const nv12_frame_view& _view, const inference_plan& _plan,
    const tensor_spec& _target, std::vector<tensor_blob>& _outputs) {
    const auto valid = vqec_vision_ai_ports_imgpr_validate(_view, _plan, _target);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    const std::uint32_t out_w = _target.dimensions_[2];
    const std::uint32_t out_h = _target.dimensions_[1];
    const auto element_bytes = vqec_vision_ai_core_tnctr_element_size(_target.dtype_);
    if (out_w == 0 || out_h == 0 || element_bytes == 0) {
        return {status_code::invalid_argument, "target tensor geometry is invalid"};
    }

    const float scale = std::min(
        static_cast<float>(out_w) / static_cast<float>(_view.width_),
        static_cast<float>(out_h) / static_cast<float>(_view.height_));
    std::uint32_t pad_left = 0;
    std::uint32_t pad_top = 0;
    if (_plan.placement_ == image_placement::centre) {
        pad_left = static_cast<std::uint32_t>(
            (out_w - static_cast<std::uint32_t>(_view.width_ * scale)) / 2);
        pad_top = static_cast<std::uint32_t>(
            (out_h - static_cast<std::uint32_t>(_view.height_ * scale)) / 2);
    }
    const bool is_stretch = _plan.placement_ == image_placement::stretch;

    tensor_blob candidate;
    candidate.spec_ = _target;
    const auto bytes = vqec_vision_ai_core_tnctr_shape_bytes(_target);
    try {
        candidate.bytes_.assign(static_cast<std::size_t>(bytes), 0U);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "reference processor allocation failed"};
    }

    const bool is_rgb = _plan.channel_order_ == channel_order::rgb;
    for (std::uint32_t oy = 0; oy < out_h; ++oy) {
        for (std::uint32_t ox = 0; ox < out_w; ++ox) {
            float red = 0.0F;
            float green = 0.0F;
            float blue = 0.0F;
            float source_x = 0.0F;
            float source_y = 0.0F;
            if (is_stretch) {
                source_x = (static_cast<float>(ox) + 0.5F) *
                    static_cast<float>(_view.width_) / static_cast<float>(out_w);
                source_y = (static_cast<float>(oy) + 0.5F) *
                    static_cast<float>(_view.height_) / static_cast<float>(out_h);
            } else if (ox >= pad_left && oy >= pad_top) {
                source_x = (static_cast<float>(ox - pad_left) + 0.5F) / scale;
                source_y = (static_cast<float>(oy - pad_top) + 0.5F) / scale;
            } else {
                source_x = -1.0F;  // padding
            }
            if (source_x >= 0.0F && source_x < static_cast<float>(_view.width_) &&
                source_y >= 0.0F && source_y < static_cast<float>(_view.height_)) {
                const auto sx = static_cast<std::int32_t>(source_x);
                const auto sy = static_cast<std::int32_t>(source_y);
                const auto luminance = static_cast<float>(
                    _view.y_data_[sy * _view.y_stride_ + sx]);
                const auto chroma_index = (sy / 2) * _view.uv_stride_ + (sx / 2) * 2;
                const auto u = static_cast<float>(_view.uv_data_[chroma_index]) - 128.0F;
                const auto v = static_cast<float>(_view.uv_data_[chroma_index + 1]) - 128.0F;
                const auto luma = 1.164F * (luminance - 16.0F);
                red = luma + 1.596F * v;
                green = luma - 0.391F * u - 0.813F * v;
                blue = luma + 2.018F * u;
            }
            const float channel[3] = {
                is_rgb ? red : blue, green, is_rgb ? blue : red};
            const std::size_t base =
                (static_cast<std::size_t>(oy) * out_w + ox) * 3 * element_bytes;
            for (std::size_t channel_index = 0; channel_index < 3; ++channel_index) {
                const float normalized =
                    (vqec_vision_ai_refer_rfprc_clamp_byte(channel[channel_index]) -
                        static_cast<float>(_plan.mean_[channel_index])) *
                    static_cast<float>(_plan.sigma_[channel_index]);
                const float stored = vqec_vision_ai_refer_rfprc_quantize(normalized, _target);
                vqec_vision_ai_refer_rfprc_store(
                    candidate.bytes_.data() + base + channel_index * element_bytes,
                    _target.dtype_, stored);
            }
        }
    }
    std::vector<tensor_blob> result;
    result.push_back(std::move(candidate));
    _outputs = std::move(result);
    return {};
}

}  // namespace vqec::vision::ai
