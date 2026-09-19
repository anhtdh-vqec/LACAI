#ifndef VQEC_VISION_AI_APPL_APPMN_APP_MANAGER_HPP
#define VQEC_VISION_AI_APPL_APPMN_APP_MANAGER_HPP

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "vqec/vision/ai/ports/management/vqec_vision_app_inventory.hpp"
#include "vqec/vision/ai/ports/management/vqec_vision_app_content_store.hpp"
#include "vqec/vision/ai/ports/management/vqec_vision_app_manager.hpp"
#include "vqec/vision/ai/ports/management/vqec_vision_app_entitlement_verifier.hpp"
#include "vqec/vision/ai/ports/management/vqec_vision_app_package_verifier.hpp"
#include "vqec_vision_app_configuration_registry.hpp"

namespace vqec::vision::ai {

struct app_manager_config {
    std::string target_id_;
    std::string device_id_;
    app_resource_envelope capacity_;
};

class app_manager final : public app_manager_port {
public:
    app_manager(app_manager_config _config, app_package_verifier_port& _package_verifier,
        app_entitlement_verifier_port& _entitlement_verifier,
        app_configuration_registry& _configuration_registry,
        app_content_store_port& _content_store, app_inventory_port& _inventory);

    [[nodiscard]] status vqec_vision_ai_appl_appmn_open(
        runtime_control_snapshot& _snapshot);
    [[nodiscard]] status vqec_vision_ai_appl_appmn_install(
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision,
        runtime_control_snapshot& _snapshot);
    [[nodiscard]] status vqec_vision_ai_appl_appmn_update(
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision,
        runtime_control_snapshot& _snapshot);
    [[nodiscard]] status vqec_vision_ai_appl_appmn_rollback(
        const std::string& _app_id, std::uint64_t _expected_inventory_revision,
        runtime_control_snapshot& _snapshot);
    [[nodiscard]] status vqec_vision_ai_appl_appmn_update_configuration(
        const std::string& _app_id, std::uint64_t _expected_configuration_revision,
        const std::vector<std::uint8_t>& _configuration_payload,
        const std::string& _configuration_sha256,
        runtime_control_snapshot& _snapshot);
    [[nodiscard]] status vqec_vision_ai_appl_appmn_apply_verified_authority(
        const app_authority_update& _update,
        runtime_control_snapshot& _snapshot);
    [[nodiscard]] status vqec_vision_ai_appl_appmn_apply_entitlement(
        const app_entitlement_candidate& _candidate,
        runtime_control_snapshot& _snapshot);
    [[nodiscard]] status vqec_vision_ai_appl_appmn_set_desired(
        const app_desired_update& _update,
        runtime_control_snapshot& _snapshot);
    [[nodiscard]] status vqec_vision_ai_appl_appmn_uninstall(
        const std::string& _app_id, std::uint64_t _expected_inventory_revision,
        runtime_control_snapshot& _snapshot);
    [[nodiscard]] status vqec_vision_ai_appl_appmn_get_snapshot(
        runtime_control_snapshot& _snapshot) const;

    [[nodiscard]] status vqec_vision_ai_ports_apmgr_install(
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision,
        runtime_control_snapshot& _snapshot) override;
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_update(
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision,
        runtime_control_snapshot& _snapshot) override;
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_rollback(
        const std::string& _app_id, std::uint64_t _expected_inventory_revision,
        runtime_control_snapshot& _snapshot) override;
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_update_configuration(
        const std::string& _app_id, std::uint64_t _expected_configuration_revision,
        const std::vector<std::uint8_t>& _configuration_payload,
        const std::string& _configuration_sha256,
        runtime_control_snapshot& _snapshot) override;
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_apply_entitlement(
        const app_entitlement_candidate& _candidate,
        runtime_control_snapshot& _snapshot) override;
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_set_desired(
        const app_desired_update& _update,
        runtime_control_snapshot& _snapshot) override;
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_uninstall(
        const std::string& _app_id, std::uint64_t _expected_inventory_revision,
        runtime_control_snapshot& _snapshot) override;
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_get_snapshot(
        runtime_control_snapshot& _snapshot) const override;

private:
    [[nodiscard]] status vqec_vision_ai_appl_appmn_verify_configuration_digest(
        const std::vector<std::uint8_t>& _payload,
        const std::string& _sha256) const;
    [[nodiscard]] const app_runtime_association*
        vqec_vision_ai_appl_appmn_find_association(
            const runtime_control_snapshot& _snapshot,
            const std::string& _app_id) const noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_appmn_stage_package_content(
        const app_package_candidate& _candidate,
        const verified_app_package& _package,
        std::vector<app_installed_component>& _components);
    [[nodiscard]] status vqec_vision_ai_appl_appmn_commit_package(
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision, bool _is_update,
        runtime_control_snapshot& _snapshot);

    app_manager_config config_;
    app_package_verifier_port& package_verifier_;
    app_entitlement_verifier_port& entitlement_verifier_;
    app_configuration_registry& configuration_registry_;
    app_content_store_port& content_store_;
    app_inventory_port& inventory_;
    mutable std::mutex mutex_;
    bool open_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_APPMN_APP_MANAGER_HPP
