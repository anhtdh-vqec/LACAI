#ifndef VQEC_VISION_AI_PORTS_TRACKER_HPP
#define VQEC_VISION_AI_PORTS_TRACKER_HPP

#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"

namespace vqec::vision::ai {

// Serialized per-source tracker boundary. Implementations own association state;
// callers own observation memory and must retain the source epoch in every result.
class tracker_port {
public:
    virtual ~tracker_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_trker_validate_activation()
        const = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_trker_update_tracks(
        const observation_batch& _detections, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, observation_batch& _tracked) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_trker_reset_epoch(
        std::uint64_t _source_epoch) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_TRACKER_HPP
