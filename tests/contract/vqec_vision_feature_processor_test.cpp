#include <cassert>

#include "vqec/vision/ai/ports/vqec_vision_feature_processor.hpp"

using namespace vqec::vision::ai;

namespace {

class fake_feature_processor final : public feature_processor_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_validate_activation(
        const feature_processor_config& _config) const override {
        return vqec_vision_ai_core_ftevt_validate_processor_config(_config);
    }

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_reset_epoch(
        std::uint64_t _source_epoch) override {
        epoch_ = _source_epoch;
        return _source_epoch == 0 ?
            status{status_code::invalid_argument, "source epoch is zero"} : status{};
    }

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_process_observations(
        const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, feature_event_batch& _events) override {
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        if (_tracked.frame_.source_epoch_ != epoch_) {
            return {status_code::invalid_state, "feature processor epoch mismatch"};
        }
        _events = {_tracked.frame_, _tracked.geometry_, {}};
        return {};
    }

private:
    std::uint64_t epoch_{0};
};

}  // namespace

int main() {
    fake_feature_processor processor;
    const feature_processor_config config{"source.front", "counting", 2, 4, 8, 8};
    assert(processor.vqec_vision_ai_ports_ftpro_validate_activation(config).code_ ==
           status_code::ok);
    assert(processor.vqec_vision_ai_ports_ftpro_reset_epoch(9).code_ == status_code::ok);
    const preview_frame_key frame{3, 1, 9, 20, 100};
    observation_batch tracked{frame, {1280, 720}, {}};
    feature_event_batch events;
    assert(processor.vqec_vision_ai_ports_ftpro_process_observations(
               tracked, 1000, false, events).code_ == status_code::ok);
    assert(events.frame_.source_epoch_ == 9);
    return 0;
}
