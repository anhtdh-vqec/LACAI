#include "vqec_vision_dsp_v1_dense_decoder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>

extern "C" {
#include "vqec_vision_dsp_v1_dense.h"
}

#include "vqec/vision/ai/contracts/perception/vqec_vision_observation.hpp"
#include "vqec/vision/ai/contracts/inference/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::size_t g_box_channels = 4U;
constexpr std::size_t g_float_bytes = sizeof(float);
constexpr std::size_t g_class_index_offset = 5U * g_float_bytes;

const tensor_spec* vqec_vision_ai_qcom_d1ddc_find_spec(
    const model_outputs& _outputs, std::string_view _name) noexcept {
    const tensor_spec* matched = nullptr;
    for (const auto& output : _outputs.outputs_) {
        if (output.name_ == _name) {
            if (matched != nullptr) {
                return nullptr;
            }
            matched = &output;
        }
    }
    return matched;
}

const tensor_blob* vqec_vision_ai_qcom_d1ddc_find_blob(
    const tensor_result& _result, std::string_view _name) noexcept {
    const tensor_blob* matched = nullptr;
    for (const auto& tensor : _result.tensors_) {
        if (tensor.spec_.name_ == _name) {
            if (matched != nullptr) {
                return nullptr;
            }
            matched = &tensor;
        }
    }
    return matched;
}

bool vqec_vision_ai_qcom_d1ddc_valid_tensor(
    const tensor_spec& _spec, std::uint32_t _channels,
    std::uint32_t _predictions) noexcept {
    return _spec.dtype_ == tensor_element_type::uint16 &&
        _spec.layout_ == tensor_layout::flat && _spec.quantization_.is_quantized_ &&
        std::isfinite(_spec.quantization_.scale_) && _spec.quantization_.scale_ > 0.0F &&
        _spec.dimensions_.size() == 3U && _spec.dimensions_[0] == 1U &&
        _spec.dimensions_[1] == _channels && _spec.dimensions_[2] == _predictions;
}

status vqec_vision_ai_qcom_d1ddc_validate_config(
    const dsp_v1_dense_decoder_config& _config) {
    if (_config.source_width_ == 0U || _config.source_height_ == 0U ||
        _config.tensor_width_ == 0U || _config.tensor_height_ == 0U ||
        _config.box_tensor_.empty() || _config.score_tensor_.empty() ||
        _config.box_tensor_ == _config.score_tensor_ || _config.prediction_count_ == 0U ||
        _config.prediction_count_ > VQEC_VISION_AI_DSP_V1_DENSE_MAX_PREDICTIONS ||
        _config.class_count_ == 0U ||
        _config.class_count_ > VQEC_VISION_AI_DSP_V1_DENSE_MAX_CLASSES ||
        _config.class_names_.size() != _config.class_count_ ||
        !std::isfinite(_config.confidence_threshold_) ||
        _config.confidence_threshold_ <= 0.0F || _config.confidence_threshold_ > 1.0F ||
        !std::isfinite(_config.iou_threshold_) || _config.iou_threshold_ <= 0.0F ||
        _config.iou_threshold_ > 1.0F || _config.candidate_capacity_ == 0U ||
        _config.candidate_capacity_ > VQEC_VISION_AI_DSP_V1_DENSE_MAX_CANDIDATES ||
        _config.output_capacity_ == 0U ||
        _config.output_capacity_ > VQEC_VISION_AI_DSP_V1_DENSE_MAX_OUTPUTS ||
        _config.output_capacity_ > _config.candidate_capacity_ || _config.client_ == nullptr) {
        return {status_code::invalid_argument, "invalid DSP v1 dense decoder configuration"};
    }
    if (_config.placement_ != image_placement::centre &&
        _config.placement_ != image_placement::top_left &&
        _config.placement_ != image_placement::stretch) {
        return {status_code::unsupported, "DSP v1 dense decoder placement is unsupported"};
    }
    if (!_config.client_->vqec_vision_ai_qcom_d1cli_is_configured()) {
        return {status_code::invalid_state, "DSP v1 dense client is not configured"};
    }
    if (_config.client_->vqec_vision_ai_qcom_d1cli_is_open()) {
        const auto capabilities = _config.client_->vqec_vision_ai_qcom_d1cli_capabilities();
        const std::uint32_t dense_mask =
            1U << (VQEC_VISION_AI_DSP_V1_DENSE_DECODE - 1U);
        if ((capabilities.operations_mask & dense_mask) == 0U) {
            return {status_code::unsupported,
                "DSP v1 service does not advertise dense_decode"};
        }
    }
    return {};
}

