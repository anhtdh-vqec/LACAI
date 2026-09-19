#ifndef VQEC_VISION_AI_APPL_APPMN_APP_MANAGER_HPP
#define VQEC_VISION_AI_APPL_APPMN_APP_MANAGER_HPP

#include <cstdint>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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
        const usecase_app_catalog& _catalog, app_content_store_port& _content_store,
        app_inventory_port& _inventory);
    ~app_manager() noexcept;

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
    [[nodiscard]] status vqec_vision_ai_appl_appmn_list_applications(
        const std::string& _source_id,
        std::vector<app_catalog_status>& _applications) const;
    [[nodiscard]] status vqec_vision_ai_appl_appmn_submit_package(
        const app_operation_request& _operation_request,
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision, bool _is_update,
        app_operation_record& _operation);
    [[nodiscard]] status vqec_vision_ai_appl_appmn_submit_rollback(
        const app_operation_request& _operation_request,
        std::uint64_t _expected_inventory_revision,
        app_operation_record& _operation);

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
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_list_applications(
        const std::string& _source_id,
        std::vector<app_catalog_status>& _applications) const override;
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_submit_install(
        const app_operation_request& _operation_request,
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision,
        app_operation_record& _operation) override;
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_submit_update(
        const app_operation_request& _operation_request,
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision,
        app_operation_record& _operation) override;
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_submit_rollback(
        const app_operation_request& _operation_request,
        std::uint64_t _expected_inventory_revision,
        app_operation_record& _operation) override;
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_get_operation(
        const std::string& _operation_id,
        app_operation_record& _operation) const override;
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_cancel_operation(
        const std::string& _operation_id,
        app_operation_record& _operation) override;

private:
    [[nodiscard]] status vqec_vision_ai_appl_appmn_verify_configuration_digest(
        const std::vector<std::uint8_t>& _payload,
        const std::string& _sha256) const;
    [[nodiscard]] const app_runtime_association*
        vqec_vision_ai_appl_appmn_find_association(
            const runtime_control_snapshot& _snapshot,
            const std::string& _app_id) const noexcept;
    [[nodiscard]] bool vqec_vision_ai_appl_appmn_is_catalogued(
        const std::string& _app_id) const noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_appmn_stage_package_content(
        const app_package_candidate& _candidate,
        const verified_app_package& _package,
        std::vector<app_installed_component>& _components);
    [[nodiscard]] status vqec_vision_ai_appl_appmn_commit_package(
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision, bool _is_update,
        const std::string& _expected_app_id,
        runtime_control_snapshot& _snapshot);
    void vqec_vision_ai_appl_appmn_run_operations() noexcept;

    struct operation_job;

    app_manager_config config_;
    app_package_verifier_port& package_verifier_;
    app_entitlement_verifier_port& entitlement_verifier_;
    app_configuration_registry& configuration_registry_;
    const usecase_app_catalog& catalog_;
    app_content_store_port& content_store_;
    app_inventory_port& inventory_;
    mutable std::mutex mutex_;
    std::mutex queue_mutex_;
    std::condition_variable queue_ready_;
    std::deque<std::unique_ptr<operation_job>> operation_queue_;
    std::thread operation_worker_;
    bool stop_worker_{false};
    bool open_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_APPMN_APP_MANAGER_HPP
