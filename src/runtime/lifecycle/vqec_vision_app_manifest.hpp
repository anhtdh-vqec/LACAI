#ifndef VQEC_VISION_AI_LIFEC_APP_MANIFEST_HPP
#define VQEC_VISION_AI_LIFEC_APP_MANIFEST_HPP

#include <istream>

#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_app_lifecycle.hpp"

namespace vqec::vision::ai {

[[nodiscard]] status vqec_vision_ai_lifec_apmft_load(
    std::istream& _stream, usecase_app_manifest& _manifest);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_LIFEC_APP_MANIFEST_HPP

