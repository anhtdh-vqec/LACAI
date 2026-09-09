#include <array>
#include <cassert>
#include <utility>

#include "vqec_vision_feature_fanout.hpp"

using namespace vqec::vision::ai;

namespace {

class fake_processor final : public feature_processor_port {
public:
    explicit fake_processor(feature_processor_config _config) : config_(std::move(_config)) {}

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_validate_activation(
        const feature_processor_config& _config) const override {
        return _config.feature_id_ == config_.feature_id_ ? status{} :
            status{status_code::invalid_argument, "feature mismatch"};
    }

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_reset_epoch(
        std::uint64_t _source_epoch) override {
        epoch_ = _source_epoch;
        return {};
    }

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_process_observations(
        const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, feature_event_batch& _events) override {
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        feature_event event{_tracked.frame_,
                            config_.source_id_,
                            is_invalid_ ? "invalid" : config_.feature_id_,
                            config_.feature_id_ + ":event",
                            "feature.snapshot",
                            "1",
                            feature_event_kind::snapshot,
                            _tracked.frame_.source_pts_ns_,
                            config_.config_revision_,
                            {"person_detector:4"},
                            {1},
                            {},
                            {}};
        _events = {_tracked.frame_, _tracked.geometry_, {std::move(event)}};
        return _tracked.frame_.source_epoch_ == epoch_ ? status{} :
            status{status_code::invalid_state, "epoch mismatch"};
    }

    feature_processor_config config_;
    std::uint64_t epoch_{0};
    bool is_invalid_{false};
};

observation_batch vqec_vision_ai_ctest_ffct_make_tracked(std::uint64_t _epoch) {
    const preview_frame_key frame{1, 0, _epoch, 8, 20};
    return {frame, {640, 360},
            {{frame, 1, "person", {1, 2, 3, 4, 0xffffffffU, "person"}, 0.9F,
              observation_quality::high, {}}}};
}

}  // namespace

int main() {
    const feature_processor_config first_config{"source.front", "counting", 4, 4, 8, 8};
    const feature_processor_config second_config{"source.front", "intrusion", 4, 4, 8, 8};
    fake_processor first_processor(first_config);
    fake_processor second_processor(second_config);
    feature_stage first_stage(first_processor, first_config);
    feature_stage second_stage(second_processor, second_config);
    assert(first_stage.vqec_vision_ai_ftmgr_ftstg_activate().code_ == status_code::ok);
    assert(second_stage.vqec_vision_ai_ftmgr_ftstg_activate().code_ == status_code::ok);

    std::array<feature_stage*, feature_fanout_limits::g_max_feature_stages> stages{};
    stages[0] = &first_stage;
    stages[1] = &second_stage;
    feature_fanout fanout;
    assert(fanout.vqec_vision_ai_appl_ftfan_configure(stages, 2).code_ == status_code::ok);
    std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages> events{};
    feature_fanout_report report;
    auto tracked = vqec_vision_ai_ctest_ffct_make_tracked(1);
    assert(fanout.vqec_vision_ai_appl_ftfan_process(tracked, 1, false, events, report).code_ ==
           status_code::ok);
    assert(report.processed_mask_ == 3U && report.failed_mask_ == 0);

    second_processor.is_invalid_ = true;
    assert(fanout.vqec_vision_ai_appl_ftfan_process(tracked, 2, false, events, report).code_ ==
           status_code::invalid_argument);
    assert(report.processed_mask_ == 1U && report.failed_mask_ == 2U &&
           report.first_error_slot_ == 1);
    assert(events[0].events_[0].feature_id_ == "counting" &&
           events[1].events_[0].feature_id_ == "intrusion");

    second_processor.is_invalid_ = false;
    tracked = vqec_vision_ai_ctest_ffct_make_tracked(2);
    assert(fanout.vqec_vision_ai_appl_ftfan_process(tracked, 3, true, events, report).code_ ==
           status_code::ok);
    assert(report.processed_mask_ == 3U && report.failed_mask_ == 0);
    return 0;
}
