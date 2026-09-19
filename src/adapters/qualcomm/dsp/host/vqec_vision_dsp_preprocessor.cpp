#include "vqec_vision_dsp_preprocessor.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_color.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"
#include "vqec_vision_dsp_v1_image.h"
#include "vqec_vision_rpcmem_pool.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::uint32_t g_tensor_batch = 1U;
constexpr std::uint32_t g_tensor_channels = 3U;

bool vqec_vision_ai_qcom_dsppr_has_image_operation(
    const dsp_v1_client& _client) noexcept {
    const auto capabilities = _client.vqec_vision_ai_qcom_d1cli_capabilities();
    return (capabilities.operations_mask &
            (1U << (VQEC_VISION_AI_DSP_V1_IMAGE_TRANSFORM - 1U))) != 0U;
}

status vqec_vision_ai_qcom_dsppr_make_config(
    const raw_frame& _frame, const inference_plan& _plan, const tensor_spec& _target,
    vqec_vision_ai_dsp_v1_image_config& _config) noexcept {
    const auto& source = _frame.descriptor_;
    const float resize_scale = std::min(
        static_cast<float>(_plan.tensor_width_) / static_cast<float>(_plan.source_width_),
        static_cast<float>(_plan.tensor_height_) / static_cast<float>(_plan.source_height_));
    const auto destination_width = static_cast<std::uint32_t>(
        static_cast<float>(_plan.source_width_) * resize_scale);
    const auto destination_height = static_cast<std::uint32_t>(
        static_cast<float>(_plan.source_height_) * resize_scale);
    if (destination_width == 0U || destination_height == 0U ||
        (destination_width & 1U) != 0U || (destination_height & 1U) != 0U) {
        return {status_code::unsupported,
            "DSP v1 image transform requires even letterbox geometry"};
    }
    _config = {};
    _config.pixel_format = VQEC_VISION_AI_DSP_V1_IMAGE_PIXEL_NV12;
    _config.matrix = _plan.preprocess_.matrix_ == color_matrix::bt709 ?
        VQEC_VISION_AI_DSP_V1_IMAGE_MATRIX_BT709 :
        VQEC_VISION_AI_DSP_V1_IMAGE_MATRIX_BT601;
    _config.range = _plan.preprocess_.range_ == color_range::limited ?
        VQEC_VISION_AI_DSP_V1_IMAGE_RANGE_LIMITED :
        VQEC_VISION_AI_DSP_V1_IMAGE_RANGE_FULL;
    _config.interpolation = VQEC_VISION_AI_DSP_V1_IMAGE_INTERPOLATION_BILINEAR;
    _config.resize = VQEC_VISION_AI_DSP_V1_IMAGE_RESIZE_LETTERBOX;
    _config.placement = _plan.preprocess_.placement_ == image_placement::centre ?
        VQEC_VISION_AI_DSP_V1_IMAGE_PLACEMENT_CENTRE :
        VQEC_VISION_AI_DSP_V1_IMAGE_PLACEMENT_TOP_LEFT;
    _config.channel_order = _plan.preprocess_.channels_ == channel_order::rgb ?
        VQEC_VISION_AI_DSP_V1_IMAGE_CHANNEL_RGB :
        VQEC_VISION_AI_DSP_V1_IMAGE_CHANNEL_BGR;
    _config.normalization = VQEC_VISION_AI_DSP_V1_IMAGE_NORMALIZATION_OFFSET_SCALE;
    _config.dtype = VQEC_VISION_AI_DSP_V1_IMAGE_DTYPE_UINT16;
    _config.source_width = source.width_;
    _config.source_height = source.height_;
    _config.source_y_offset = source.offsets_[0];
    _config.source_y_stride = static_cast<std::uint32_t>(source.strides_[0]);
    _config.source_uv_offset = source.offsets_[1];
    _config.source_uv_stride = static_cast<std::uint32_t>(source.strides_[1]);
    _config.crop_width = source.width_;
    _config.crop_height = source.height_;
    _config.tensor_width = _plan.tensor_width_;
    _config.tensor_height = _plan.tensor_height_;
    _config.destination_width = destination_width;
    _config.destination_height = destination_height;
    if (_plan.preprocess_.placement_ == image_placement::centre) {
        _config.destination_x = (_plan.tensor_width_ - destination_width) / 2U;
        _config.destination_y = (_plan.tensor_height_ - destination_height) / 2U;
    }
    for (std::size_t channel = 0U; channel < g_tensor_channels; ++channel) {
        _config.pad[channel] = _plan.preprocess_.pad_value_[channel];
        _config.offset[channel] = _plan.preprocess_.offset_[channel];
        _config.scale[channel] = _plan.preprocess_.scale_[channel];
    }
    _config.quantization_scale = _target.quantization_.scale_;
    _config.quantization_zero_point = _target.quantization_.zero_point_;
    return {};
}

}  // namespace

