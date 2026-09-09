#include "vqec_vision_model_decode_stage.hpp"

#include <utility>

namespace vqec::vision::ai {

model_decode_stage::model_decode_stage(model_decoder_port& _decoder, preview_geometry _geometry)
    : decoder_(_decoder), geometry_(_geometry) {}

status model_decode_stage::vqec_vision_ai_detec_mdstg_decode_result(
    const tensor_result& _result, const preview_frame_key& _expected_frame,
    observation_batch& _observations) {
    observation_batch candidate;
    const auto decoded = decoder_.vqec_vision_ai_cntr_mddec_decode(
        _result, _expected_frame, candidate);
    if (decoded.code_ != status_code::ok) {
        return decoded;
    }
    const auto validated = vqec_vision_ai_core_obval_validate_batch(
        candidate, _expected_frame, geometry_);
    if (validated.code_ != status_code::ok) {
        return validated;
    }
    _observations = std::move(candidate);
    return {};
}

}  // namespace vqec::vision::ai
