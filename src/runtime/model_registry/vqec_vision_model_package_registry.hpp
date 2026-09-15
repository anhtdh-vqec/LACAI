#ifndef VQEC_VISION_AI_MODEL_REGISTRY_MODEL_PACKAGE_REGISTRY_HPP
#define VQEC_VISION_AI_MODEL_REGISTRY_MODEL_PACKAGE_REGISTRY_HPP

#include <cstddef>
#include <istream>

#include "vqec/vision/ai/contracts/vqec_vision_model_package_registry.hpp"

namespace vqec::vision::ai {

namespace model_package_registry_document_limits {
inline constexpr std::size_t g_max_document_bytes = 256U * 1024U;
inline constexpr int g_max_json_depth = 12;
}  // namespace model_package_registry_document_limits

// Startup-only strict JSON load. The caller authenticates and opens the document.
// Failure preserves _registry; loading paths does not authenticate their contents.
[[nodiscard]] status vqec_vision_ai_mreg_mprld_load_registry(
    std::istream& _stream, model_package_registry& _registry);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_MODEL_REGISTRY_MODEL_PACKAGE_REGISTRY_HPP
