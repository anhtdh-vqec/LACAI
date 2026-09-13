// Device-free tests for the reference IoU tracker: deterministic association, miss aging,
// lost-timeout expiry, source-gap behavior, epoch reset and bounded track capacity.

#include <cstdint>
#include <iostream>
#include <vector>

#include "vqec_vision_reference_tracker.hpp"

using namespace vqec::vision::ai;

namespace {

observation make_observation(float _x, float _y, float _w, float _h) {
    observation item;
    item.frame_.source_epoch_ = 1;
    item.frame_.frame_id_ = 1;
    item.class_id_ = "person";
    item.box_ = {_x, _y, _w, _h, 0xffffffffU, "person"};
    item.confidence_ = 0.9F;
    return item;
}

observation_batch make_batch(const std::vector<observation>& _detections) {
    observation_batch batch;
    batch.frame_.source_epoch_ = 1;
    batch.geometry_ = {640, 480};
    batch.observations_ = _detections;
    return batch;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    reference_tracker_config config;
    config.iou_threshold_ = 0.3F;
    config.max_lost_frames_ = 2;
    config.max_tracks_ = 8;

    // Configuration and epoch validation.
    {
        reference_tracker tracker(config);
        check(tracker.vqec_vision_ai_ports_trker_validate_activation().code_ ==
              status_code::ok);
        check(tracker.vqec_vision_ai_ports_trker_reset_epoch(0).code_ ==
              status_code::invalid_argument);
        check(tracker.vqec_vision_ai_ports_trker_reset_epoch(1).code_ == status_code::ok);
        auto bad = config;
        bad.iou_threshold_ = 0.0F;
        reference_tracker rejected(bad);
        check(rejected.vqec_vision_ai_ports_trker_validate_activation().code_ ==
              status_code::invalid_argument);
        bad = config;
        bad.max_tracks_ = 0;
        reference_tracker rejected_tracks(bad);
        check(rejected_tracks.vqec_vision_ai_ports_trker_validate_activation().code_ ==
              status_code::invalid_argument);
    }

    // Deterministic association across frames keeps IDs stable.
    {
        reference_tracker tracker(config);
        check(tracker.vqec_vision_ai_ports_trker_reset_epoch(1).code_ == status_code::ok);
        observation_batch tracked;
        check(tracker.vqec_vision_ai_ports_trker_update_tracks(
                  make_batch({make_observation(0, 0, 10, 10),
                      make_observation(100, 100, 10, 10)}),
                  0, false, tracked).code_ == status_code::ok);
        check(tracked.observations_.size() == 2 &&
              tracked.observations_[0].track_id_ == 1 &&
              tracked.observations_[1].track_id_ == 2);

        check(tracker.vqec_vision_ai_ports_trker_update_tracks(
                  make_batch({make_observation(1, 1, 10, 10),
                      make_observation(101, 101, 10, 10)}),
                  1, false, tracked).code_ == status_code::ok);
        check(tracked.observations_[0].track_id_ == 1 &&
              tracked.observations_[1].track_id_ == 2);

        // The nearer box must continue track 2, not steal track 1.
        check(tracker.vqec_vision_ai_ports_trker_update_tracks(
                  make_batch({make_observation(99, 99, 10, 10)}),
                  2, false, tracked).code_ == status_code::ok);
        check(tracked.observations_.size() == 1 && tracked.observations_[0].track_id_ == 2);
    }

    // Source gaps age tracks and a lost track expires after the configured window.
    {
        reference_tracker tracker(config);
        check(tracker.vqec_vision_ai_ports_trker_reset_epoch(1).code_ == status_code::ok);
        observation_batch tracked;
        check(tracker.vqec_vision_ai_ports_trker_update_tracks(
                  make_batch({make_observation(0, 0, 10, 10)}),
                  0, false, tracked).code_ == status_code::ok);
        check(tracked.observations_[0].track_id_ == 1);
        const observation_batch empty = make_batch({});
        for (std::uint64_t now = 1; now <= 3; ++now) {
            check(tracker.vqec_vision_ai_ports_trker_update_tracks(
                      empty, now, true, tracked).code_ == status_code::ok);
            check(tracked.observations_.empty());
        }
        // Lost beyond the window: the next detection starts a new track.
        check(tracker.vqec_vision_ai_ports_trker_update_tracks(
                  make_batch({make_observation(0, 0, 10, 10)}),
                  4, false, tracked).code_ == status_code::ok);
        check(tracked.observations_[0].track_id_ == 2);
    }

    // Track capacity is bounded and fails closed instead of silently truncating.
    {
        reference_tracker_config tight = config;
        tight.max_tracks_ = 1;
        reference_tracker tracker(tight);
        check(tracker.vqec_vision_ai_ports_trker_reset_epoch(1).code_ == status_code::ok);
        observation_batch tracked;
        check(tracker.vqec_vision_ai_ports_trker_update_tracks(
                  make_batch({make_observation(0, 0, 10, 10),
                      make_observation(100, 100, 10, 10)}),
                  0, false, tracked).code_ == status_code::resource_exhausted);
    }

    // Epoch reset clears identity so an old epoch never continues a new one.
    {
        reference_tracker tracker(config);
        check(tracker.vqec_vision_ai_ports_trker_reset_epoch(1).code_ == status_code::ok);
        observation_batch tracked;
        check(tracker.vqec_vision_ai_ports_trker_update_tracks(
                  make_batch({make_observation(0, 0, 10, 10)}),
                  0, false, tracked).code_ == status_code::ok);
        check(tracked.observations_[0].track_id_ == 1);
        check(tracker.vqec_vision_ai_ports_trker_reset_epoch(2).code_ == status_code::ok);
        check(tracker.vqec_vision_ai_ports_trker_update_tracks(
                  make_batch({make_observation(0, 0, 10, 10)}),
                  1, false, tracked).code_ == status_code::ok);
        check(tracked.observations_[0].track_id_ == 1);
    }

    std::cout << "reference tracker failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
