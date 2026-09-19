#ifndef VQEC_VISION_AI_STOR_SQLITE_APP_INVENTORY_HPP
#define VQEC_VISION_AI_STOR_SQLITE_APP_INVENTORY_HPP

#include <cstdint>
#include <string>

#include "vqec/vision/ai/ports/management/vqec_vision_app_inventory.hpp"

struct sqlite3;

namespace vqec::vision::ai {

struct sqlite_app_inventory_config {
    std::string database_path_;
    std::uint64_t max_database_bytes_{0};
    int busy_timeout_ms_{0};
};

class sqlite_app_inventory final : public app_inventory_port {
public:
    explicit sqlite_app_inventory(sqlite_app_inventory_config _config);
    ~sqlite_app_inventory() override;
    sqlite_app_inventory(const sqlite_app_inventory&) = delete;
    sqlite_app_inventory& operator=(const sqlite_app_inventory&) = delete;

    [[nodiscard]] status vqec_vision_ai_ports_apinv_open() override;
    [[nodiscard]] status vqec_vision_ai_ports_apinv_install(
        const app_install_request& _request,
        runtime_control_snapshot& _snapshot) override;
    [[nodiscard]] status vqec_vision_ai_ports_apinv_update_configuration(
        const app_configuration_update& _update,
        runtime_control_snapshot& _snapshot) override;
    [[nodiscard]] status vqec_vision_ai_ports_apinv_update_authority(
        const app_authority_update& _update,
        runtime_control_snapshot& _snapshot) override;
    [[nodiscard]] status vqec_vision_ai_ports_apinv_set_desired(
        const app_desired_update& _update,
        runtime_control_snapshot& _snapshot) override;
    [[nodiscard]] status vqec_vision_ai_ports_apinv_uninstall(
        const std::string& _app_id, std::uint64_t _expected_inventory_revision,
        runtime_control_snapshot& _snapshot) override;
    [[nodiscard]] status vqec_vision_ai_ports_apinv_load_snapshot(
        runtime_control_snapshot& _snapshot) const override;

private:
    sqlite_app_inventory_config config_;
    sqlite3* database_{nullptr};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_STOR_SQLITE_APP_INVENTORY_HPP

