#ifndef VQEC_VISION_AI_CONTRACTS_MODEL_PACKAGE_REGISTRY_HPP
#define VQEC_VISION_AI_CONTRACTS_MODEL_PACKAGE_REGISTRY_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/base/vqec_vision_version_registry.h"

#include "vqec/vision/ai/contracts/inference/vqec_vision_model_catalog.hpp"
#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace model_package_registry_limits {
inline constexpr std::uint32_t g_schema_version = VQEC_VISION_AI_BASELINE_SCHEMA_VERSION;
inline constexpr std::size_t g_max_bindings = 64;
inline constexpr std::size_t g_max_identifier_bytes = 128;
inline constexpr std::size_t g_max_path_bytes = 4096;
}  // namespace model_package_registry_limits

// Cold-path deployment binding. Paths are configuration values and are not evidence of
// artifact authentication; a trusted resolver must separately establish that property.
struct model_package_binding {
    std::string model_id_;
    std::string model_version_;
    std::string target_id_;
    std::string artifact_ref_;
    std::string package_dir_;
    std::string model_library_;
};

struct model_package_registry {
    std::uint32_t schema_version_{0};
    std::vector<model_package_binding> bindings_;
};

// Requires one exact binding for every catalog model and rejects unknown/duplicate entries.
[[nodiscard]] status vqec_vision_ai_core_mprgy_validate_registry(
    const model_package_registry& _registry, const model_catalog& _catalog);

[[nodiscard]] const model_package_binding* vqec_vision_ai_core_mprgy_find_binding(
    const model_package_registry& _registry, const std::string& _model_id) noexcept;

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_MODEL_PACKAGE_REGISTRY_HPP
