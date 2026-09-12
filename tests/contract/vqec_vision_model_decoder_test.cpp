#include "vqec/vision/ai/contracts/vqec_vision_model_decoder.hpp"

#include <cassert>
#include <utility>

using namespace vqec::vision::ai;

namespace {

class fake_model_decoder final : public model_decoder_port {
public:
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const override {
        return _outputs.model_id_.empty()
            ? status{status_code::invalid_argument, "model identity is required"}
            : status{};
    }

    [[nodiscard]] status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) override {
        if (_result.tensors_.empty() || _expected_frame.frame_id_ == 0) {
            return {status_code::invalid_argument, "decoder input identity is incomplete"};
        }
        _observations.frame_ = _expected_frame;
        return {};
    }
};

}  // namespace

int main() {
    fake_model_decoder decoder;
    model_outputs outputs;
    assert(decoder.vqec_vision_ai_cntr_mddec_validate(outputs).code_ ==
           status_code::invalid_argument);
    outputs.model_id_ = "detector";
    assert(decoder.vqec_vision_ai_cntr_mddec_validate(outputs).code_ == status_code::ok);

    tensor_result result;
    tensor_blob boxes;
    boxes.spec_.name_ = "boxes";
    boxes.spec_.dimensions_ = {1, 4};
    boxes.spec_.dtype_ = tensor_element_type::float32;
    boxes.bytes_.assign(16, 0U);  // four float32 elements
    result.tensors_.push_back(std::move(boxes));
    preview_frame_key frame;
    frame.frame_id_ = 9;
    observation_batch observations;
    assert(decoder.vqec_vision_ai_cntr_mddec_decode(result, frame, observations).code_ ==
           status_code::ok);
    assert(observations.frame_.frame_id_ == 9);
    return 0;
}
