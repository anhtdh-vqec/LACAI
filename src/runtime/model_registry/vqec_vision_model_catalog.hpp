#ifndef VQEC_VISION_AI_MODEL_REGISTRY_MODEL_CATALOG_HPP
#define VQEC_VISION_AI_MODEL_REGISTRY_MODEL_CATALOG_HPP

#include <cstddef>
#include <cstdint>
#include <istream>

#include "vqec/vision/ai/contracts/vqec_vision_model_catalog.hpp"

namespace vqec::vision::ai {

namespace model_catalog_document_limits {
inline constexpr std::size_t g_max_document_bytes = 512U * 1024U;
inline constexpr int g_max_json_depth = 16;
}  // namespace model_catalog_document_limits

// Startup-only strict JSON load. Caller authenticates and opens the file.
// Failure preserves both outputs; parse success does not authenticate artifacts.
[[nodiscard]] status vqec_vision_ai_mreg_mdcat_load_catalog(
    std::istream& _stream, model_catalog& _catalog,
    std::uint64_t& _declared_resident_bytes);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_MODEL_REGISTRY_MODEL_CATALOG_HPP
