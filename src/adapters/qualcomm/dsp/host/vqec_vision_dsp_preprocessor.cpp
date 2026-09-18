#include "vqec_vision_dsp_preprocessor.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <vqec_vision_dsp_legacy_types.h>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {

namespace {

constexpr std::size_t g_geom_size = VQEC_GEOM_INTS;
constexpr std::uint32_t g_rgb_channels = 3;

}  // namespace

struct dsp_preprocessor::implementation {
    dsp_preprocessor_config config_;
    std::shared_ptr<dsp_buffer_cache> buffer_cache_;
    tensor_blob scratch_;

    explicit implementation(dsp_preprocessor_config _config)
        : config_(std::move(_config)),
          buffer_cache_(config_.buffer_cache_ != nullptr ?
              config_.buffer_cache_ : std::make_shared<dsp_buffer_cache>()) {}
};

dsp_preprocessor::dsp_preprocessor(dsp_preprocessor_config _config)
    : implementation_(std::make_unique<implementation>(std::move(_config))) {}

dsp_preprocessor::~dsp_preprocessor() noexcept = default;

dsp_preprocessor::dsp_preprocessor(dsp_preprocessor&&) noexcept = default;
dsp_preprocessor& dsp_preprocessor::operator=(dsp_preprocessor&&) noexcept = default;

std::array<std::int32_t, 12> dsp_preprocessor::vqec_vision_ai_qcom_dsppr_compute_geom(
    dsp_preprocessor_kind _kind,
    std::uint32_t _src_w, std::uint32_t _src_h,
    std::int32_t _y_stride, std::uint32_t _uv_offset, std::int32_t _uv_stride,
    std::uint32_t _tensor_side) {
    std::array<std::int32_t, 12> geom{};
    if (_src_w == 0 || _src_h == 0 || _src_w > VQEC_MAX_FRAME_SIDE ||
        _src_h > VQEC_MAX_FRAME_SIDE || _tensor_side < inference_limits::g_min_tensor_dimension ||
        _tensor_side > inference_limits::g_max_tensor_dimension ||
        _uv_offset > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        (_kind != dsp_preprocessor_kind::yolov8 && _kind != dsp_preprocessor_kind::scrfd)) {
        return geom;
    }

    const int src_w = static_cast<int>(_src_w);
    const int src_h = static_cast<int>(_src_h);
    const int side = static_cast<int>(_tensor_side);
    const int y_stride = _y_stride > 0 ? _y_stride : src_w;
    const int uv_offset = _uv_offset > 0 ? static_cast<int>(_uv_offset) : (y_stride * src_h);
    const int uv_stride = _uv_stride > 0 ? _uv_stride : src_w;

    const float scale = std::min(
        static_cast<float>(side) / static_cast<float>(src_w),
        static_cast<float>(side) / static_cast<float>(src_h));

    int new_w = 0;
    int new_h = 0;
    int dst_x = 0;
    int dst_y = 0;
    int pad = 0;

    if (_kind == dsp_preprocessor_kind::yolov8) {
        new_w = static_cast<int>(std::lround(static_cast<float>(src_w) * scale));
        new_h = static_cast<int>(std::lround(static_cast<float>(src_h) * scale));
        new_w &= ~1;
        new_h &= ~1;
        dst_x = static_cast<int>(std::lround(static_cast<float>(side - new_w) / 2.0F - 0.1F));
        dst_y = static_cast<int>(std::lround(static_cast<float>(side - new_h) / 2.0F - 0.1F));
        pad = 114;
    } else {
        new_w = static_cast<int>(static_cast<float>(src_w) * scale);
        new_h = static_cast<int>(static_cast<float>(src_h) * scale);
        new_w &= ~1;
        new_h &= ~1;
        dst_x = 0;
        dst_y = 0;
        pad = 0;
    }

    geom[0] = src_w;
    geom[1] = src_h;
    geom[2] = y_stride;
    geom[3] = uv_offset;
    geom[4] = uv_stride;
    geom[5] = side;
    geom[6] = side;
    geom[7] = dst_x;
    geom[8] = dst_y;
    geom[9] = new_w;
    geom[10] = new_h;
    geom[11] = pad;
    return geom;
}

