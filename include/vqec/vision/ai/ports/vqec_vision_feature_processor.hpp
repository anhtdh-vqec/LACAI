#ifndef VQEC_VISION_AI_PORTS_FEATURE_PROCESSOR_HPP
#define VQEC_VISION_AI_PORTS_FEATURE_PROCESSOR_HPP

#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_feature_event.hpp"

namespace vqec::vision::ai {

class feature_processor_port {
public:
    virtual ~feature_processor_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_ftpro_validate_activation(
        const feature_processor_config& _config) const = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_ftpro_reset_epoch(
        std::uint64_t _source_epoch) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_ftpro_process_observations(
        const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, feature_event_batch& _events) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_FEATURE_PROCESSOR_HPP
