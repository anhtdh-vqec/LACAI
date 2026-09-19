#include "vqec_vision_reference_processor.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>

#include <sys/mman.h>

#include "vqec/vision/ai/contracts/inference/vqec_vision_tensor_contract.hpp"

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

// Rec.601/709/2020 YCbCr -> RGB coefficients for full-range luma/chroma (range scaling is
// applied by the caller). R = Y + c1*Cr; G = Y - c3*Cr - c4*Cb; B = Y + c2*Cb.
struct yuv_coefficients {
    float c1_{0.0F};
    float c2_{0.0F};
    float c3_{0.0F};
    float c4_{0.0F};
};

yuv_coefficients vqec_vision_ai_refer_rfprc_coefficients(color_matrix _matrix) noexcept {
    float kr = 0.299F;
    float kb = 0.114F;
    if (_matrix == color_matrix::bt709) {
        kr = 0.2126F;
        kb = 0.0722F;
    } else if (_matrix == color_matrix::bt2020) {
        kr = 0.2627F;
        kb = 0.0593F;
    }
    const float kg = 1.0F - kr - kb;
    yuv_coefficients coefficients;
    coefficients.c1_ = 2.0F * (1.0F - kr);
    coefficients.c2_ = 2.0F * (1.0F - kb);
    coefficients.c3_ = 2.0F * kr * (1.0F - kr) / kg;
    coefficients.c4_ = 2.0F * kb * (1.0F - kb) / kg;
    return coefficients;
}

// Bilinear or nearest (floor) sample of one plane. Neighbour taps are clamped to the plane.
float vqec_vision_ai_refer_rfprc_sample(const std::uint8_t* _plane, int _stride, int _width,
    int _height, float _x, float _y, bool _bilinear) noexcept {
    if (_width <= 0 || _height <= 0) {
        return 0.0F;
    }
    const float clamped_x = std::clamp(_x, 0.0F, static_cast<float>(_width - 1));
    const float clamped_y = std::clamp(_y, 0.0F, static_cast<float>(_height - 1));
    const int x0 = static_cast<int>(clamped_x);
    const int y0 = static_cast<int>(clamped_y);
    if (!_bilinear) {
        return static_cast<float>(_plane[y0 * _stride + x0]);
    }
    const int x1 = x0 + 1 < _width ? x0 + 1 : x0;
    const int y1 = y0 + 1 < _height ? y0 + 1 : y0;
    const float fx = clamped_x - static_cast<float>(x0);
    const float fy = clamped_y - static_cast<float>(y0);
    const float top = static_cast<float>(_plane[y0 * _stride + x0]) * (1.0F - fx) +
        static_cast<float>(_plane[y0 * _stride + x1]) * fx;
    const float bottom = static_cast<float>(_plane[y1 * _stride + x0]) * (1.0F - fx) +
        static_cast<float>(_plane[y1 * _stride + x1]) * fx;
    return top * (1.0F - fy) + bottom * fy;
}

// Sample one interleaved chroma component (0 = U, 1 = V) in chroma-sample coordinates; the
// byte offset is x*2 because NV12 stores U and V interleaved.
float vqec_vision_ai_refer_rfprc_sample_chroma(const std::uint8_t* _plane, int _stride,
    int _width, int _height, float _x, float _y, bool _bilinear, int _component) noexcept {
    if (_width <= 0 || _height <= 0) {
        return 0.0F;
    }
    const float clamped_x = std::clamp(_x, 0.0F, static_cast<float>(_width - 1));
    const float clamped_y = std::clamp(_y, 0.0F, static_cast<float>(_height - 1));
    const int x0 = static_cast<int>(clamped_x);
    const int y0 = static_cast<int>(clamped_y);
    const auto at = [&](int _x_index, int _y_index) {
        return static_cast<float>(_plane[_y_index * _stride + _x_index * 2 + _component]);
    };
    if (!_bilinear) {
        return at(x0, y0);
    }
    const int x1 = x0 + 1 < _width ? x0 + 1 : x0;
    const int y1 = y0 + 1 < _height ? y0 + 1 : y0;
    const float fx = clamped_x - static_cast<float>(x0);
    const float fy = clamped_y - static_cast<float>(y0);
    const float top = at(x0, y0) * (1.0F - fx) + at(x1, y0) * fx;
    const float bottom = at(x0, y1) * (1.0F - fx) + at(x1, y1) * fx;
    return top * (1.0F - fy) + bottom * fy;
}