status dsp_preprocessor::vqec_vision_ai_ports_imgpr_validate(
    const raw_frame& _frame, const inference_plan& _plan,
    const tensor_spec& _target) const {
    if (implementation_ == nullptr) {
        return {status_code::unsupported, "dsp_preprocessor implementation is null"};
    }
    const auto& descriptor = _frame.descriptor_;
    if (descriptor.width_ == 0 || descriptor.height_ == 0 ||
        descriptor.width_ > VQEC_MAX_FRAME_SIDE ||
        descriptor.height_ > VQEC_MAX_FRAME_SIDE ||
        descriptor.width_ % 2U != 0 || descriptor.height_ % 2U != 0) {
        return {status_code::invalid_argument,
            "dsp_preprocessor requires non-zero frame dimensions"};
    }
    if (_frame.native_handle_ < 0 || _frame.native_handle_ > std::numeric_limits<int>::max() ||
        !_frame.owner_ || descriptor.allocation_size_bytes_ == 0 ||
        descriptor.view_size_bytes_ == 0 ||
        descriptor.view_size_bytes_ > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
        descriptor.memory_offset_bytes_ > descriptor.allocation_size_bytes_ ||
        descriptor.view_size_bytes_ > descriptor.allocation_size_bytes_ - descriptor.memory_offset_bytes_) {
        return {status_code::invalid_argument,
            "dsp_preprocessor requires a valid dma-buf native handle"};
    }
    if (_target.dtype_ != tensor_element_type::uint16) {
        return {status_code::invalid_argument,
            "dsp_preprocessor target must be quantized uint16"};
    }
    if (_target.layout_ != tensor_layout::nhwc) {
        return {status_code::invalid_argument,
            "dsp_preprocessor target layout must be NHWC"};
    }
    if (_target.dimensions_.size() != 4 || _target.dimensions_[0] != 1 ||
        _target.dimensions_[3] != g_rgb_channels) {
        return {status_code::invalid_argument,
            "dsp_preprocessor target dimensions must be 1xHxWx3"};
    }
    if (_target.dimensions_[1] != _target.dimensions_[2] ||
        _target.dimensions_[1] < inference_limits::g_min_tensor_dimension ||
        _target.dimensions_[1] > inference_limits::g_max_tensor_dimension) {
        return {status_code::invalid_argument,
            "dsp_preprocessor target must be a square tensor"};
    }
    if (implementation_->config_.kind_ != dsp_preprocessor_kind::yolov8 &&
        implementation_->config_.kind_ != dsp_preprocessor_kind::scrfd) {
        return {status_code::unsupported, "unknown DSP preprocessing operation"};
    }
    for (std::size_t plane = 0; plane < descriptor.strides_.size(); ++plane) {
        const auto rows = plane == 0 ? descriptor.height_ : descriptor.height_ / 2U;
        if (descriptor.strides_[plane] < static_cast<std::int32_t>(descriptor.width_) ||
            descriptor.offsets_[plane] > descriptor.view_size_bytes_ ||
            rows > (descriptor.view_size_bytes_ - descriptor.offsets_[plane]) /
                static_cast<std::uint64_t>(descriptor.strides_[plane])) {
            return {status_code::invalid_argument, "DSP source plane exceeds memory view"};
        }
    }
    const auto luma_end = descriptor.offsets_[0] +
        static_cast<std::uint64_t>(descriptor.strides_[0]) * descriptor.height_;
    if (descriptor.offsets_[1] < luma_end) {
        return {status_code::invalid_argument, "DSP source planes overlap"};
    }
    if (_plan.source_width_ != descriptor.width_ || _plan.source_height_ != descriptor.height_ ||
        _plan.tensor_width_ != _target.dimensions_[2] ||
        _plan.tensor_height_ != _target.dimensions_[1]) {
        return {status_code::invalid_argument, "DSP source/target does not match admitted plan"};
    }
    if (!_target.quantization_.is_quantized_ || !std::isfinite(_target.quantization_.scale_) ||
        _target.quantization_.scale_ <= 0 ||
        vqec_vision_ai_core_ppspc_validate(_plan.preprocess_).code_ != status_code::ok ||
        _plan.preprocess_.source_format_ != source_pixel_format::nv12 ||
        _plan.preprocess_.channels_ != channel_order::rgb ||
        _plan.preprocess_.resize_ != resize_mode::letterbox ||
        _plan.preprocess_.normalization_ != normalization_formula::offset_scale ||
        _plan.preprocess_.placement_ != (implementation_->config_.kind_ == dsp_preprocessor_kind::yolov8 ?
            image_placement::centre : image_placement::top_left)) {
        return {status_code::unsupported, "preprocess contract exceeds legacy DSP envelope"};
    }
    // Legacy kernels widen RGB bytes by 257. Prove compatibility with the
    // declared affine for all possible byte values, not merely tensor dtype.
    constexpr std::uint32_t g_pixel_max = std::numeric_limits<std::uint8_t>::max();
    constexpr std::uint32_t g_widen_factor = std::numeric_limits<std::uint16_t>::max() / g_pixel_max;
    constexpr double g_quantized_rounding_lsb = 1.0;
    for (std::size_t channel = 0; channel < g_rgb_channels; ++channel) {
        if (_plan.preprocess_.pad_value_[channel] != _plan.preprocess_.pad_value_[0] ||
            std::floor(_plan.preprocess_.pad_value_[channel]) != _plan.preprocess_.pad_value_[channel]) {
            return {status_code::unsupported, "legacy DSP requires one integer RGB pad value"};
        }
        for (std::uint32_t pixel = 0; pixel <= g_pixel_max; ++pixel) {
            const double real = (static_cast<double>(pixel) - _plan.preprocess_.offset_[channel]) *
                _plan.preprocess_.scale_[channel];
            const double quantized = std::clamp(std::round(real / _target.quantization_.scale_ +
                _target.quantization_.zero_point_), 0.0,
                static_cast<double>(std::numeric_limits<std::uint16_t>::max()));
            if (std::abs(quantized - pixel * g_widen_factor) > g_quantized_rounding_lsb) {
                return {status_code::unsupported, "legacy DSP widening does not match input quantization"};
            }
        }
    }
    return {};
}

