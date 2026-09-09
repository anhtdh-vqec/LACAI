#ifndef VQEC_VISION_AI_PORTS_FEATURE_EVENT_SINK_HPP
#define VQEC_VISION_AI_PORTS_FEATURE_EVENT_SINK_HPP

#include "vqec/vision/ai/contracts/vqec_vision_feature_event.hpp"

namespace vqec::vision::ai {

class feature_event_sink_port {
public:
    virtual ~feature_event_sink_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_fesnk_deliver_event(
        const feature_event& _event) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_FEATURE_EVENT_SINK_HPP
