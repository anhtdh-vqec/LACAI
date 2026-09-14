#include "vqec_vision_reference_tracker.hpp"

#include <utility>

namespace vqec::vision::ai {
namespace {

float vqec_vision_ai_refer_rftrk_iou(const overlay_box& _left, const overlay_box& _right) {
    if (_left.width_ <= 0.0F || _left.height_ <= 0.0F || _right.width_ <= 0.0F ||
        _right.height_ <= 0.0F) {
        return 0.0F;
    }
    const float left_x2 = _left.x_ + _left.width_;
    const float left_y2 = _left.y_ + _left.height_;
    const float right_x2 = _right.x_ + _right.width_;
    const float right_y2 = _right.y_ + _right.height_;
    const float inter_x1 = _left.x_ > _right.x_ ? _left.x_ : _right.x_;
    const float inter_y1 = _left.y_ > _right.y_ ? _left.y_ : _right.y_;
    const float inter_x2 = left_x2 < right_x2 ? left_x2 : right_x2;
    const float inter_y2 = left_y2 < right_y2 ? left_y2 : right_y2;
    const float inter_w = inter_x2 - inter_x1;
    const float inter_h = inter_y2 - inter_y1;
    if (inter_w <= 0.0F || inter_h <= 0.0F) {
        return 0.0F;
    }
    const float intersection = inter_w * inter_h;
    const float union_area =
        _left.width_ * _left.height_ + _right.width_ * _right.height_ - intersection;
    return union_area > 0.0F ? intersection / union_area : 0.0F;
}

}  // namespace

reference_tracker::reference_tracker(reference_tracker_config _config) noexcept
    : config_(_config) {}

status reference_tracker::vqec_vision_ai_refer_rftrk_validate() const {
    if (config_.iou_threshold_ < reference_tracker_limits::g_min_iou_threshold ||
        config_.iou_threshold_ > reference_tracker_limits::g_max_iou_threshold) {
        return {status_code::invalid_argument, "tracker IoU threshold is out of range"};
    }
    if (config_.max_lost_frames_ == 0 ||
        config_.max_tracks_ == 0 ||
        config_.max_tracks_ > reference_tracker_limits::g_max_tracks) {
        return {status_code::invalid_argument, "tracker track/lost bounds are invalid"};
    }
    return {};
}

status reference_tracker::vqec_vision_ai_ports_trker_validate_activation() const {
    return vqec_vision_ai_refer_rftrk_validate();
}

void reference_tracker::vqec_vision_ai_refer_rftrk_compact() {
    std::size_t write = 0;
    for (std::size_t read = 0; read < track_count_; ++read) {
        if (tracks_[read].lost_frames_ <= config_.max_lost_frames_) {
            if (write != read) {
                tracks_[write] = tracks_[read];
            }
            ++write;
        }
    }
    track_count_ = static_cast<std::uint16_t>(write);
}

status reference_tracker::vqec_vision_ai_ports_trker_update_tracks(
    const observation_batch& _detections, std::uint64_t _now_monotonic_ns,
    bool _is_source_gap, observation_batch& _tracked) {
    (void)_now_monotonic_ns;
    const auto valid = vqec_vision_ai_refer_rftrk_validate();
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (_detections.observations_.size() > config_.max_tracks_) {
        return {status_code::resource_exhausted, "tracker detection count exceeds track bound"};
    }
    std::array<bool, reference_tracker_limits::g_max_tracks> matched{};
    observation_batch candidate = _detections;
    if (_is_source_gap) {
        // A gap ages every live track once. Detections still associate below so no
        // output observation is left untracked, which the tracking stage rejects.
        for (std::size_t index = 0; index < track_count_; ++index) {
            ++tracks_[index].lost_frames_;
        }
    }
    for (std::size_t index = 0; index < candidate.observations_.size(); ++index) {
        auto& item = candidate.observations_[index];
        float best_iou = config_.iou_threshold_;
        std::size_t best_track = reference_tracker_limits::g_max_tracks;
        for (std::size_t track = 0; track < track_count_; ++track) {
            if (matched[track]) {
                continue;
            }
            const float iou = vqec_vision_ai_refer_rftrk_iou(item.box_, tracks_[track].box_);
            if (iou > best_iou) {
                best_iou = iou;
                best_track = track;
            }
        }
        if (best_track < reference_tracker_limits::g_max_tracks) {
            matched[best_track] = true;
            tracks_[best_track].lost_frames_ = 0;
            tracks_[best_track].box_ = item.box_;
            item.track_id_ = tracks_[best_track].track_id_;
            continue;
        }
        if (track_count_ >= config_.max_tracks_) {
            // Fault the caller; the tracking stage faults the epoch and requires a reset.
            return {status_code::resource_exhausted, "tracker has no capacity for a new track"};
        }
        track_state created;
        created.track_id_ = next_track_id_++;
        created.box_ = item.box_;
        created.lost_frames_ = 0;
        matched[track_count_] = true;
        tracks_[track_count_] = created;
        ++track_count_;
        item.track_id_ = created.track_id_;
    }
    for (std::size_t track = 0; track < track_count_; ++track) {
        if (!matched[track] && !_is_source_gap) {
            ++tracks_[track].lost_frames_;
        }
    }
    vqec_vision_ai_refer_rftrk_compact();
    _tracked = std::move(candidate);
    return {};
}

status reference_tracker::vqec_vision_ai_ports_trker_reset_epoch(
    std::uint64_t _source_epoch) {
    if (_source_epoch == 0) {
        return {status_code::invalid_argument, "source epoch must be nonzero"};
    }
    track_count_ = 0;
    next_track_id_ = 1;
    return {};
}

}  // namespace vqec::vision::ai
