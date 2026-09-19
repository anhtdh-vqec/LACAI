#ifndef VQEC_VISION_AI_LIFEC_APP_CATALOG_HPP
#define VQEC_VISION_AI_LIFEC_APP_CATALOG_HPP

#include <istream>

#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_app_lifecycle.hpp"

namespace vqec::vision::ai {

[[nodiscard]] status vqec_vision_ai_lifec_apcat_load(
    std::istream& _stream, usecase_app_catalog& _catalog);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_LIFEC_APP_CATALOG_HPP