float vqec_vision_ai_qcom_d1ddc_read_f32(const std::uint8_t* _input) noexcept {
    const std::uint32_t bits = static_cast<std::uint32_t>(_input[0]) |
        (static_cast<std::uint32_t>(_input[1]) << 8U) |
        (static_cast<std::uint32_t>(_input[2]) << 16U) |
        (static_cast<std::uint32_t>(_input[3]) << 24U);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t vqec_vision_ai_qcom_d1ddc_read_u32(const std::uint8_t* _input) noexcept {
    return static_cast<std::uint32_t>(_input[0]) |
        (static_cast<std::uint32_t>(_input[1]) << 8U) |
        (static_cast<std::uint32_t>(_input[2]) << 16U) |
        (static_cast<std::uint32_t>(_input[3]) << 24U);
}

float vqec_vision_ai_qcom_d1ddc_fit_extent(
    float _origin, float _extent, std::uint32_t _limit) noexcept {
    const double available = static_cast<double>(_limit) - static_cast<double>(_origin);
    if (!(available > 0.0) || !std::isfinite(_extent)) {
        return 0.0F;
    }
    float fitted = static_cast<float>(std::min(static_cast<double>(_extent), available));
    if (static_cast<double>(fitted) > available) {
        fitted = std::nextafter(fitted, 0.0F);
    }
    return fitted;
}

}  // namespace

dsp_v1_dense_decoder::dsp_v1_dense_decoder(dsp_v1_dense_decoder_config _config)
    : config_(std::move(_config)) {
    if (config_.prediction_count_ <= VQEC_VISION_AI_DSP_V1_DENSE_MAX_PREDICTIONS &&
        config_.class_count_ <= VQEC_VISION_AI_DSP_V1_DENSE_MAX_CLASSES &&
        config_.output_capacity_ <= VQEC_VISION_AI_DSP_V1_DENSE_MAX_OUTPUTS) {
        const std::size_t box_bytes = static_cast<std::size_t>(config_.prediction_count_) *
            g_box_channels * sizeof(std::uint16_t);
        const std::size_t score_bytes = static_cast<std::size_t>(config_.prediction_count_) *
            config_.class_count_ * sizeof(std::uint16_t);
        packed_input_.resize(box_bytes + score_bytes);
        compact_output_.resize(static_cast<std::size_t>(config_.output_capacity_) *
            VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES);
    }
}

status dsp_v1_dense_decoder::vqec_vision_ai_cntr_mddec_validate(
    const model_outputs& _outputs) const {
    const auto configured = vqec_vision_ai_qcom_d1ddc_validate_config(config_);
    if (configured.code_ != status_code::ok) {
        return configured;
    }
    if (_outputs.outputs_.size() != 2U) {
        return {status_code::unsupported, "DSP v1 dense decoder requires two tensor roles"};
    }
    const auto* boxes = vqec_vision_ai_qcom_d1ddc_find_spec(_outputs, config_.box_tensor_);
    const auto* scores = vqec_vision_ai_qcom_d1ddc_find_spec(_outputs, config_.score_tensor_);
    if (boxes == nullptr || scores == nullptr ||
        !vqec_vision_ai_qcom_d1ddc_valid_tensor(
            *boxes, static_cast<std::uint32_t>(g_box_channels), config_.prediction_count_) ||
        !vqec_vision_ai_qcom_d1ddc_valid_tensor(
            *scores, config_.class_count_, config_.prediction_count_)) {
        return {status_code::unsupported, "DSP v1 dense tensor contract mismatch"};
    }
    return {};
}

