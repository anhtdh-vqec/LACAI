#ifndef VQEC_VISION_AI_DETEC_MODEL_DECODE_STAGE_HPP
#define VQEC_VISION_AI_DETEC_MODEL_DECODE_STAGE_HPP

#include "vqec/vision/ai/contracts/vqec_vision_model_decoder.hpp"

namespace vqec::vision::ai {

// Owns no model state. It makes decoder output transactional and applies the
// model-independent observation checks before publication to downstream features.
class model_decode_stage final {
public:
    model_decode_stage(model_decoder_port& _decoder, preview_geometry _geometry);
    model_decode_stage(const model_decode_stage& _other) = delete;
    model_decode_stage& operator=(const model_decode_stage& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_detec_mdstg_decode_result(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations);
    [[nodiscard]] status vqec_vision_ai_detec_mdstg_validate_geometry(
        const preview_geometry& _expected_geometry) const;

private:
    model_decoder_port& decoder_;
    preview_geometry geometry_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_DETEC_MODEL_DECODE_STAGE_HPP
