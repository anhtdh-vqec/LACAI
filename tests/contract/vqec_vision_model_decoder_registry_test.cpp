#include <cassert>
#include <string>

#include "vqec_vision_model_decoder_registry.hpp"

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
        (void)_result;
        (void)_expected_frame;
        (void)_observations;
        return {};
    }
};

}  // namespace

int main() {
    model_decoder_registry registry;
    fake_decoder decoder;
    model_decoder_port* resolved = nullptr;
    assert(registry.vqec_vision_ai_detec_mdreg_get_count() == 0U);
    assert(registry.vqec_vision_ai_detec_mdreg_register_decoder("person.detector.v1", decoder)
               .code_ == status_code::ok);
    assert(registry.vqec_vision_ai_detec_mdreg_register_decoder("person.detector.v1", decoder)
               .code_ == status_code::invalid_argument);
    assert(registry.vqec_vision_ai_detec_mdreg_resolve_decoder(
               "person.detector.v1", resolved).code_ == status_code::ok);
    assert(resolved == &decoder);
    assert(registry.vqec_vision_ai_detec_mdreg_resolve_decoder("missing.v1", resolved).code_ ==
           status_code::unsupported);
    assert(resolved == nullptr);
    assert(registry.vqec_vision_ai_detec_mdreg_register_decoder("", decoder).code_ ==
           status_code::invalid_argument);
    assert(registry.vqec_vision_ai_detec_mdreg_register_decoder(
               std::string(model_decoder_limits::g_max_contract_bytes + 1U, 'x'), decoder)
               .code_ == status_code::invalid_argument);
    registry.vqec_vision_ai_detec_mdreg_clear();
    assert(registry.vqec_vision_ai_detec_mdreg_get_count() == 0U);
    return 0;
}
