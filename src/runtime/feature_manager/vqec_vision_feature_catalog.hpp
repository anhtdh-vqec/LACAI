#ifndef VQEC_VISION_AI_FTMGR_FEATURE_CATALOG_HPP
#define VQEC_VISION_AI_FTMGR_FEATURE_CATALOG_HPP

#include <cstddef>
#include <istream>

#include "vqec/vision/ai/contracts/vqec_vision_feature_catalog.hpp"

namespace vqec::vision::ai {

namespace feature_catalog_document_limits {
inline constexpr std::size_t g_max_document_bytes = 512U * 1024U;
inline constexpr int g_max_json_depth = 16;
}  // namespace feature_catalog_document_limits

// Startup-only strict JSON load. Caller authenticates and opens the stream.
// Failure preserves _catalog; parsing does not authenticate feature packages/config.
[[nodiscard]] status vqec_vision_ai_ftmgr_ftcat_load_catalog(
    std::istream& _stream, feature_catalog& _catalog);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_FTMGR_FEATURE_CATALOG_HPP
