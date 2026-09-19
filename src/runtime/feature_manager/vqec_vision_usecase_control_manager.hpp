#ifndef VQEC_VISION_AI_FTMGR_USECASE_CONTROL_MANAGER_HPP
#define VQEC_VISION_AI_FTMGR_USECASE_CONTROL_MANAGER_HPP

#include <cstdint>
#include <vector>

#include "vqec_vision_usecase_config.hpp"
#include "vqec/vision/ai/ports/management/vqec_vision_usecase_control.hpp"

namespace vqec::vision::ai {

// Serialized cold-path owner. The caller authenticates the startup snapshot and is the
// only runtime-generation publisher; D-Bus callbacks may change desired state only.
class usecase_control_manager final : public usecase_control_port {
public:
    [[nodiscard]] status vqec_vision_ai_ftmgr_ucmgr_configure(
        const usecase_control_snapshot& _trusted_snapshot,
        const deployment_config& _base_deployment, const model_catalog& _models);

    [[nodiscard]] status vqec_vision_ai_ports_ucctl_apply_desired_plan(
        const usecase_desired_plan& _plan, usecase_apply_receipt& _receipt) override;
    [[nodiscard]] status vqec_vision_ai_ports_ucctl_get_status(
        usecase_control_status& _status) const override;
    [[nodiscard]] status vqec_vision_ai_ports_ucctl_get_capabilities(
        usecase_capability_snapshot& _capabilities) const override;

    // A pending plan is immutable until the runtime owner publishes or rejects it.
    [[nodiscard]] status vqec_vision_ai_ftmgr_ucmgr_get_pending(
        usecase_control_snapshot& _snapshot, deployment_config& _deployment) const;
    [[nodiscard]] bool vqec_vision_ai_ftmgr_ucmgr_has_pending() const noexcept {
        return has_pending_;
    }
    [[nodiscard]] status vqec_vision_ai_ftmgr_ucmgr_publish_initial(
        std::uint64_t _runtime_generation);
    [[nodiscard]] status vqec_vision_ai_ftmgr_ucmgr_publish_pending(
        std::uint64_t _control_revision, std::uint64_t _runtime_generation);
    [[nodiscard]] status vqec_vision_ai_ftmgr_ucmgr_fail_pending(
        std::uint64_t _control_revision, status_code _reason_code);

private:
    struct receipt_record {
        std::string request_id_;
        std::uint64_t expected_control_revision_{0};
        std::vector<usecase_desired_entry> entries_;
        usecase_apply_receipt receipt_;
    };

    usecase_control_snapshot control_;
    deployment_config base_deployment_;
    model_catalog models_;
    usecase_activation_snapshot activation_;
    deployment_config active_deployment_;
    usecase_control_snapshot pending_control_;
    usecase_activation_snapshot pending_activation_;
    deployment_config pending_deployment_;
    std::vector<receipt_record> receipts_;
    std::uint64_t runtime_generation_{0};
    status_code pending_failure_{status_code::ok};
    bool configured_{false};
    bool has_pending_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_FTMGR_USECASE_CONTROL_MANAGER_HPP