status dsp_preprocessor::vqec_vision_ai_ports_imgpr_preprocess(
    const raw_frame& _frame, const inference_plan& _plan,
    const tensor_spec& _target, std::vector<tensor_blob>& _outputs) {
    const auto valid = vqec_vision_ai_ports_imgpr_validate(_frame, _plan, _target);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (implementation_->config_.session_ == nullptr) {
        return {status_code::unsupported,
            "dsp_preprocessor requires an initialized dsp_session"};
    }

    status map_status;
    const auto mapping = implementation_->buffer_cache_->vqec_vision_ai_qcom_dspbc_map(
        static_cast<int>(_frame.native_handle_),
        static_cast<std::size_t>(_frame.descriptor_.allocation_size_bytes_),
        map_status);
    if (mapping.data_ == nullptr || map_status.code_ != status_code::ok) {
        return map_status.code_ != status_code::ok ? map_status :
            status{status_code::io_error, "dsp_buffer_cache map returned null"};
    }
    const auto* frame_base = mapping.data_ + _frame.descriptor_.memory_offset_bytes_ +
        _frame.descriptor_.offsets_[0];

    const std::uint32_t tensor_side = _target.dimensions_[1];
    auto geom = vqec_vision_ai_qcom_dsppr_compute_geom(
        implementation_->config_.kind_,
        _frame.descriptor_.width_, _frame.descriptor_.height_,
        _frame.descriptor_.strides_[0],
        _frame.descriptor_.offsets_[1] - _frame.descriptor_.offsets_[0],
        _frame.descriptor_.strides_[1],
        tensor_side);
    geom[VQEC_GEOM_PAD] = static_cast<std::int32_t>(_plan.preprocess_.pad_value_[0]);

    const std::size_t tensor_elements = static_cast<std::size_t>(tensor_side) * tensor_side * g_rgb_channels;
    const std::size_t expected_bytes = tensor_elements * sizeof(std::uint16_t);

    auto& candidate = implementation_->scratch_;
    try {
        if (candidate.bytes_.size() != expected_bytes) {
            candidate.bytes_.resize(expected_bytes);
        }
        candidate.spec_ = _target;
        _outputs.reserve(1U);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "cannot allocate dsp_preprocessor output tensor"};
    }

    auto* tensor_ptr = reinterpret_cast<std::uint16_t*>(candidate.bytes_.data());
    const int tensor_len = static_cast<int>(tensor_elements);
    const int frame_len = static_cast<int>(_frame.descriptor_.view_size_bytes_ -
        _frame.descriptor_.offsets_[0]);

    status prep_status;
    if (implementation_->config_.kind_ == dsp_preprocessor_kind::yolov8) {
        prep_status = implementation_->config_.session_->vqec_vision_ai_qcom_dspsn_preprocess_person_yolov8n(
            frame_base, frame_len, geom.data(), static_cast<int>(g_geom_size),
            tensor_ptr, tensor_len);
    } else {
        prep_status = implementation_->config_.session_->vqec_vision_ai_qcom_dspsn_preprocess_face_scrfd(
            frame_base, frame_len, geom.data(), static_cast<int>(g_geom_size),
            tensor_ptr, tensor_len);
    }

    if (prep_status.code_ != status_code::ok) {
        return prep_status;
    }

    if (_outputs.empty()) {
        _outputs.emplace_back();
    }
    std::swap(_outputs[0], candidate);
    _outputs.resize(1U);
    return {};
}

}  // namespace vqec::vision::ai
