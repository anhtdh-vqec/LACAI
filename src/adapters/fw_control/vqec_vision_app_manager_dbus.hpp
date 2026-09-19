#ifndef VQEC_VISION_AI_FWCTL_AMDBS_APP_MANAGER_DBUS_HPP
#define VQEC_VISION_AI_FWCTL_AMDBS_APP_MANAGER_DBUS_HPP

#include <cstddef>
#include <memory>
#include <string>

#include "vqec/vision/ai/ports/vqec_vision_app_manager.hpp"

namespace vqec::vision::ai {

namespace app_manager_dbus_protocol {
inline constexpr char g_interface_name[] = "com.vqec.AiVision.AppManager1";
inline constexpr char g_install_method[] = "Install";
inline constexpr char g_configuration_method[] = "ApplyConfiguration";
inline constexpr char g_desired_method[] = "SetDesired";
inline constexpr char g_uninstall_method[] = "Uninstall";
inline constexpr char g_snapshot_method[] = "GetSnapshot";
}  // namespace app_manager_dbus_protocol

struct app_manager_dbus_config {
    std::string service_bus_name_;
    std::string object_path_;
    std::string trusted_peer_bus_name_;
    int rpc_timeout_ms_{0};
    std::size_t max_callbacks_per_poll_{0};
    bool use_session_bus_{false};
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