void vqec_vision_ai_refer_rfprc_store(
    std::uint8_t* _destination, tensor_element_type _dtype, float _value) {
    switch (_dtype) {
        case tensor_element_type::float32: {
            const float typed = _value;
            std::memcpy(_destination, &typed, sizeof(typed));
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
    const raw_frame& _frame, const inference_plan& _plan,
    const tensor_spec& _target) const {
    const auto& descriptor = _frame.descriptor_;
    if (descriptor.width_ == 0 || descriptor.height_ == 0 ||
        descriptor.width_ % 2 != 0 || descriptor.height_ % 2 != 0 ||
        descriptor.strides_[0] < static_cast<std::int32_t>(descriptor.width_) ||
        descriptor.strides_[1] < static_cast<std::int32_t>(descriptor.width_) ||
        descriptor.allocation_size_bytes_ == 0) {
        return {status_code::invalid_argument, "invalid RAW frame descriptor for preprocessing"};
    }
    if (_frame.native_handle_ < 0) {
        return {status_code::unsupported, "reference processor requires a memory-mappable FD"};
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
    const raw_frame& _frame, const inference_plan& _plan,
    const tensor_spec& _target, std::vector<tensor_blob>& _outputs) {
    const auto valid = vqec_vision_ai_ports_imgpr_validate(_frame, _plan, _target);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    const auto& descriptor = _frame.descriptor_;
    const std::uint32_t source_width = descriptor.width_;
    const std::uint32_t source_height = descriptor.height_;
    void* base = ::mmap(nullptr, static_cast<std::size_t>(descriptor.allocation_size_bytes_),
                        PROT_READ, MAP_SHARED, static_cast<int>(_frame.native_handle_), 0);
    if (base == MAP_FAILED) {
        return {status_code::io_error, "cannot map RAW frame for preprocessing"};
    }
    const auto* y_plane = static_cast<const std::uint8_t*>(base) + descriptor.offsets_[0];
    const auto* uv_plane = static_cast<const std::uint8_t*>(base) + descriptor.offsets_[1];
    const std::int32_t y_stride = descriptor.strides_[0];
    const std::int32_t uv_stride = descriptor.strides_[1];

    const std::uint32_t out_w = _target.dimensions_[2];
    const std::uint32_t out_h = _target.dimensions_[1];
    const auto element_bytes = vqec_vision_ai_core_tnctr_element_size(_target.dtype_);
    if (out_w == 0 || out_h == 0 || element_bytes == 0) {
        ::munmap(base, static_cast<std::size_t>(descriptor.allocation_size_bytes_));
        return {status_code::invalid_argument, "target tensor geometry is invalid"};
    }
    // Authoritative preprocess contract when present; legacy plan fields otherwise.
    const bool has_spec =
        vqec_vision_ai_core_ppspc_validate(_plan.preprocess_).code_ == status_code::ok;
    if (has_spec && _plan.preprocess_.resize_ == resize_mode::crop) {
        ::munmap(base, static_cast<std::size_t>(descriptor.allocation_size_bytes_));
        return {status_code::unsupported, "crop resize mode is not implemented"};
    }
    if (has_spec && _plan.preprocess_.interpolation_ == interpolation_mode::area) {
        ::munmap(base, static_cast<std::size_t>(descriptor.allocation_size_bytes_));
        return {status_code::unsupported, "area interpolation is not implemented"};
    }
    const image_placement placement = has_spec ? _plan.preprocess_.placement_ : _plan.placement_;
    const channel_order channels = has_spec ? _plan.preprocess_.channels_ : _plan.channel_order_;
    const bool bilinear =
        has_spec && _plan.preprocess_.interpolation_ == interpolation_mode::bilinear;
    const yuv_coefficients coefficients = has_spec ?
        vqec_vision_ai_refer_rfprc_coefficients(_plan.preprocess_.matrix_) :
        vqec_vision_ai_refer_rfprc_coefficients(color_matrix::bt601);
    const bool limited = has_spec ? _plan.preprocess_.range_ == color_range::limited : true;
    const float luma_gain = limited ? 255.0F / 219.0F : 1.0F;
    const float chroma_gain = limited ? 255.0F / 224.0F : 1.0F;
    const float luma_offset = limited ? 16.0F : 0.0F;
    const bool is_rgb = channels == channel_order::rgb;

    const float scale = std::min(
        static_cast<float>(out_w) / static_cast<float>(source_width),
        static_cast<float>(out_h) / static_cast<float>(source_height));
    std::uint32_t pad_left = 0;
    std::uint32_t pad_top = 0;
    if (placement == image_placement::centre) {
        pad_left = static_cast<std::uint32_t>(
            (out_w - static_cast<std::uint32_t>(source_width * scale)) / 2);
        pad_top = static_cast<std::uint32_t>(
            (out_h - static_cast<std::uint32_t>(source_height * scale)) / 2);
    }
    const bool is_stretch = placement == image_placement::stretch;

    tensor_blob candidate;
    candidate.spec_ = _target;
    const auto bytes = vqec_vision_ai_core_tnctr_shape_bytes(_target);
    // Reuse caller-provided storage when it already matches the target, so a steady-state
    // pipeline that keeps one output vector per binding does not allocate per frame.
    const bool can_reuse = _outputs.size() == 1 &&
        _outputs[0].spec_.dtype_ == _target.dtype_ &&
        _outputs[0].spec_.dimensions_ == _target.dimensions_ &&
        _outputs[0].bytes_.size() == static_cast<std::size_t>(bytes);
    if (can_reuse) {
        candidate.bytes_ = std::move(_outputs[0].bytes_);
    } else {
        try {
            candidate.bytes_.assign(static_cast<std::size_t>(bytes), 0U);
        } catch (const std::bad_alloc&) {
            ::munmap(base, static_cast<std::size_t>(descriptor.allocation_size_bytes_));
            return {status_code::resource_exhausted, "reference processor allocation failed"};
        }
    }
    for (std::uint32_t oy = 0; oy < out_h; ++oy) {
        for (std::uint32_t ox = 0; ox < out_w; ++ox) {
            float source_x = -1.0F;
            float source_y = 0.0F;
            if (is_stretch) {
                source_x = (static_cast<float>(ox) + 0.5F) *
                    static_cast<float>(source_width) / static_cast<float>(out_w);
                source_y = (static_cast<float>(oy) + 0.5F) *
                    static_cast<float>(source_height) / static_cast<float>(out_h);
            } else if (ox >= pad_left && oy >= pad_top) {
                source_x = (static_cast<float>(ox - pad_left) + 0.5F) / scale;
                source_y = (static_cast<float>(oy - pad_top) + 0.5F) / scale;
            }
            const bool inside = source_x >= 0.0F && source_x < static_cast<float>(source_width) &&
                source_y >= 0.0F && source_y < static_cast<float>(source_height);
            float output[3] = {0.0F, 0.0F, 0.0F};
            if (inside) {
                const float luminance = vqec_vision_ai_refer_rfprc_sample(
                    y_plane, y_stride, static_cast<int>(source_width),
                    static_cast<int>(source_height), source_x, source_y, bilinear);
                const float u = vqec_vision_ai_refer_rfprc_sample_chroma(uv_plane, uv_stride,
                    static_cast<int>(source_width / 2), static_cast<int>(source_height / 2),
                    source_x / 2.0F, source_y / 2.0F, false, 0);
                const float v = vqec_vision_ai_refer_rfprc_sample_chroma(uv_plane, uv_stride,
                    static_cast<int>(source_width / 2), static_cast<int>(source_height / 2),
                    source_x / 2.0F, source_y / 2.0F, false, 1);
                const float luma = (luminance - luma_offset) * luma_gain;
                const float cr = (v - 128.0F) * chroma_gain;
                const float cb = (u - 128.0F) * chroma_gain;
                const float red = luma + coefficients.c1_ * cr;
                const float green = luma - coefficients.c3_ * cr - coefficients.c4_ * cb;
                const float blue = luma + coefficients.c2_ * cb;
                output[0] = is_rgb ? red : blue;
                output[1] = green;
                output[2] = is_rgb ? blue : red;
            } else if (has_spec) {
                output[0] = _plan.preprocess_.pad_value_[0];
                output[1] = _plan.preprocess_.pad_value_[1];
                output[2] = _plan.preprocess_.pad_value_[2];
            }
            const std::size_t pixel_base =
                (static_cast<std::size_t>(oy) * out_w + ox) * 3 * element_bytes;
            for (std::size_t channel_index = 0; channel_index < 3; ++channel_index) {
                const float clamped = vqec_vision_ai_refer_rfprc_clamp_byte(output[channel_index]);
                float normalized = 0.0F;
                if (has_spec) {
                    switch (_plan.preprocess_.normalization_) {
                        case normalization_formula::mean_std:
                            normalized = (clamped - _plan.preprocess_.offset_[channel_index]) /
                                _plan.preprocess_.scale_[channel_index];
                            break;
                        case normalization_formula::none:
                            normalized = clamped;
                            break;
                        default:
                            normalized = (clamped - _plan.preprocess_.offset_[channel_index]) *
                                _plan.preprocess_.scale_[channel_index];
                            break;
                    }
                } else {
                    normalized = (clamped - static_cast<float>(_plan.mean_[channel_index])) *
                        static_cast<float>(_plan.sigma_[channel_index]);
                }
                const float stored = vqec_vision_ai_refer_rfprc_quantize(normalized, _target);
                vqec_vision_ai_refer_rfprc_store(
                    candidate.bytes_.data() + pixel_base + channel_index * element_bytes,
                    _target.dtype_, stored);
            }
        }
    }
    ::munmap(base, static_cast<std::size_t>(descriptor.allocation_size_bytes_));
    _outputs.clear();
    _outputs.push_back(std::move(candidate));
    return {};
}

}  // namespace vqec::vision::ai
