#ifndef VQEC_VISION_AI_LIFEC_RCSNP_RUNTIME_CONTROL_SNAPSHOT_HPP
#define VQEC_VISION_AI_LIFEC_RCSNP_RUNTIME_CONTROL_SNAPSHOT_HPP

#include <istream>
#include <ostream>

#include "vqec/vision/ai/contracts/vqec_vision_app_lifecycle.hpp"

namespace vqec::vision::ai {

[[nodiscard]] status vqec_vision_ai_lifec_rcsnp_load(
    std::istream& _stream, runtime_control_snapshot& _snapshot);
[[nodiscard]] status vqec_vision_ai_lifec_rcsnp_write(
    const runtime_control_snapshot& _snapshot, std::ostream& _stream);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_LIFEC_RCSNP_RUNTIME_CONTROL_SNAPSHOT_HPP
