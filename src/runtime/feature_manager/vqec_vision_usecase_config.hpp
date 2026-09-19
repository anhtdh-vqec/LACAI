#ifndef VQEC_VISION_AI_FTMGR_USECASE_CONFIG_HPP
#define VQEC_VISION_AI_FTMGR_USECASE_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <istream>
#include <vector>

#include "vqec/vision/ai/contracts/features/vqec_vision_usecase_activation.hpp"

namespace vqec::vision::ai {

namespace usecase_config_limits {
inline constexpr std::size_t g_max_document_bytes = 512U * 1024U;
inline constexpr int g_max_json_depth = 16;
}  // namespace usecase_config_limits

struct usecase_control_snapshot {
    std::uint64_t control_revision_{0};
    std::uint64_t entitlement_revision_{0};
    std::uint64_t deployment_revision_{0};
    usecase_catalog catalog_;
    std::vector<usecase_activation_request> requests_;
};

// Startup-only strict loader. The caller authenticates the opened file and entitlement
// provenance. Parsing booleans does not grant entitlement or prove hardware admission.
[[nodiscard]] status vqec_vision_ai_ftmgr_ucfg_load_snapshot(
    std::istream& _stream, usecase_control_snapshot& _snapshot);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_FTMGR_USECASE_CONFIG_HPP
