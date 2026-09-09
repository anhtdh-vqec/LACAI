#include <array>
#include <cassert>
#include <cstdint>
#include <utility>

#include "vqec_vision_multi_model_feature_pipeline.hpp"

using namespace vqec::vision::ai;

namespace {

class fake_decoder final : public model_decoder_port {
public:
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const override {
        (void)_outputs;
        return {};
    }

    [[nodiscard]] status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) override {
        (void)_result;
        _observations = {_expected_frame, {640, 360},
                         {{_expected_frame, 0, "person",
                           {1, 2, 3, 4, 0xffffffffU, "person"}, 0.9F,
                           observation_quality::high, {}}}};
        return {};
    }
};

class fake_tracker final : public tracker_port {
public:
    explicit fake_tracker(std::uint64_t _track_id) noexcept : track_id_(_track_id) {}

    [[nodiscard]] status vqec_vision_ai_ports_trker_validate_activation()
        const override {
        return {};
    }

    [[nodiscard]] status vqec_vision_ai_ports_trker_update_tracks(
        const observation_batch& _detections, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, observation_batch& _tracked) override {
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        _tracked = _detections;
        _tracked.observations_[0].track_id_ = track_id_;
        return {};
    }

    [[nodiscard]] status vqec_vision_ai_ports_trker_reset_epoch(
        std::uint64_t _source_epoch) override {
        epoch_ = _source_epoch;
        return {};
    }

    std::uint64_t track_id_{0};
    std::uint64_t epoch_{0};
};

class fake_feature_processor final : public feature_processor_port {
public:
    explicit fake_feature_processor(feature_processor_config _config)
        : config_(std::move(_config)) {}

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
        ++process_count_;
        last_gap_ = _is_source_gap;
        feature_event event{_tracked.frame_,
                            config_.source_id_,
                            is_invalid_ ? "invalid" : config_.feature_id_,
                            "counting:event",
                            "feature.snapshot",
                            "1",
                            feature_event_kind::snapshot,
                            _tracked.frame_.source_pts_ns_,
                            config_.config_revision_,
                            {"person_detector:4"},
                            {_tracked.observations_[0].track_id_},
                            {},
                            {}};
        _events = {_tracked.frame_, _tracked.geometry_, {std::move(event)}};
        return {};
    }

    feature_processor_config config_;
    std::uint64_t epoch_{0};
    unsigned process_count_{0};
    bool last_gap_{false};
    bool is_invalid_{false};
};

}  // namespace

int main() {
    fake_decoder decoder_0;
    fake_decoder decoder_1;
    fake_tracker tracker_0(11);
    fake_tracker tracker_1(22);
    model_decode_stage decode_stage_0(decoder_0, {640, 360});
    model_decode_stage decode_stage_1(decoder_1, {640, 360});
    tracking_stage tracking_stage_0(tracker_0);
    tracking_stage tracking_stage_1(tracker_1);
    perception_result_stage result_stage_0(decode_stage_0, tracking_stage_0);
    perception_result_stage result_stage_1(decode_stage_1, tracking_stage_1);
    const perception_result_config source{4, 1, {640, 360}};
    assert(result_stage_0.vqec_vision_ai_appl_prstg_configure(source).code_ ==
           status_code::ok);
    assert(result_stage_1.vqec_vision_ai_appl_prstg_configure(source).code_ ==
           status_code::ok);

    std::array<perception_result_stage*, deployment_limits::g_max_models_per_source>
        result_stages{};
    result_stages[0] = &result_stage_0;
    result_stages[1] = &result_stage_1;
    multi_model_result_router result_router;
    assert(result_router.vqec_vision_ai_appl_mmrrt_configure(
               source, result_stages, 2).code_ == status_code::ok);

    const feature_processor_config feature_config{
        "source.front", "counting", 4, 4, 8, 8};
    fake_feature_processor processor(feature_config);
    feature_stage stage(processor, feature_config);
    assert(stage.vqec_vision_ai_ftmgr_ftstg_activate().code_ == status_code::ok);
    std::array<feature_stage*, feature_fanout_limits::g_max_feature_stages>
        feature_stages{};
    feature_stages[0] = &stage;
    feature_fanout fanout;
    assert(fanout.vqec_vision_ai_appl_ftfan_configure(feature_stages, 1).code_ ==
           status_code::ok);

    multi_model_feature_pipeline pipeline(result_router);
    std::array<feature_fanout*, deployment_limits::g_max_models_per_source> fanouts{};
    fanouts[0] = &fanout;
    fanouts[1] = &fanout;
    assert(pipeline.vqec_vision_ai_appl_mmfpl_configure(fanouts, 2).code_ ==
           status_code::invalid_argument);
    fanouts[1] = nullptr;
    assert(pipeline.vqec_vision_ai_appl_mmfpl_configure(fanouts, 2).code_ ==
           status_code::ok);

    std::array<observation_batch, deployment_limits::g_max_models_per_source> tracked{};
    std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages> events{};
    multi_model_feature_pipeline_report report;
    multi_model_pump_report pump_report;
    pump_report.has_result_ = true;
    pump_report.result_model_slot_ = 1;
    pump_report.result_ticket_ = {{2, 1}, 1, 1, 100, 500};
    tensor_result result{500, {}};
    assert(pipeline.vqec_vision_ai_appl_mmfpl_process_result(
               result, pump_report, 10, tracked, events, report).code_ == status_code::ok);
    assert(report.result_.has_tracked_ && !report.has_feature_fanout_ &&
           tracked[1].observations_[0].track_id_ == 22 && processor.process_count_ == 0U);

    pump_report.result_model_slot_ = 0;
    pump_report.result_ticket_ = {{1, 1}, 1, 1, 100, 600};
    result.pipeline_pts_ns_ = 600;
    assert(pipeline.vqec_vision_ai_appl_mmfpl_process_result(
               result, pump_report, 10, tracked, events, report).code_ == status_code::ok);
    assert(report.has_feature_fanout_ && report.features_.processed_mask_ == 1U &&
           events[0].events_[0].feature_id_ == "counting" &&
           events[0].events_[0].track_ids_[0] == 11);

    pump_report.result_ticket_ = {{1, 2}, 1, 3, 300, 800};
    result.pipeline_pts_ns_ = 800;
    assert(pipeline.vqec_vision_ai_appl_mmfpl_process_result(
               result, pump_report, 20, tracked, events, report).code_ == status_code::ok);
    assert(report.result_.is_source_gap_ && processor.last_gap_);

    processor.is_invalid_ = true;
    pump_report.result_ticket_ = {{1, 3}, 1, 4, 400, 900};
    result.pipeline_pts_ns_ = 900;
    assert(pipeline.vqec_vision_ai_appl_mmfpl_process_result(
               result, pump_report, 30, tracked, events, report).code_ ==
           status_code::invalid_argument);
    assert(report.result_.has_tracked_ && report.has_feature_fanout_ &&
           report.features_.failed_mask_ == 1U && tracked[0].frame_.frame_id_ == 4 &&
           events[0].frame_.frame_id_ == 3);

    return 0;
}