struct dsp_preprocessor::implementation {
    explicit implementation(dsp_preprocessor_config _config)
        : config_(std::move(_config)) {}

    dsp_preprocessor_config config_;
    rpcmem_pool output_pool_;
    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_IMAGE_DESCRIPTOR_BYTES> descriptor_{};
    std::shared_ptr<const void> quarantined_input_;
    std::size_t output_capacity_bytes_{0U};
    bool has_uncertain_completion_{false};
};

dsp_preprocessor::dsp_preprocessor(dsp_preprocessor_config _config)
    : implementation_(std::make_unique<implementation>(std::move(_config))) {}

dsp_preprocessor::~dsp_preprocessor() noexcept {
    if (implementation_ != nullptr && implementation_->has_uncertain_completion_) {
        // Process-lifetime quarantine: a failed FastRPC return does not prove cDSP stopped
        // touching the registered source or rpcmem tensor.
        (void)implementation_.release();
    }
}

dsp_preprocessor::dsp_preprocessor(dsp_preprocessor&&) noexcept = default;
dsp_preprocessor& dsp_preprocessor::operator=(dsp_preprocessor&&) noexcept = default;

status dsp_preprocessor::vqec_vision_ai_ports_imgpr_validate(
    const raw_frame& _frame, const inference_plan& _plan,
    const tensor_spec& _target) const {
    if (implementation_ == nullptr || implementation_->config_.client_ == nullptr ||
        implementation_->config_.buffer_cache_ == nullptr ||
        implementation_->has_uncertain_completion_ ||
        !implementation_->config_.client_->vqec_vision_ai_qcom_d1cli_is_configured()) {
        return {status_code::invalid_state, "DSP v1 image processor is not runnable"};
    }
    if (implementation_->config_.client_->vqec_vision_ai_qcom_d1cli_is_open() &&
        !vqec_vision_ai_qcom_dsppr_has_image_operation(
            *implementation_->config_.client_)) {
        return {status_code::unsupported,
            "DSP v1 domain does not advertise image_transform"};
    }
    const auto plan_status = vqec_vision_ai_core_infpl_validate_plan(_plan);
    if (plan_status.code_ != status_code::ok) {
        return plan_status;
    }
    const auto preprocess_status = vqec_vision_ai_core_ppspc_validate(_plan.preprocess_);
    if (preprocess_status.code_ != status_code::ok) {
        return preprocess_status;
    }
    const auto& source = _frame.descriptor_;
    if (!_frame.owner_ || _frame.native_handle_ < 0 ||
        _frame.native_handle_ > std::numeric_limits<int>::max() ||
        source.width_ != _plan.source_width_ || source.height_ != _plan.source_height_ ||
        source.width_ == 0U || source.height_ == 0U ||
        source.width_ > VQEC_VISION_AI_DSP_V1_IMAGE_MAX_SIDE ||
        source.height_ > VQEC_VISION_AI_DSP_V1_IMAGE_MAX_SIDE ||
        (source.width_ & 1U) != 0U || (source.height_ & 1U) != 0U ||
        source.allocation_size_bytes_ == 0U || source.view_size_bytes_ == 0U ||
        source.view_size_bytes_ > VQEC_VISION_AI_DSP_V1_IMAGE_MAX_INPUT_BYTES ||
        source.memory_offset_bytes_ > source.allocation_size_bytes_ ||
        source.view_size_bytes_ > source.allocation_size_bytes_ - source.memory_offset_bytes_) {
        return {status_code::invalid_argument, "invalid RAW frame for DSP v1 preprocessing"};
    }
    for (std::size_t plane = 0U; plane < source.strides_.size(); ++plane) {
        const auto rows = plane == 0U ? source.height_ : source.height_ / 2U;
        if (source.strides_[plane] < static_cast<std::int32_t>(source.width_) ||
            source.offsets_[plane] > source.view_size_bytes_ ||
            rows > (source.view_size_bytes_ - source.offsets_[plane]) /
                static_cast<std::uint64_t>(source.strides_[plane])) {
            return {status_code::invalid_argument, "DSP v1 source plane exceeds memory view"};
        }
    }
    const auto luma_end = source.offsets_[0] +
        static_cast<std::uint64_t>(source.strides_[0]) * source.height_;
    if (source.offsets_[1] < luma_end) {
        return {status_code::invalid_argument, "DSP v1 source planes overlap"};
    }
    if (_target.layout_ != tensor_layout::nhwc ||
        _target.dtype_ != tensor_element_type::uint16 ||
        _target.dimensions_.size() != 4U || _target.dimensions_[0] != g_tensor_batch ||
        _target.dimensions_[1] != _plan.tensor_height_ ||
        _target.dimensions_[2] != _plan.tensor_width_ ||
        _target.dimensions_[3] != g_tensor_channels ||
        !_target.quantization_.is_quantized_) {
        return {status_code::unsupported,
            "DSP v1 image transform requires quantized UINT16 NHWC RGB"};
    }
    if (_plan.preprocess_.source_format_ != source_pixel_format::nv12 ||
        _plan.preprocess_.matrix_ != color_matrix::bt709 ||
        _plan.preprocess_.range_ != color_range::limited ||
        _plan.preprocess_.resize_ != resize_mode::letterbox ||
        _plan.preprocess_.interpolation_ != interpolation_mode::bilinear ||
        (_plan.preprocess_.placement_ != image_placement::centre &&
            _plan.preprocess_.placement_ != image_placement::top_left) ||
        _plan.preprocess_.channels_ != channel_order::rgb ||
        _plan.preprocess_.normalization_ != normalization_formula::offset_scale) {
        return {status_code::unsupported,
            "preprocess contract exceeds DSP v1 image_transform capability"};
    }
    const auto direct_mapping =
        vqec_vision_ai_core_color_validate_direct_integer_mapping(
            _plan.preprocess_, _target);
    if (direct_mapping.code_ != status_code::ok) {
        return direct_mapping;
    }
    vqec_vision_ai_dsp_v1_image_config config{};
    return vqec_vision_ai_qcom_dsppr_make_config(_frame, _plan, _target, config);
}

