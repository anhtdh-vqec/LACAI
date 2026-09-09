#include <cassert>
#include <utility>

#include "vqec_vision_feature_stage.hpp"

using namespace vqec::vision::ai;

namespace {

class fake_feature_processor final : public feature_processor_port {
public:
    explicit fake_feature_processor(feature_processor_config _config)
        : config_(std::move(_config)) {}

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_validate_activation(
        const feature_processor_config& _config) const override {
        return _config.source_id_ == config_.source_id_ &&
                       _config.feature_id_ == config_.feature_id_ ?
            status{} : status{status_code::invalid_argument, "feature config mismatch"};
    }

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_reset_epoch(
        std::uint64_t _source_epoch) override {
        ++reset_count_;
        epoch_ = _source_epoch;
        return {};
    }

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_process_observations(
        const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, feature_event_batch& _events) override {
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        if (_tracked.frame_.source_epoch_ != epoch_) {
            return {status_code::invalid_state, "processor epoch mismatch"};
        }
        feature_event event{_tracked.frame_,
                            config_.source_id_,
                            is_invalid_output_ ? "wrong_feature" : config_.feature_id_,
                            "event:1",
                            "security.intrusion",
                            "1",
                            feature_event_kind::episode_opened,
                            _tracked.frame_.source_pts_ns_,
                            config_.config_revision_,
                            {"person_detector:4"},
                            {1},
                            {},
                            {}};
        _events = {_tracked.frame_, _tracked.geometry_, {std::move(event)}};
        return {};
    }

    feature_processor_config config_;
    std::uint64_t epoch_{0};
    unsigned reset_count_{0};
    bool is_invalid_output_{false};
};

observation_batch vqec_vision_ai_ctest_fsct_make_tracked(std::uint64_t _epoch) {
    const preview_frame_key frame{1, 0, _epoch, 5, 20};
    return {frame, {640, 360},
            {{frame, 1, "person", {1, 2, 3, 4, 0xffffffffU, "person"}, 0.9F,
              observation_quality::high, {}}}};
}

}  // namespace

int main() {
    const feature_processor_config config{"source.front", "intrusion", 4, 4, 8, 8};
    fake_feature_processor processor(config);
    feature_stage stage(processor, config);
    feature_event_batch events;
    assert(stage.vqec_vision_ai_ftmgr_ftstg_process(
               vqec_vision_ai_ctest_fsct_make_tracked(1), 1, false, events).code_ ==
           status_code::invalid_state);
    assert(stage.vqec_vision_ai_ftmgr_ftstg_activate().code_ == status_code::ok);
    auto tracked = vqec_vision_ai_ctest_fsct_make_tracked(2);
    assert(stage.vqec_vision_ai_ftmgr_ftstg_process(tracked, 2, false, events).code_ ==
           status_code::ok);
    assert(processor.reset_count_ == 1U);
    assert(events.events_.size() == 1U);

    const auto prior_epoch = events.frame_.source_epoch_;
    processor.is_invalid_output_ = true;
    assert(stage.vqec_vision_ai_ftmgr_ftstg_process(tracked, 3, false, events).code_ ==
           status_code::invalid_argument);
    assert(stage.vqec_vision_ai_ftmgr_ftstg_is_faulted());
    assert(events.frame_.source_epoch_ == prior_epoch);
    assert(stage.vqec_vision_ai_ftmgr_ftstg_process(tracked, 4, false, events).code_ ==
           status_code::invalid_state);

    processor.is_invalid_output_ = false;
    tracked = vqec_vision_ai_ctest_fsct_make_tracked(3);
    assert(stage.vqec_vision_ai_ftmgr_ftstg_process(tracked, 5, true, events).code_ ==
           status_code::ok);
    assert(!stage.vqec_vision_ai_ftmgr_ftstg_is_faulted());
    assert(processor.reset_count_ == 2U);
    tracked = vqec_vision_ai_ctest_fsct_make_tracked(2);
    assert(stage.vqec_vision_ai_ftmgr_ftstg_process(tracked, 6, false, events).code_ ==
           status_code::invalid_state);
    return 0;
}
