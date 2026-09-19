#ifndef VQEC_VISION_AI_APPL_APP_CONFIGURATION_REGISTRY_HPP
#define VQEC_VISION_AI_APPL_APP_CONFIGURATION_REGISTRY_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_app_lifecycle.hpp"
#include "vqec/vision/ai/ports/management/vqec_vision_app_configuration.hpp"

namespace vqec::vision::ai {

namespace app_configuration_registry_limits {
inline constexpr std::size_t g_max_entries = app_lifecycle_limits::g_max_applications;
}  // namespace app_configuration_registry_limits

class app_configuration_registry final {
public:
    [[nodiscard]] status vqec_vision_ai_appl_apcrg_register(
        const std::string& _app_id, const std::string& _schema_id,
        const std::string& _processor_contract,
        app_configuration_validator_port& _validator);
    [[nodiscard]] status vqec_vision_ai_appl_apcrg_validate(
        const std::string& _app_id, const std::string& _schema_id,
        std::uint64_t _revision, const std::vector<std::uint8_t>& _payload) const;
    [[nodiscard]] bool vqec_vision_ai_appl_apcrg_supports_manifest(
        const usecase_app_manifest& _manifest) const noexcept;

private:
    struct entry {
        std::string app_id_;
        std::string schema_id_;
        std::string processor_contract_;
        app_configuration_validator_port* validator_{nullptr};
    };
    std::array<entry, app_configuration_registry_limits::g_max_entries> entries_{};
    std::size_t count_{0};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_APP_CONFIGURATION_REGISTRY_HPP

