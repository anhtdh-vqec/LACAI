#include <cassert>

#include "vqec_vision_perception_result_stage.hpp"

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
        _tracked.observations_[0].track_id_ = 11;
        return {};
    }

    [[nodiscard]] status vqec_vision_ai_ports_trker_reset_epoch(
        std::uint64_t _source_epoch) override {
        ++reset_count_;
        epoch_ = _source_epoch;
        return {};
    }

    std::uint64_t epoch_{0};
    unsigned reset_count_{0};
};

}  // namespace

int main() {
    fake_decoder decoder;
    fake_tracker tracker;
    model_decode_stage decode_stage(decoder, {640, 360});
    tracking_stage track_stage(tracker);
    perception_result_stage stage(decode_stage, track_stage);
    tensor_result result{500, {}};
    const submission_ticket ticket{{2, 3}, 7, 42, 100, 500};
    observation_batch tracked;
    assert(stage.vqec_vision_ai_appl_prstg_process(
               result, ticket, 1, false, tracked).code_ == status_code::invalid_state);
    assert(stage.vqec_vision_ai_appl_prstg_configure({4, 1, {320, 180}}).code_ ==
           status_code::invalid_state);
    assert(stage.vqec_vision_ai_appl_prstg_configure({4, 1, {640, 360}}).code_ ==
           status_code::ok);
    assert(stage.vqec_vision_ai_appl_prstg_configure({4, 1, {640, 360}}).code_ ==
           status_code::invalid_state);
    assert(stage.vqec_vision_ai_appl_prstg_process(
               result, ticket, 2, false, tracked).code_ == status_code::ok);
    assert(tracked.frame_.camera_id_ == 4 && tracked.frame_.channel_id_ == 1 &&
           tracked.frame_.source_epoch_ == 7 && tracked.frame_.frame_id_ == 42 &&
           tracked.frame_.source_pts_ns_ == 100);
    assert(tracked.observations_[0].track_id_ == 11 && tracker.reset_count_ == 1U);

    const auto prior_frame = tracked.frame_;
    result.pipeline_pts_ns_ = 501;
    assert(stage.vqec_vision_ai_appl_prstg_process(
               result, ticket, 3, false, tracked).code_ == status_code::invalid_argument);
    assert(tracked.frame_.frame_id_ == prior_frame.frame_id_ && tracker.reset_count_ == 1U);
    return 0;
}
