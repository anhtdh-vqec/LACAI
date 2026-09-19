#ifndef VQEC_VISION_AI_PORTS_APP_MANAGER_HPP
#define VQEC_VISION_AI_PORTS_APP_MANAGER_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/ports/management/vqec_vision_app_inventory.hpp"
#include "vqec/vision/ai/ports/management/vqec_vision_app_entitlement_verifier.hpp"
#include "vqec/vision/ai/ports/management/vqec_vision_app_package_verifier.hpp"

namespace vqec::vision::ai {

class app_manager_port {
public:
    virtual ~app_manager_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_install(
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision,
        runtime_control_snapshot& _snapshot) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_update(
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision,
        runtime_control_snapshot& _snapshot) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_rollback(
        const std::string& _app_id, std::uint64_t _expected_inventory_revision,
        runtime_control_snapshot& _snapshot) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_update_configuration(
        const std::string& _app_id, std::uint64_t _expected_configuration_revision,
        const std::vector<std::uint8_t>& _configuration_payload,
        const std::string& _configuration_sha256,
        runtime_control_snapshot& _snapshot) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_apply_entitlement(
        const app_entitlement_candidate& _candidate,
        runtime_control_snapshot& _snapshot) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_set_desired(
        const app_desired_update& _update,
        runtime_control_snapshot& _snapshot) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_uninstall(
        const std::string& _app_id, std::uint64_t _expected_inventory_revision,
        runtime_control_snapshot& _snapshot) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_get_snapshot(
        runtime_control_snapshot& _snapshot) const = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_APP_MANAGER_HPP
