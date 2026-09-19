#ifndef VQEC_VISION_AI_LIFEC_APENT_APP_ENTITLEMENT_HPP
#define VQEC_VISION_AI_LIFEC_APENT_APP_ENTITLEMENT_HPP

#include <istream>

#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_app_lifecycle.hpp"

namespace vqec::vision::ai {

[[nodiscard]] status vqec_vision_ai_lifec_apent_load(
    std::istream& _stream, app_entitlement_grant& _grant);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_LIFEC_APENT_APP_ENTITLEMENT_HPP