status dsp_preprocessor::vqec_vision_ai_ports_imgpr_preprocess(
    const raw_frame& _frame, const inference_plan& _plan,
    const tensor_spec& _target, std::vector<tensor_blob>& _outputs) {
    if (implementation_ == nullptr || implementation_->config_.client_ == nullptr) {
        return {status_code::invalid_state, "DSP v1 image processor is unavailable"};
    }
    // preprocess is reached only with a retained source frame. This is the first legal
    // point at which the Qualcomm process domain may be activated.
    const auto opened =
        implementation_->config_.client_->vqec_vision_ai_qcom_d1cli_ensure_open();
    if (opened.code_ != status_code::ok) {
        return opened;
    }
    const auto valid = vqec_vision_ai_ports_imgpr_validate(_frame, _plan, _target);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    auto& impl = *implementation_;
    const auto expected_bytes = vqec_vision_ai_core_tnctr_shape_bytes(_target);
    if (expected_bytes == 0U || expected_bytes > VQEC_VISION_AI_DSP_V1_IMAGE_MAX_OUTPUT_BYTES) {
        return {status_code::invalid_argument, "DSP v1 target tensor size is invalid"};
    }
    if (impl.output_pool_.vqec_vision_ai_qcom_rpcm_count() == 0U ||
        impl.output_capacity_bytes_ < expected_bytes) {
        const auto allocated = impl.output_pool_.vqec_vision_ai_qcom_rpcm_allocate(
            static_cast<std::size_t>(expected_bytes), 1U);
        if (allocated.code_ != status_code::ok) {
            return allocated;
        }
        impl.output_capacity_bytes_ =
            impl.output_pool_.vqec_vision_ai_qcom_rpcm_slot(0U).size_;
    }
    const auto& output = impl.output_pool_.vqec_vision_ai_qcom_rpcm_slot(0U);
    if (output.data_ == nullptr || output.size_ < expected_bytes) {
        return {status_code::invalid_state, "DSP v1 output rpcmem is unavailable"};
    }
    status map_status;
    const auto mapping = impl.config_.buffer_cache_->vqec_vision_ai_qcom_dspbc_map(
        static_cast<int>(_frame.native_handle_),
        static_cast<std::size_t>(_frame.descriptor_.allocation_size_bytes_), map_status);
    if (mapping.data_ == nullptr || map_status.code_ != status_code::ok) {
        return map_status.code_ == status_code::ok ?
            status{status_code::io_error, "DSP input mapping returned null"} : map_status;
    }
    const auto* input = mapping.data_ + _frame.descriptor_.memory_offset_bytes_;
    const auto input_bytes = static_cast<std::size_t>(_frame.descriptor_.view_size_bytes_);
    vqec_vision_ai_dsp_v1_image_config config{};
    const auto configured =
        vqec_vision_ai_qcom_dsppr_make_config(_frame, _plan, _target, config);
    if (configured.code_ != status_code::ok) {
        return configured;
    }
    const auto capabilities = impl.config_.client_->vqec_vision_ai_qcom_d1cli_capabilities();
    const auto encoded = vqec_vision_ai_qcom_d1img_encode_descriptor(
        &config, capabilities.domain_generation, input_bytes, expected_bytes,
        impl.descriptor_.data(), impl.descriptor_.size());
    if (encoded != vqec_vision_ai_dsp_v1_wire_ok) {
        return {status_code::invalid_argument,
            "cannot encode DSP v1 image_transform descriptor"};
    }
    const auto call = impl.config_.client_->vqec_vision_ai_qcom_d1cli_execute(
        impl.descriptor_.data(), impl.descriptor_.size(), input, input_bytes,
        static_cast<std::uint8_t*>(output.data_), expected_bytes);
    if (call.completion_ == dsp_v1_completion::uncertain) {
        impl.has_uncertain_completion_ = true;
        impl.quarantined_input_ = mapping.owner_;
        return call.status_;
    }
    if (call.status_.code_ != status_code::ok || call.output_bytes_ != expected_bytes) {
        return call.status_.code_ != status_code::ok ? call.status_ :
            status{status_code::protocol_error,
                "DSP v1 image_transform returned an unexpected tensor size"};
    }
    tensor_blob candidate;
    const bool can_reuse = _outputs.size() == 1U &&
        _outputs[0].spec_.dtype_ == _target.dtype_ &&
        _outputs[0].spec_.dimensions_ == _target.dimensions_ &&
        _outputs[0].bytes_.size() == expected_bytes;
    try {
        if (can_reuse) {
            candidate = std::move(_outputs[0]);
        } else {
            candidate.bytes_.resize(expected_bytes);
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "cannot allocate DSP v1 output tensor owner"};
    }
    candidate.spec_ = _target;
    std::memcpy(candidate.bytes_.data(), output.data_, expected_bytes);
    if (can_reuse) {
        _outputs[0] = std::move(candidate);
    } else {
        _outputs.clear();
        _outputs.push_back(std::move(candidate));
    }
    return {};
}

}  // namespace vqec::vision::ai
