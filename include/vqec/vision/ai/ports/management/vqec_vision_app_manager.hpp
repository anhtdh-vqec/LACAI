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
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_get_snapshot(
        runtime_control_snapshot& _snapshot) const = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_list_applications(
        const std::string& _source_id,
        std::vector<app_catalog_status>& _applications) const = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_submit_install(
        const app_operation_request& _operation_request,
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision,
        app_operation_record& _operation) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_submit_update(
        const app_operation_request& _operation_request,
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision,
        app_operation_record& _operation) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_submit_rollback(
        const app_operation_request& _operation_request,
        std::uint64_t _expected_inventory_revision,
        app_operation_record& _operation) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_submit_configuration(
        const app_operation_request& _operation_request,
        std::uint64_t _expected_configuration_revision,
        const std::vector<std::uint8_t>& _configuration_payload,
        const std::string& _configuration_sha256,
        app_operation_record& _operation) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_submit_entitlement(
        const app_operation_request& _operation_request,
        const app_entitlement_candidate& _candidate,
        app_operation_record& _operation) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_submit_desired(
        const app_operation_request& _operation_request,
        const app_desired_update& _update,
        app_operation_record& _operation) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_submit_uninstall(
        const app_operation_request& _operation_request,
        std::uint64_t _expected_inventory_revision,
        app_operation_record& _operation) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_get_operation(
        const std::string& _operation_id,
        app_operation_record& _operation) const = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apmgr_cancel_operation(
        const std::string& _operation_id,
        app_operation_record& _operation) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_APP_MANAGER_HPP
