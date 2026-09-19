#ifndef VQEC_VISION_AI_PORTS_APP_INVENTORY_HPP
#define VQEC_VISION_AI_PORTS_APP_INVENTORY_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_app_lifecycle.hpp"

namespace vqec::vision::ai {

struct app_install_request {
    usecase_app_manifest manifest_;
    std::string manifest_sha256_;
    std::uint64_t expected_inventory_revision_{0};
    std::uint64_t configuration_revision_{0};
    std::string configuration_sha256_;
    std::vector<std::uint8_t> configuration_payload_;
    bool supported_{false};
    bool compatible_{false};
    bool admitted_{false};
};

struct app_configuration_update {
    std::string app_id_;
    std::uint64_t expected_configuration_revision_{0};
    std::uint64_t configuration_revision_{0};
    std::string configuration_sha256_;
    std::vector<std::uint8_t> configuration_payload_;
};

struct app_authority_update {
    std::string app_id_;
    std::string source_id_;
    std::uint64_t expected_entitlement_revision_{0};
    bool entitled_{false};
    bool supported_{false};
    bool compatible_{false};
    bool admitted_{false};
    std::uint64_t entitlement_expires_utc_ns_{0};
    std::vector<std::string> output_scopes_;
    std::string reason_code_;
};

struct app_desired_update {
    std::string app_id_;
    std::string source_id_;
    std::uint64_t expected_desired_revision_{0};
    bool desired_{false};
};

class app_inventory_port {
public:
    virtual ~app_inventory_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apinv_open() = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apinv_install(
        const app_install_request& _request, runtime_control_snapshot& _snapshot) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apinv_update_configuration(
        const app_configuration_update& _update,
        runtime_control_snapshot& _snapshot) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apinv_update_authority(
        const app_authority_update& _update,
        runtime_control_snapshot& _snapshot) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apinv_set_desired(
        const app_desired_update& _update,
        runtime_control_snapshot& _snapshot) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apinv_uninstall(
        const std::string& _app_id, std::uint64_t _expected_inventory_revision,
        runtime_control_snapshot& _snapshot) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apinv_load_snapshot(
        runtime_control_snapshot& _snapshot) const = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_APP_INVENTORY_HPP
