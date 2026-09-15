// Device-free tests for the generic embedding decoder: manifest validation, L2
// normalization, dimension/norm rejection and transactional output.

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include "vqec_vision_embedding_decoder.hpp"

using namespace vqec::vision::ai;

namespace {

embedding_decoder_config make_config() {
    embedding_decoder_config config;
    config.model_id_ = "edgeface_s_gamma_05";
    config.model_version_ = "1.0";
    config.output_tensor_ = "embedding";
    config.dimension_ = 4;
    config.min_norm_ = 1e-6F;
    return config;
}

model_outputs make_outputs(std::size_t _dimension) {
    model_outputs outputs;
    outputs.outputs_.push_back(
        {"embedding", {1U, static_cast<std::uint32_t>(_dimension)},
            tensor_element_type::float32, {}});
    return outputs;
}

tensor_result make_result(const std::vector<float>& _values) {
    tensor_result result;
    tensor_blob blob;
    blob.spec_.name_ = "embedding";
    blob.spec_.dimensions_ = {1U, static_cast<std::uint32_t>(_values.size())};
    blob.spec_.dtype_ = tensor_element_type::float32;
    blob.bytes_.resize(_values.size() * sizeof(float));
    std::memcpy(blob.bytes_.data(), _values.data(), blob.bytes_.size());
    result.tensors_.push_back(std::move(blob));
    return result;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    embedding_decoder decoder(make_config());
    const preview_frame_key frame{0U, 0U, 1U, 1U, 1000U};
    check(decoder.vqec_vision_ai_ports_embdec_validate(make_outputs(4)).code_ ==
        status_code::ok);
    check(decoder.vqec_vision_ai_ports_embdec_validate(make_outputs(3)).code_ ==
        status_code::invalid_argument);

    embedding_result embedding;
    const auto decoded = decoder.vqec_vision_ai_ports_embdec_decode(
        make_result({3.0F, 4.0F, 0.0F, 0.0F}), frame, 7U, embedding);
    check(decoded.code_ == status_code::ok && embedding.is_l2_normalized_ &&
        embedding.track_id_ == 7U && embedding.model_id_ == "edgeface_s_gamma_05");
    check(std::fabs(embedding.values_[0] - 0.6F) < 1e-5F &&
        std::fabs(embedding.values_[1] - 0.8F) < 1e-5F);

    // A missing tensor, a wrong dimension and a zero vector are rejected.
    tensor_result empty;
    check(decoder.vqec_vision_ai_ports_embdec_decode(empty, frame, 1U, embedding).code_ ==
        status_code::invalid_argument);
    check(decoder.vqec_vision_ai_ports_embdec_decode(
              make_result({1.0F, 2.0F}), frame, 1U, embedding).code_ ==
        status_code::invalid_argument);
    check(decoder.vqec_vision_ai_ports_embdec_decode(
              make_result({0.0F, 0.0F, 0.0F, 0.0F}), frame, 1U, embedding).code_ ==
        status_code::protocol_error);

    // Output is preserved on failure.
    embedding_result preserved = embedding;
    check(decoder.vqec_vision_ai_ports_embdec_decode(
              make_result({0.0F, 0.0F, 0.0F, 0.0F}), frame, 1U, embedding).code_ !=
        status_code::ok);
    check(embedding.values_ == preserved.values_);

    std::cout << "embedding decoder failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
