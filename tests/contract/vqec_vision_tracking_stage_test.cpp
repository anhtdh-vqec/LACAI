#include <cassert>
#include <stdexcept>

#include "vqec_vision_tracking_stage.hpp"

using namespace vqec::vision::ai;

namespace {

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
        if (throw_on_update_) {
            throw std::runtime_error("fake tracker failure");
        }
        _tracked = _detections;
        for (auto& observation : _tracked.observations_) {
            observation.track_id_ = return_invalid_track_ ? 0 : next_track_id_++;
        }
        return {};
    }

    [[nodiscard]] status vqec_vision_ai_ports_trker_reset_epoch(
        std::uint64_t _source_epoch) override {
        ++reset_count_;
        return _source_epoch == 0 ?
            status{status_code::invalid_argument, "epoch is zero"} : status{};
    }

    std::uint64_t next_track_id_{1};
    unsigned reset_count_{0};
    bool return_invalid_track_{false};
    bool throw_on_update_{false};
};

observation_batch vqec_vision_ai_ctest_tstgt_make_detections(std::uint64_t _epoch) {
    const preview_frame_key frame{0, 0, _epoch, 5, 10};
    return {frame, {640, 360},
            {{frame, 0, "person", {1, 2, 3, 4, 0xffffffffU, "person"}, 0.9F,
              observation_quality::high, {}}}};
}

}  // namespace

int main() {
    fake_tracker tracker;
    tracking_stage stage(tracker);
    assert(stage.vqec_vision_ai_track_trkst_validate_activation().code_ == status_code::ok);
    auto detections = vqec_vision_ai_ctest_tstgt_make_detections(1);
    observation_batch tracked;
    assert(stage.vqec_vision_ai_track_trkst_process(detections, 10, false, tracked).code_ ==
           status_code::ok);
    assert(tracker.reset_count_ == 1U);
    assert(tracked.observations_[0].track_id_ == 1U);

    const auto prior_track = tracked.observations_[0].track_id_;
    tracker.return_invalid_track_ = true;
    assert(stage.vqec_vision_ai_track_trkst_process(detections, 11, false, tracked).code_ ==
           status_code::invalid_argument);
    assert(stage.vqec_vision_ai_track_trkst_is_faulted());
    assert(tracked.observations_[0].track_id_ == prior_track);
    assert(stage.vqec_vision_ai_track_trkst_process(detections, 12, false, tracked).code_ ==
           status_code::invalid_state);

    tracker.return_invalid_track_ = false;
    detections = vqec_vision_ai_ctest_tstgt_make_detections(2);
    assert(stage.vqec_vision_ai_track_trkst_process(detections, 13, true, tracked).code_ ==
           status_code::ok);
    assert(!stage.vqec_vision_ai_track_trkst_is_faulted());
    assert(tracker.reset_count_ == 2U);

    tracker.throw_on_update_ = true;
    assert(stage.vqec_vision_ai_track_trkst_process(detections, 14, false, tracked).code_ ==
           status_code::io_error);
    assert(stage.vqec_vision_ai_track_trkst_is_faulted());
    assert(tracked.frame_.source_epoch_ == 2);
    tracker.throw_on_update_ = false;
    detections = vqec_vision_ai_ctest_tstgt_make_detections(3);
    assert(stage.vqec_vision_ai_track_trkst_process(detections, 15, false, tracked).code_ ==
           status_code::ok);
    assert(tracker.reset_count_ == 3U);
    return 0;
}
