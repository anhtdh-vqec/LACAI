#ifndef VQEC_VISION_AI_CONTRACTS_MODEL_DECODER_HPP
#define VQEC_VISION_AI_CONTRACTS_MODEL_DECODER_HPP

#include "vqec/vision/ai/contracts/vqec_vision_model_outputs.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"

namespace vqec::vision::ai {

// Neutral decoder boundary. Implementations own model-specific tensor semantics;
// callers own the input result lifetime and must provide the matching frame key.
class model_decoder_port {
public:
    virtual ~model_decoder_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const = 0;
    [[nodiscard]] virtual status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_MODEL_DECODER_HPP
