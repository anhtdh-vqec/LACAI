#include <array>
#include <cassert>
#include <cstdint>

#include "vqec_vision_multi_model_result_router.hpp"

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
        last_gap_ = _is_source_gap;
        ++update_count_;
        _tracked = _detections;
        _tracked.observations_[0].track_id_ = track_id_;
        return {};
    }

    [[nodiscard]] status vqec_vision_ai_ports_trker_reset_epoch(
        std::uint64_t _source_epoch) override {
        epoch_ = _source_epoch;
        ++reset_count_;
        return {};
    }

    std::uint64_t track_id_{0};
    std::uint64_t epoch_{0};
    unsigned reset_count_{0};
    unsigned update_count_{0};
    bool last_gap_{false};
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
    perception_result_stage stage_0(decode_stage_0, tracking_stage_0);
    perception_result_stage stage_1(decode_stage_1, tracking_stage_1);
    const perception_result_config source{4, 1, {640, 360}};
    assert(stage_0.vqec_vision_ai_appl_prstg_configure(source).code_ == status_code::ok);
    assert(stage_1.vqec_vision_ai_appl_prstg_configure(source).code_ == status_code::ok);

    std::array<perception_result_stage*, deployment_limits::g_max_models_per_source>
        stages{};
    stages[0] = &stage_0;
    stages[1] = &stage_1;
    multi_model_result_router router;
    assert(router.vqec_vision_ai_appl_mmrrt_configure(
               {4, 1, {320, 180}}, stages, 2).code_ == status_code::invalid_state);
    assert(router.vqec_vision_ai_appl_mmrrt_configure(source, stages, 2).code_ ==
           status_code::ok);

    std::array<observation_batch, deployment_limits::g_max_models_per_source> tracked{};
    multi_model_result_report report;
    multi_model_pump_report pump_report;
    tensor_result result{500, {}};
    assert(router.vqec_vision_ai_appl_mmrrt_route_result(
               result, pump_report, 1, tracked, report).code_ == status_code::pending);
    assert(!report.has_tracked_ && report.model_slot_ == UINT16_MAX);

    pump_report.has_result_ = true;
    pump_report.result_model_slot_ = 0;
    pump_report.result_ticket_ = {{1, 1}, 2, 1, 100, 500};
    assert(router.vqec_vision_ai_appl_mmrrt_route_result(
               result, pump_report, 10, tracked, report).code_ == status_code::ok);
    assert(report.has_tracked_ && report.model_slot_ == 0 && !report.is_source_gap_);
    assert(tracked[0].frame_.frame_id_ == 1 &&
           tracked[0].observations_[0].track_id_ == 11 && tracker_0.reset_count_ == 1U);

    pump_report.result_ticket_ = {{1, 2}, 2, 3, 300, 700};
    result.pipeline_pts_ns_ = 700;
    assert(router.vqec_vision_ai_appl_mmrrt_route_result(
               result, pump_report, 20, tracked, report).code_ == status_code::ok);
    assert(report.is_source_gap_ && tracker_0.last_gap_ && tracked[0].frame_.frame_id_ == 3);

    pump_report.result_ticket_ = {{1, 3}, 2, 2, 200, 600};
    result.pipeline_pts_ns_ = 600;
    assert(router.vqec_vision_ai_appl_mmrrt_route_result(
               result, pump_report, 21, tracked, report).code_ == status_code::invalid_state);
    assert(!report.has_tracked_ && tracked[0].frame_.frame_id_ == 3 &&
           tracker_0.update_count_ == 2U);

    pump_report.result_model_slot_ = 1;
    pump_report.result_ticket_ = {{2, 1}, 2, 10, 50, 800};
    result.pipeline_pts_ns_ = 800;
    assert(router.vqec_vision_ai_appl_mmrrt_route_result(
               result, pump_report, 15, tracked, report).code_ == status_code::ok);
    assert(!report.is_source_gap_ && tracked[1].frame_.frame_id_ == 10 &&
           tracked[1].observations_[0].track_id_ == 22 && tracker_1.reset_count_ == 1U);

    pump_report.result_ticket_ = {{2, 2}, 2, 11, 60, 900};
    result.pipeline_pts_ns_ = 901;
    assert(router.vqec_vision_ai_appl_mmrrt_route_result(
               result, pump_report, 16, tracked, report).code_ ==
           status_code::invalid_argument);
    assert(tracked[1].frame_.frame_id_ == 10 && tracker_1.update_count_ == 1U);
    result.pipeline_pts_ns_ = 900;
    assert(router.vqec_vision_ai_appl_mmrrt_route_result(
               result, pump_report, 16, tracked, report).code_ == status_code::ok);
    assert(!report.is_source_gap_ && tracked[1].frame_.frame_id_ == 11);

    pump_report.result_model_slot_ = 0;
    pump_report.result_ticket_ = {{1, 4}, 3, 1, 10, 1000};
    result.pipeline_pts_ns_ = 1000;
    assert(router.vqec_vision_ai_appl_mmrrt_route_result(
               result, pump_report, 30, tracked, report).code_ == status_code::ok);
    assert(!report.is_source_gap_ && tracker_0.reset_count_ == 2U &&
           tracker_0.epoch_ == 3);

    return 0;
}
