#ifndef VQEC_VISION_AI_APPL_SERVICE_EXECUTION_LOOP_HPP
#define VQEC_VISION_AI_APPL_SERVICE_EXECUTION_LOOP_HPP

#include <array>
#include <cstdint>
#include <functional>
#include <string>

#include "vqec_vision_service_cascade_runtime.hpp"

namespace vqec::vision::ai {

struct model_catalog;
struct parsed_arguments;
struct runtime_feature_activation;
class face_enrollment_port;
class metadata_runtime;
class output_gate;
class production_platform;
class recognition_session;
class runtime_composition_bundle;
class runtime_executor;
class service_enrollment_runtime;
class usecase_control_manager;

struct service_execution_context {
    const parsed_arguments* arguments_{nullptr};
    const deployment_config* deployment_{nullptr};
    const model_catalog* catalog_{nullptr};
    const std::string* tracker_contract_{nullptr};
    runtime_composition_bundle* bundle_{nullptr};
    runtime_executor* executor_{nullptr};
    production_platform* production_{nullptr};
    output_gate* output_policy_gate_{nullptr};
    metadata_runtime* metadata_{nullptr};
    recognition_session* recognition_{nullptr};
    service_enrollment_runtime* enrollment_{nullptr};
    face_enrollment_port* enrollment_port_{nullptr};
    std::array<service_cascade_owner, deployment_limits::g_max_sources>*
        cascade_owners_{nullptr};
    usecase_control_manager* control_manager_{nullptr};
    const runtime_feature_activation* feature_wiring_{nullptr};
    std::function<void()> poll_control_{};
    std::function<bool()> reconcile_requested_{};
    std::function<bool()> stop_requested_{};
    std::uint64_t runtime_generation_{0};
    std::uint64_t pending_control_revision_{0};
    bool production_platform_enabled_{false};
    bool recognition_enabled_{false};
    bool metadata_required_{false};
};

struct service_execution_result {
    std::uint64_t steady_now_ns_{0};
    std::uint64_t steps_{0};
    std::uint32_t routed_source_mask_{0};
    status_code first_error_code_{status_code::ok};
    bool generation_published_{false};
    bool reconcile_requested_{false};
};

// Fixed-capacity rational preview cadence. The phase is generation-owned and must not be
// shared across sources or survive a generation replacement.
[[nodiscard]] bool vqec_vision_ai_appl_svxlp_select_preview(
    std::uint32_t _source_fps, std::uint32_t _output_fps,
    std::uint32_t& _phase, bool& _initialized) noexcept;

// Advances one generation until stop, replacement or the configured step limit. It owns no
// graph or frame; all pointers are borrowed from the generation controller.
[[nodiscard]] status vqec_vision_ai_appl_svxlp_run(
    const service_execution_context& _context,
    service_execution_result& _result);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_SERVICE_EXECUTION_LOOP_HPP
