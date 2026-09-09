#include <cassert>
#include <stdexcept>

#include "vqec_vision_model_decode_stage.hpp"

using namespace vqec::vision::ai;

namespace {

class fake_decoder final : public model_decoder_port {
public:
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const override {
        return _outputs.model_id_.empty() ?
            status{status_code::invalid_argument, "model id required"} : status{};
    }

    [[nodiscard]] status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) override {
        if (should_throw_) {
            throw std::runtime_error("synthetic decoder exception");
        }
        if (_result.tensors_.empty()) {
            return {status_code::invalid_argument, "tensor result is empty"};
        }
        _observations.frame_ = _expected_frame;
        _observations.geometry_ = {640, 360};
        _observations.observations_.push_back(
            {_expected_frame, 0, "person", {10, 20, 30, 40, 0xffffffffU, "person"},
             0.9F, observation_quality::high, {}});
        return {};
    }

    bool should_throw_{false};
};

}  // namespace

int main() {
    fake_decoder decoder;
    model_decode_stage stage(decoder, {640, 360});
    assert(stage.vqec_vision_ai_detec_mdstg_validate_geometry({640, 360}).code_ ==
           status_code::ok);
    assert(stage.vqec_vision_ai_detec_mdstg_validate_geometry({320, 180}).code_ ==
           status_code::invalid_state);
    tensor_result result;
    result.tensors_.push_back({{"boxes", {1, 4}}, {0.0F, 1.0F, 2.0F, 3.0F}});
    const preview_frame_key frame{0, 0, 3, 9, 100};
    observation_batch observations;
    assert(stage.vqec_vision_ai_detec_mdstg_decode_result(result, frame, observations).code_ ==
           status_code::ok);
    assert(observations.observations_.size() == 1U);

    result.tensors_.clear();
    const auto failed = stage.vqec_vision_ai_detec_mdstg_decode_result(
        result, frame, observations);
    assert(failed.code_ == status_code::invalid_argument);
    assert(observations.observations_.size() == 1U);
    result.tensors_.push_back({{"boxes", {1, 4}}, {0.0F, 1.0F, 2.0F, 3.0F}});
    decoder.should_throw_ = true;
    assert(stage.vqec_vision_ai_detec_mdstg_decode_result(
               result, frame, observations).code_ == status_code::io_error);
    assert(observations.observations_.size() == 1U);
    return 0;
}
