#ifndef VQEC_VISION_AI_APPL_SERVICE_GENERATION_HPP
#define VQEC_VISION_AI_APPL_SERVICE_GENERATION_HPP

#include <cstdint>
#include <functional>

#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_app_lifecycle.hpp"
#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_deployment_config.hpp"

namespace vqec::vision::ai {
class usecase_control_manager;
}

[[nodiscard]] int vqec_vision_ai_appl_svgen_run_generation(
    int _argc, char** _argv,
    const vqec::vision::ai::deployment_config* _effective_deployment,
    const vqec::vision::ai::runtime_control_snapshot* _runtime_control,
    vqec::vision::ai::usecase_control_manager* _control_manager,
    const std::function<void()>& _poll_control,
    const std::function<bool()>& _is_runtime_reconcile_requested,
    const std::function<const vqec::vision::ai::runtime_control_snapshot*()>&
        _get_pending_runtime_control,
    const std::function<void()>& _mark_runtime_control_applied,
    std::uint64_t _runtime_generation, std::uint64_t _pending_control_revision);

[[nodiscard]] std::uint64_t
vqec_vision_ai_appl_svgen_current_monotonic_ns() noexcept;
[[nodiscard]] bool vqec_vision_ai_appl_svgen_is_stop_requested() noexcept;

#endif  // VQEC_VISION_AI_APPL_SERVICE_GENERATION_HPP
