#ifndef VQEC_VISION_AI_RUNTIME_LIFECYCLE_DEPLOYMENT_CONFIG_HPP
#define VQEC_VISION_AI_RUNTIME_LIFECYCLE_DEPLOYMENT_CONFIG_HPP

#include <cstddef>
#include <istream>

#include "vqec/vision/ai/contracts/vqec_vision_deployment_config.hpp"

namespace vqec::vision::ai {

namespace deployment_document_limits {
inline constexpr std::size_t g_max_document_bytes = 128U * 1024U;
inline constexpr int g_max_json_depth = 16;
}  // namespace deployment_document_limits

// Startup-only bounded JSON load. Caller authenticates and opens the file.
// Failure preserves _config; parse success is not signature/RAW-source/model qualification.
[[nodiscard]] status vqec_vision_ai_life_dpcfg_load(
    std::istream& _stream, deployment_config& _config,
    std::uint64_t& _declared_resident_bytes);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_RUNTIME_LIFECYCLE_DEPLOYMENT_CONFIG_HPP
