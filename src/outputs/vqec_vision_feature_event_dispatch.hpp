#ifndef VQEC_VISION_AI_OUTPT_FEATURE_EVENT_DISPATCH_HPP
#define VQEC_VISION_AI_OUTPT_FEATURE_EVENT_DISPATCH_HPP

#include <cstddef>
#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_output_gate.hpp"
#include "vqec/vision/ai/ports/vqec_vision_feature_event_sink.hpp"

namespace vqec::vision::ai {

[[nodiscard]] status vqec_vision_ai_outpt_ftdsp_dispatch_event(
    const feature_event_batch& _batch, std::size_t _event_index,
    const feature_processor_config& _config, std::uint64_t _policy_revision,
    std::uint64_t _steady_now_ns, output_gate& _gate, feature_event_sink_port& _sink);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_OUTPT_FEATURE_EVENT_DISPATCH_HPP
