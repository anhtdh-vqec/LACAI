#ifndef VQEC_VISION_AI_EMBED_EMBEDDING_DECODER_HPP
#define VQEC_VISION_AI_EMBED_EMBEDDING_DECODER_HPP

#include <cstddef>
#include <string>

#include "vqec/vision/ai/ports/perception/vqec_vision_embedding_decoder.hpp"

namespace vqec::vision::ai {

// Package-configured, model-agnostic embedding decoder. Tensor name, expected dimension and
// minimum norm come from the reviewed model package, not from a model name.
struct embedding_decoder_config {
    std::string model_id_;
    std::string model_version_;
    std::string output_tensor_;
    std::size_t dimension_{0};
    float min_norm_{0.0F};
};

class embedding_decoder final : public embedding_decoder_port {
public:
    embedding_decoder() = default;
    explicit embedding_decoder(embedding_decoder_config _config);

    [[nodiscard]] status vqec_vision_ai_ports_embdec_validate(
        const model_outputs& _outputs) const override;
    [[nodiscard]] status vqec_vision_ai_ports_embdec_decode(
        const tensor_result& _result, const preview_frame_key& _frame,
        std::uint64_t _track_id, embedding_result& _embedding) override;

private:
    embedding_decoder_config config_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_EMBED_EMBEDDING_DECODER_HPP
