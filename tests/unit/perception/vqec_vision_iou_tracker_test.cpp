#include <cstdint>
#include <iostream>

#include "vqec_vision_iou_tracker.hpp"

using namespace vqec::vision::ai;

namespace {

observation_batch vqec_vision_ai_unit_iotst_make_batch(float _x) {
    observation item;
    item.frame_.source_epoch_ = 1;
    item.frame_.frame_id_ = 1;
    item.class_id_ = "person";
    item.box_ = {_x, 0.0F, 10.0F, 10.0F, 0xffffffffU, "person"};
    item.confidence_ = 0.9F;
    observation_batch batch;
    batch.frame_.source_epoch_ = 1;
    batch.geometry_ = {640, 480};
    batch.observations_.push_back(std::move(item));
    return batch;
}

}  // namespace

int main() {
    unsigned failures = 0;
    iou_tracker tracker;
    if (tracker.vqec_vision_ai_ports_trker_validate_activation().code_ != status_code::ok ||
        tracker.vqec_vision_ai_ports_trker_reset_epoch(1).code_ != status_code::ok) {
        ++failures;
    }
    observation_batch tracked;
    if (tracker.vqec_vision_ai_ports_trker_update_tracks(
            vqec_vision_ai_unit_iotst_make_batch(0.0F), 1, false, tracked).code_ !=
            status_code::ok ||
        tracked.observations_.size() != 1 || tracked.observations_[0].track_id_ != 1) {
        ++failures;
    }
    if (tracker.vqec_vision_ai_ports_trker_update_tracks(
            vqec_vision_ai_unit_iotst_make_batch(1.0F), 2, false, tracked).code_ !=
            status_code::ok ||
        tracked.observations_[0].track_id_ != 1) {
        ++failures;
    }
    iou_tracker_config invalid;
    invalid.max_tracks_ = 0;
    iou_tracker rejected(invalid);
    if (rejected.vqec_vision_ai_ports_trker_validate_activation().code_ !=
        status_code::invalid_argument) {
        ++failures;
    }
    std::cout << "portable IoU tracker failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
