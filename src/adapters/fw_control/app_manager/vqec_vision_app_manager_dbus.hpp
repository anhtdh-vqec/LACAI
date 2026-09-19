#ifndef VQEC_VISION_AI_FWCTL_AMDBS_APP_MANAGER_DBUS_HPP
#define VQEC_VISION_AI_FWCTL_AMDBS_APP_MANAGER_DBUS_HPP

#include <cstddef>
#include <memory>
#include <string>

#include "vqec/vision/ai/ports/management/vqec_vision_app_manager.hpp"

namespace vqec::vision::ai {

namespace app_manager_dbus_protocol {
inline constexpr char g_interface_name[] = "com.vqec.AiVision.AppManager1";
inline constexpr char g_submit_install_method[] = "SubmitInstall";
inline constexpr char g_submit_update_method[] = "SubmitUpdate";
inline constexpr char g_submit_rollback_method[] = "SubmitRollback";
inline constexpr char g_submit_configuration_method[] = "SubmitConfiguration";
inline constexpr char g_submit_entitlement_method[] = "SubmitEntitlement";
inline constexpr char g_submit_desired_method[] = "SubmitDesired";
inline constexpr char g_submit_uninstall_method[] = "SubmitUninstall";
inline constexpr char g_get_operation_method[] = "GetOperation";
inline constexpr char g_cancel_operation_method[] = "CancelOperation";
inline constexpr char g_snapshot_method[] = "GetSnapshot";
inline constexpr char g_list_applications_method[] = "ListApplications";
}  // namespace app_manager_dbus_protocol

struct app_manager_dbus_config {
    std::string service_bus_name_;
    std::string object_path_;
    std::string trusted_backend_bus_name_;
    std::string trusted_runtime_bus_name_;
    int rpc_timeout_ms_{0};
    std::size_t max_callbacks_per_poll_{0};
    bool use_session_bus_{false};
};

struct app_manager_dbus_client_config {
    std::string service_bus_name_;
    std::string client_bus_name_;
    std::string object_path_;
    int rpc_timeout_ms_{0};
    bool use_session_bus_{false};
};

class app_manager_dbus_client final {
public:
    app_manager_dbus_client();
    ~app_manager_dbus_client() noexcept;
    app_manager_dbus_client(const app_manager_dbus_client&) = delete;
    app_manager_dbus_client& operator=(const app_manager_dbus_client&) = delete;

    [[nodiscard]] status vqec_vision_ai_fwctl_amdbs_fetch_snapshot(
        const app_manager_dbus_client_config& _config,
        runtime_control_snapshot& _snapshot);
    [[nodiscard]] status vqec_vision_ai_fwctl_amdbs_list_applications(
        const app_manager_dbus_client_config& _config,
        const std::string& _source_id,
        std::vector<app_catalog_status>& _applications);
    [[nodiscard]] status vqec_vision_ai_fwctl_amdbs_submit_install(
        const app_manager_dbus_client_config& _config,
        const app_operation_request& _operation_request,
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision,
        std::string& _operation_id);
    [[nodiscard]] status vqec_vision_ai_fwctl_amdbs_submit_update(
        const app_manager_dbus_client_config& _config,
        const app_operation_request& _operation_request,
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision,
        std::string& _operation_id);
    [[nodiscard]] status vqec_vision_ai_fwctl_amdbs_submit_rollback(
        const app_manager_dbus_client_config& _config,
        const app_operation_request& _operation_request,
        std::uint64_t _expected_inventory_revision,
        std::string& _operation_id);
    [[nodiscard]] status vqec_vision_ai_fwctl_amdbs_submit_configuration(
        const app_manager_dbus_client_config& _config,
        const app_operation_request& _operation_request,
        std::uint64_t _expected_configuration_revision,
        const std::vector<std::uint8_t>& _configuration_payload,
        const std::string& _configuration_sha256,
        std::string& _operation_id);
    [[nodiscard]] status vqec_vision_ai_fwctl_amdbs_submit_entitlement(
        const app_manager_dbus_client_config& _config,
        const app_operation_request& _operation_request,
        const app_entitlement_candidate& _candidate,
        std::string& _operation_id);
    [[nodiscard]] status vqec_vision_ai_fwctl_amdbs_submit_desired(
        const app_manager_dbus_client_config& _config,
        const app_operation_request& _operation_request,
        const app_desired_update& _update,
        std::string& _operation_id);
    [[nodiscard]] status vqec_vision_ai_fwctl_amdbs_submit_uninstall(
        const app_manager_dbus_client_config& _config,
        const app_operation_request& _operation_request,
        std::uint64_t _expected_inventory_revision,
        std::string& _operation_id);
    [[nodiscard]] status vqec_vision_ai_fwctl_amdbs_get_operation(
        const app_manager_dbus_client_config& _config,
        const std::string& _operation_id,
        app_operation_record& _operation);
    [[nodiscard]] status vqec_vision_ai_fwctl_amdbs_cancel_operation(
        const app_manager_dbus_client_config& _config,
        const std::string& _operation_id,
        app_operation_record& _operation);

private:
    [[nodiscard]] status vqec_vision_ai_fwctl_amdbs_commit_package(
        const app_manager_dbus_client_config& _config,
        const app_package_candidate& _candidate,
        std::uint64_t _expected_inventory_revision,
        const char* _method_name,
        const app_operation_request& _operation_request,
        std::string& _operation_id);
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

class app_manager_dbus_server final {
public:
    app_manager_dbus_server();
    ~app_manager_dbus_server() noexcept;
    app_manager_dbus_server(const app_manager_dbus_server&) = delete;
    app_manager_dbus_server& operator=(const app_manager_dbus_server&) = delete;

    [[nodiscard]] status vqec_vision_ai_fwctl_amdbs_open(
        app_manager_port& _port, const app_manager_dbus_config& _config);
    void vqec_vision_ai_fwctl_amdbs_poll() noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_FWCTL_AMDBS_APP_MANAGER_DBUS_HPP