status dsp_v1_dense_decoder::vqec_vision_ai_cntr_mddec_decode(
    const tensor_result& _result, const preview_frame_key& _expected_frame,
    observation_batch& _observations) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (config_.client_ == nullptr) {
        return {status_code::invalid_state, "DSP v1 dense client is unavailable"};
    }
    const auto opened = config_.client_->vqec_vision_ai_qcom_d1cli_ensure_open();
    if (opened.code_ != status_code::ok) {
        return opened;
    }
    const auto configured = vqec_vision_ai_qcom_d1ddc_validate_config(config_);
    if (configured.code_ != status_code::ok) {
        return configured;
    }
    if (_result.tensors_.size() != 2U) {
        return {status_code::unsupported, "DSP v1 dense result requires two tensor roles"};
    }
    const auto* boxes = vqec_vision_ai_qcom_d1ddc_find_blob(_result, config_.box_tensor_);
    const auto* scores = vqec_vision_ai_qcom_d1ddc_find_blob(_result, config_.score_tensor_);
    if (boxes == nullptr || scores == nullptr ||
        !vqec_vision_ai_qcom_d1ddc_valid_tensor(
            boxes->spec_, static_cast<std::uint32_t>(g_box_channels),
            config_.prediction_count_) ||
        !vqec_vision_ai_qcom_d1ddc_valid_tensor(
            scores->spec_, config_.class_count_, config_.prediction_count_)) {
        return {status_code::unsupported, "DSP v1 dense result tensor contract mismatch"};
    }
    const std::size_t expected_box_bytes = static_cast<std::size_t>(config_.prediction_count_) *
        g_box_channels * sizeof(std::uint16_t);
    const std::size_t expected_score_bytes =
        static_cast<std::size_t>(config_.prediction_count_) * config_.class_count_ *
        sizeof(std::uint16_t);
    if (boxes->bytes_.size() != expected_box_bytes ||
        scores->bytes_.size() != expected_score_bytes ||
        packed_input_.size() != expected_box_bytes + expected_score_bytes) {
        return {status_code::invalid_argument, "DSP v1 dense result byte count mismatch"};
    }

    std::memcpy(packed_input_.data(), boxes->bytes_.data(), expected_box_bytes);
    std::memcpy(packed_input_.data() + expected_box_bytes, scores->bytes_.data(),
        expected_score_bytes);

    const float source_width = static_cast<float>(config_.source_width_);
    const float source_height = static_cast<float>(config_.source_height_);
    const float tensor_width = static_cast<float>(config_.tensor_width_);
    const float tensor_height = static_cast<float>(config_.tensor_height_);
    const float scale = std::min(tensor_width / source_width, tensor_height / source_height);

    vqec_vision_ai_dsp_v1_dense_config descriptor{};
    descriptor.flags = VQEC_VISION_AI_DSP_V1_DENSE_CLASS_AWARE_NMS;
    descriptor.prediction_count = config_.prediction_count_;
    descriptor.class_count = config_.class_count_;
    descriptor.box_offset = 0U;
    descriptor.score_offset = static_cast<std::uint32_t>(expected_box_bytes);
    descriptor.source_width = config_.source_width_;
    descriptor.source_height = config_.source_height_;
    descriptor.candidate_capacity = config_.candidate_capacity_;
    descriptor.output_capacity = config_.output_capacity_;
    descriptor.box_scale = boxes->spec_.quantization_.scale_;
    descriptor.box_zero_point = boxes->spec_.quantization_.zero_point_;
    descriptor.score_scale = scores->spec_.quantization_.scale_;
    descriptor.score_zero_point = scores->spec_.quantization_.zero_point_;
    descriptor.confidence_threshold = config_.confidence_threshold_;
    descriptor.iou_threshold = config_.iou_threshold_;
    descriptor.scale_x = config_.placement_ == image_placement::stretch ?
        tensor_width / source_width : scale;
    descriptor.scale_y = config_.placement_ == image_placement::stretch ?
        tensor_height / source_height : scale;
    descriptor.pad_x = config_.placement_ == image_placement::centre ?
        (tensor_width - source_width * scale) / 2.0F : 0.0F;
    descriptor.pad_y = config_.placement_ == image_placement::centre ?
        (tensor_height - source_height * scale) / 2.0F : 0.0F;

    const auto capabilities = config_.client_->vqec_vision_ai_qcom_d1cli_capabilities();
    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES> wire{};
    const auto encoded = vqec_vision_ai_qcom_d1dns_encode_descriptor(
        &descriptor, capabilities.domain_generation, packed_input_.size(),
        compact_output_.size(), wire.data(), wire.size());
    if (encoded != vqec_vision_ai_dsp_v1_wire_ok) {
        return {status_code::invalid_argument, "DSP v1 dense descriptor rejected"};
    }
    const auto executed = config_.client_->vqec_vision_ai_qcom_d1cli_execute(
        wire.data(), wire.size(), packed_input_.data(), packed_input_.size(),
        compact_output_.data(), compact_output_.size());
    if (executed.status_.code_ != status_code::ok ||
        executed.completion_ != dsp_v1_completion::completed) {
        return executed.status_;
    }
    if (executed.detail_ != 0U) {
        return {status_code::resource_exhausted,
            "DSP v1 dense candidate capacity was exceeded"};
    }
    if (executed.output_bytes_ > compact_output_.size() ||
        executed.output_bytes_ % VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES != 0U) {
        return {status_code::protocol_error, "DSP v1 dense response length mismatch"};
    }

    observation_batch batch;
    batch.frame_ = _expected_frame;
    batch.geometry_ = {config_.source_width_, config_.source_height_};
    const std::size_t count =
        executed.output_bytes_ / VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES;
    batch.observations_.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint8_t* record = compact_output_.data() +
            index * VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES;
        const float x1 = vqec_vision_ai_qcom_d1ddc_read_f32(record);
        const float y1 = vqec_vision_ai_qcom_d1ddc_read_f32(record + g_float_bytes);
        const float x2 = vqec_vision_ai_qcom_d1ddc_read_f32(record + 2U * g_float_bytes);
        const float y2 = vqec_vision_ai_qcom_d1ddc_read_f32(record + 3U * g_float_bytes);
        const float confidence =
            vqec_vision_ai_qcom_d1ddc_read_f32(record + 4U * g_float_bytes);
        const std::uint32_t class_index =
            vqec_vision_ai_qcom_d1ddc_read_u32(record + g_class_index_offset);
        if (!std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(x2) ||
            !std::isfinite(y2) || !std::isfinite(confidence) || x1 < 0.0F || y1 < 0.0F ||
            x2 <= x1 || y2 <= y1 || x2 > source_width || y2 > source_height ||
            confidence < 0.0F || confidence > 1.0F ||
            class_index >= config_.class_names_.size()) {
            return {status_code::protocol_error, "DSP v1 dense record is invalid"};
        }
        const float width = vqec_vision_ai_qcom_d1ddc_fit_extent(
            x1, x2 - x1, config_.source_width_);
        const float height = vqec_vision_ai_qcom_d1ddc_fit_extent(
            y1, y2 - y1, config_.source_height_);
        if (!(width > 0.0F) || !(height > 0.0F)) {
            return {status_code::protocol_error, "DSP v1 dense record has empty geometry"};
        }
        observation item;
        item.frame_ = _expected_frame;
        item.class_id_ = config_.class_names_[class_index];
        item.box_ = {x1, y1, width, height, 0xffffffffU, item.class_id_};
        item.confidence_ = confidence;
        item.quality_ = confidence >= 0.75F ? observation_quality::high :
            (confidence >= 0.5F ? observation_quality::medium : observation_quality::low);
        batch.observations_.push_back(std::move(item));
    }
    _observations = std::move(batch);
    return {};
}

}  // namespace vqec::vision::ai
