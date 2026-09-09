#include "vqec_vision_tracking_stage.hpp"

#include <utility>

namespace vqec::vision::ai {

tracking_stage::tracking_stage(tracker_port& _tracker) noexcept : tracker_(_tracker) {}

status tracking_stage::vqec_vision_ai_track_trkst_validate_activation() const {
    try {
        return tracker_.vqec_vision_ai_ports_trker_validate_activation();
    } catch (...) {
        return {status_code::io_error, "tracker activation validation raised an exception"};
    }
}

status tracking_stage::vqec_vision_ai_track_trkst_process(
    const observation_batch& _detections, std::uint64_t _now_monotonic_ns,
    bool _is_source_gap, observation_batch& _tracked) {
    const auto valid_detections = vqec_vision_ai_core_obval_validate_detections(
        _detections, _detections.frame_, _detections.geometry_);
    if (valid_detections.code_ != status_code::ok) {
        return valid_detections;
    }
    if (_now_monotonic_ns == UINT64_MAX ||
        _now_monotonic_ns < last_now_monotonic_ns_) {
        return {status_code::invalid_argument, "tracking stage requires monotonic time"};
    }
    last_now_monotonic_ns_ = _now_monotonic_ns;
    const auto incoming_epoch = _detections.frame_.source_epoch_;
    if (incoming_epoch != source_epoch_) {
        const auto reset = tracker_.vqec_vision_ai_ports_trker_reset_epoch(incoming_epoch);
        if (reset.code_ != status_code::ok) {
            is_faulted_ = true;
            return reset;
        }
        source_epoch_ = incoming_epoch;
        is_faulted_ = false;
    } else if (is_faulted_) {
        return {status_code::invalid_state, "tracker is faulted for the current epoch"};
    }
    observation_batch candidate;
    status updated;
    try {
        updated = tracker_.vqec_vision_ai_ports_trker_update_tracks(
            _detections, _now_monotonic_ns, _is_source_gap, candidate);
    } catch (...) {
        is_faulted_ = true;
        return {status_code::io_error, "tracker update raised an exception"};
    }
    if (updated.code_ != status_code::ok) {
        is_faulted_ = true;
        return updated;
    }
    const auto valid_tracks = vqec_vision_ai_core_obval_validate_batch(
        candidate, _detections.frame_, _detections.geometry_);
    if (valid_tracks.code_ != status_code::ok) {
        is_faulted_ = true;
        return valid_tracks;
    }
    _tracked = std::move(candidate);
    return {};
}

bool tracking_stage::vqec_vision_ai_track_trkst_is_faulted() const noexcept {
    return is_faulted_;
}

}  // namespace vqec::vision::ai
