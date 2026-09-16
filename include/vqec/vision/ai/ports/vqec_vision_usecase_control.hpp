#ifndef VQEC_VISION_AI_PORTS_USECASE_CONTROL_HPP
#define VQEC_VISION_AI_PORTS_USECASE_CONTROL_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace usecase_control_limits {
inline constexpr std::size_t g_max_request_id_bytes = 128;
inline constexpr std::size_t g_max_reason_bytes = 128;
inline constexpr std::size_t g_max_idempotency_receipts = 64;
}  // namespace usecase_control_limits

enum class usecase_apply_state { unchanged, reconciling, running, degraded, failed };

enum class usecase_runtime_state {
    disabled,
    denied,
    unsupported,
    incompatible,
    resource_limited,
    loading,
    running,
    draining,
    faulted
};

struct usecase_desired_entry {
    std::string source_id_;
    std::string usecase_id_;
    bool desired_enabled_{false};
};

struct usecase_desired_plan {
    std::string request_id_;
    std::uint64_t expected_control_revision_{0};
    std::vector<usecase_desired_entry> entries_;
};

struct usecase_apply_receipt {
    bool accepted_{false};
    std::uint64_t control_revision_{0};
    usecase_apply_state apply_state_{usecase_apply_state::failed};
    status_code reason_code_{status_code::ok};
};

struct usecase_status_entry {
    std::string source_id_;
    std::string usecase_id_;
    bool installed_{false};
    bool entitled_{false};
    bool desired_{false};
    bool supported_{false};
    bool compatible_{false};
    bool admitted_{false};
    bool loaded_{false};
    bool running_{false};
    usecase_runtime_state effective_state_{usecase_runtime_state::disabled};
    std::string reason_;
};

struct usecase_control_status {
    std::uint64_t control_revision_{0};
    std::uint64_t entitlement_revision_{0};
    std::uint64_t runtime_generation_{0};
    std::vector<usecase_status_entry> entries_;
};

struct usecase_capability {
    std::string usecase_id_;
    std::string usecase_version_;
};

struct usecase_capability_snapshot {
    std::uint64_t catalog_revision_{0};
    std::vector<usecase_capability> usecases_;
};

class usecase_control_port {
public:
    virtual ~usecase_control_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_ucctl_apply_desired_plan(
        const usecase_desired_plan& _plan, usecase_apply_receipt& _receipt) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_ucctl_get_status(
        usecase_control_status& _status) const = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_ucctl_get_capabilities(
        usecase_capability_snapshot& _capabilities) const = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_USECASE_CONTROL_HPP
