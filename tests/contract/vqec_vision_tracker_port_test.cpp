#include <cassert>

#include "vqec/vision/ai/ports/vqec_vision_tracker.hpp"

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
        if (_now_monotonic_ns < last_now_ns_ || _detections.frame_.source_epoch_ != epoch_) {
            return {status_code::invalid_state, "tracker epoch or clock mismatch"};
        }
        last_now_ns_ = _now_monotonic_ns;
        _tracked = _detections;
        for (auto& observation : _tracked.observations_) {
            observation.track_id_ = _is_source_gap ? 2 : 1;
        }
        return {};
    }

    [[nodiscard]] status vqec_vision_ai_ports_trker_reset_epoch(
        std::uint64_t _source_epoch) override {
        if (_source_epoch == 0) {
            return {status_code::invalid_argument, "tracker epoch is zero"};
        }
        epoch_ = _source_epoch;
        last_now_ns_ = 0;
        return {};
    }

private:
    std::uint64_t epoch_{0};
    std::uint64_t last_now_ns_{0};
};

}  // namespace

int main() {
    fake_tracker tracker;
    assert(tracker.vqec_vision_ai_ports_trker_validate_activation().code_ == status_code::ok);
    assert(tracker.vqec_vision_ai_ports_trker_reset_epoch(0).code_ ==
           status_code::invalid_argument);
    assert(tracker.vqec_vision_ai_ports_trker_reset_epoch(4).code_ == status_code::ok);
    const preview_frame_key frame{0, 0, 4, 10, 20};
    const preview_geometry geometry{640, 360};
    observation_batch detections{
        frame, geometry,
        {{frame, 0, "person", {10, 10, 20, 20, 0xffffffffU, "person"}, 0.9F,
          observation_quality::high, {}}}};
    observation_batch tracked;
    assert(tracker.vqec_vision_ai_ports_trker_update_tracks(
               detections, 100, false, tracked).code_ == status_code::ok);
    assert(tracked.observations_[0].track_id_ == 1);
    assert(tracker.vqec_vision_ai_ports_trker_update_tracks(
               detections, 90, false, tracked).code_ == status_code::invalid_state);
    assert(tracker.vqec_vision_ai_ports_trker_update_tracks(
               detections, 110, true, tracked).code_ == status_code::ok);
    assert(tracked.observations_[0].track_id_ == 2);
    return 0;
}
