#ifndef VQEC_VISION_AI_FWCTL_USECASE_CONTROL_DBUS_HPP
#define VQEC_VISION_AI_FWCTL_USECASE_CONTROL_DBUS_HPP

#include <cstddef>
#include <memory>
#include <string>

#include "vqec/vision/ai/ports/management/vqec_vision_usecase_control.hpp"

namespace vqec::vision::ai {

namespace usecase_control_dbus_protocol {
inline constexpr char g_interface_name[] = "com.vqec.AiVision.UsecaseControl1";
inline constexpr char g_apply_method[] = "ApplyDesiredPlan";
inline constexpr char g_status_method[] = "GetUsecaseStatus";
inline constexpr char g_capabilities_method[] = "GetCapabilities";
}  // namespace usecase_control_dbus_protocol

struct usecase_control_dbus_config {
    std::string service_bus_name_;
    std::string object_path_;
    std::string trusted_peer_bus_name_;
    int rpc_timeout_ms_{0};
    std::size_t max_callbacks_per_poll_{0};
    bool use_session_bus_{false};
};

class usecase_control_dbus_server final {
public:
    usecase_control_dbus_server();
    ~usecase_control_dbus_server() noexcept;
    usecase_control_dbus_server(const usecase_control_dbus_server& _other) = delete;
    usecase_control_dbus_server& operator=(
        const usecase_control_dbus_server& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_fwctl_ucdbs_open(
        usecase_control_port& _port, const usecase_control_dbus_config& _config);
    void vqec_vision_ai_fwctl_ucdbs_poll() noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_FWCTL_USECASE_CONTROL_DBUS_HPP
