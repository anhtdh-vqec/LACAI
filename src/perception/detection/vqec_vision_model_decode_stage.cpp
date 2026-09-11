#include "vqec_vision_model_decode_stage.hpp"

#include <utility>

namespace vqec::vision::ai {

model_decode_stage::model_decode_stage(model_decoder_port& _decoder, preview_geometry _geometry)
    : decoder_(_decoder), geometry_(_geometry) {}

status model_decode_stage::vqec_vision_ai_detec_mdstg_validate_geometry(
    const preview_geometry& _expected_geometry) const {
    const preview_frame_key frame{0, 0, 1, 1, 0};
    return vqec_vision_ai_core_pvctr_validate_identity(
        frame, frame, geometry_, _expected_geometry);
}

status model_decode_stage::vqec_vision_ai_detec_mdstg_decode_result(
    const tensor_result& _result, const preview_frame_key& _expected_frame,
    observation_batch& _observations) {
    scratch_.frame_ = {};
    scratch_.geometry_ = {};
    scratch_.observations_.clear();
    status decoded;
    try {
        decoded = decoder_.vqec_vision_ai_cntr_mddec_decode(
            _result, _expected_frame, scratch_);
    } catch (...) {
        return {status_code::io_error, "model decoder raised an exception"};
    }
    if (decoded.code_ != status_code::ok) {
        return decoded;
    }
    const auto validated = vqec_vision_ai_core_obval_validate_detections(
        scratch_, _expected_frame, geometry_);
    if (validated.code_ != status_code::ok) {
        return validated;
    }
    std::swap(_observations, scratch_);
    return {};
}

}  // namespace vqec::vision::ai
