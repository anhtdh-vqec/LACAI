#include "vqec_vision_embedding_decoder.hpp"

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "vqec_vision_tensor_reader.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_embed_embdd_element_count(
    const tensor_spec& _spec, std::size_t& _count) noexcept {
    std::size_t product = 1;
    for (const auto dimension : _spec.dimensions_) {
        if (dimension == 0 ||
            product > std::numeric_limits<std::size_t>::max() / dimension) {
            return false;
        }
        product *= dimension;
    }
    _count = product;
    return true;
}

}  // namespace

embedding_decoder::embedding_decoder(embedding_decoder_config _config)
    : config_(std::move(_config)) {}

status embedding_decoder::vqec_vision_ai_ports_embdec_validate(
    const model_outputs& _outputs) const {
    if (!vqec_vision_ai_cntr_ident_is_valid(
            config_.model_id_, embedding_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            config_.model_version_, embedding_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            config_.output_tensor_, embedding_limits::g_max_identifier_bytes) ||
        config_.dimension_ == 0 ||
        config_.dimension_ > embedding_limits::g_max_dimensions ||
        !std::isfinite(config_.min_norm_) || config_.min_norm_ < 0.0F) {
        return {status_code::invalid_argument, "invalid embedding decoder configuration"};
    }
    for (const auto& output : _outputs.outputs_) {
        if (output.name_ != config_.output_tensor_) {
            continue;
        }
        std::size_t count = 0;
        if (!vqec_vision_ai_embed_embdd_element_count(output, count)) {
            return {status_code::unsupported, "embedding tensor has an invalid shape"};
        }
        if (count != config_.dimension_) {
            return {status_code::invalid_argument,
                "embedding tensor dimension differs from configuration"};
        }
        return {};
    }
    return {status_code::unsupported, "embedding tensor is absent from the manifest"};
}

status embedding_decoder::vqec_vision_ai_ports_embdec_decode(
    const tensor_result& _result, const preview_frame_key& _frame,
    std::uint64_t _track_id, embedding_result& _embedding) {
    if (config_.dimension_ == 0 ||
        config_.dimension_ > embedding_limits::g_max_dimensions) {
        return {status_code::invalid_state, "embedding decoder is not configured"};
    }
    const tensor_blob* tensor = nullptr;
    if (vqec_vision_ai_detec_tnrd_find_tensor(
            _result, config_.output_tensor_, tensor).code_ != status_code::ok) {
        return {status_code::invalid_argument, "embedding tensor is missing from the result"};
    }
    const auto element_bytes = vqec_vision_ai_core_tnctr_element_size(tensor->spec_.dtype_);
    if (element_bytes == 0 || tensor->bytes_.size() % element_bytes != 0) {
        return {status_code::unsupported, "embedding tensor dtype is unsupported"};
    }
    const std::size_t count = tensor->bytes_.size() / element_bytes;
    if (count != config_.dimension_) {
        return {status_code::invalid_argument,
            "embedding tensor element count differs from dimension"};
    }
    std::vector<float> values(count, 0.0F);
    double norm_squared = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        float value = 0.0F;
        if (vqec_vision_ai_detec_tnrd_read_scalar(*tensor, index, value).code_ !=
            status_code::ok) {
            return {status_code::unsupported, "cannot read embedding element"};
        }
        if (!std::isfinite(value)) {
            return {status_code::protocol_error, "embedding element is not finite"};
        }
        values[index] = value;
        norm_squared += static_cast<double>(value) * value;
    }
    const double norm = std::sqrt(norm_squared);
    if (!(norm > static_cast<double>(config_.min_norm_))) {
        return {status_code::protocol_error, "embedding norm is too small to normalize"};
    }
    embedding_result candidate;
    candidate.frame_ = _frame;
    candidate.track_id_ = _track_id;
    candidate.model_id_ = config_.model_id_;
    candidate.model_version_ = config_.model_version_;
    candidate.values_.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        candidate.values_[index] = static_cast<float>(values[index] / norm);
    }
    candidate.is_l2_normalized_ = true;
    const auto valid = vqec_vision_ai_core_embct_validate_result(candidate, _frame);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    _embedding = std::move(candidate);
    return {};
}

}  // namespace vqec::vision::ai
