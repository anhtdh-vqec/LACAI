#ifndef VQEC_VISION_AI_TRACK_TRACKING_STAGE_HPP
#define VQEC_VISION_AI_TRACK_TRACKING_STAGE_HPP

#include "vqec/vision/ai/ports/vqec_vision_tracker.hpp"

namespace vqec::vision::ai {

class tracking_stage final {
public:
    explicit tracking_stage(tracker_port& _tracker) noexcept;
    tracking_stage(const tracking_stage& _other) = delete;
    tracking_stage& operator=(const tracking_stage& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_track_trkst_validate_activation() const;
    [[nodiscard]] status vqec_vision_ai_track_trkst_process(
        const observation_batch& _detections, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, observation_batch& _tracked);
    [[nodiscard]] bool vqec_vision_ai_track_trkst_is_faulted() const noexcept;

private:
    tracker_port& tracker_;
    std::uint64_t source_epoch_{0};
    std::uint64_t last_now_monotonic_ns_{0};
    bool is_faulted_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_TRACK_TRACKING_STAGE_HPP
