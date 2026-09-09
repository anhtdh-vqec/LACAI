#ifndef VQEC_VISION_AI_OUTPT_OVERLAY_PREPARATION_HPP
#define VQEC_VISION_AI_OUTPT_OVERLAY_PREPARATION_HPP

#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_output_gate.hpp"

namespace vqec::vision::ai {

struct overlay_preparation_context {
    std::string source_id_;
    std::string feature_id_;
    std::uint64_t policy_revision_{0};
    std::uint64_t prepared_monotonic_ns_{0};
    std::vector<std::string> attributes_;
};

// Converts validated observations into neutral overlay metadata only after the
// complete rendered scope has been authorized. No pixel or vendor operation occurs here.
[[nodiscard]] status vqec_vision_ai_outpt_ovrpr_prepare(
    const observation_batch& _observations, const overlay_preparation_context& _context,
    output_gate& _gate, overlay_batch& _overlay);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_OUTPT_OVERLAY_PREPARATION_HPP
