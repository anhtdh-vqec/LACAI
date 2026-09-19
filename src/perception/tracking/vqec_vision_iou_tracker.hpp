#ifndef VQEC_VISION_AI_PERCEPTION_TRACKING_IOU_TRACKER_HPP
#define VQEC_VISION_AI_PERCEPTION_TRACKING_IOU_TRACKER_HPP

#include <array>
#include <cstdint>

#include "vqec/vision/ai/ports/perception/vqec_vision_tracker.hpp"

namespace vqec::vision::ai {

namespace iou_tracker_limits {
inline constexpr std::uint32_t g_max_tracks = 256;
inline constexpr float g_min_iou_threshold = 0.01F;
inline constexpr float g_max_iou_threshold = 0.99F;
}  // namespace iou_tracker_limits

struct iou_tracker_config {
    float iou_threshold_{0.3F};
    std::uint32_t max_lost_frames_{30};
    std::uint32_t max_tracks_{iou_tracker_limits::g_max_tracks};
};

// Portable bounded baseline tracker for the explicit portable.iou.tracker.v1 contract.
// It is deterministic and per binding; it makes no claim to ByteTrack/MOT equivalence.
class iou_tracker final : public tracker_port {
public:
    iou_tracker() = default;
    explicit iou_tracker(iou_tracker_config _config) noexcept;

    [[nodiscard]] status vqec_vision_ai_ports_trker_validate_activation() const override;
    [[nodiscard]] status vqec_vision_ai_ports_trker_update_tracks(
        const observation_batch& _detections, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, observation_batch& _tracked) override;
    [[nodiscard]] status vqec_vision_ai_ports_trker_reset_epoch(
        std::uint64_t _source_epoch) override;

private:
    struct track_state {
        std::uint64_t track_id_{0};
        overlay_box box_;
        std::uint32_t lost_frames_{0};
    };

    [[nodiscard]] status vqec_vision_ai_track_ioutr_validate() const;
    void vqec_vision_ai_track_ioutr_compact();

    iou_tracker_config config_;
    std::array<track_state, iou_tracker_limits::g_max_tracks> tracks_{};
    std::uint16_t track_count_{0};
    std::uint64_t next_track_id_{1};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PERCEPTION_TRACKING_IOU_TRACKER_HPP
