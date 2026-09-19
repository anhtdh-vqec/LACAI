#ifndef VQEC_VISION_AI_APPL_SERVICE_SHUTDOWN_HPP
#define VQEC_VISION_AI_APPL_SERVICE_SHUTDOWN_HPP

#include <array>
#include <cstdint>

#include "vqec_vision_service_cascade_runtime.hpp"
#include "vqec_vision_runtime_executor.hpp"

namespace vqec::vision::ai {

struct parsed_arguments;
class event_delivery_seam;
class service_enrollment_runtime;
class service_output_runtime;

struct service_shutdown_context {
    const parsed_arguments* arguments_{nullptr};
    runtime_executor* executor_{nullptr};
    std::array<service_cascade_owner, deployment_limits::g_max_sources>*
        cascade_owners_{nullptr};
    service_enrollment_runtime* enrollment_{nullptr};
    service_output_runtime* output_{nullptr};
    event_delivery_seam* production_seam_{nullptr};
    std::uint64_t steady_now_ns_{0};
    std::uint64_t steps_{0};
    std::uint32_t routed_source_mask_{0};
    status_code first_error_code_{status_code::ok};
    bool recognition_enabled_{false};
    bool production_platform_{false};
    bool generation_published_{false};
    bool reconcile_requested_{false};
};

// Pure exit-policy decision used by the generation controller and direct tests.
[[nodiscard]] int vqec_vision_ai_appl_svshd_decide_exit(
    bool _executor_stopped, bool _enrollment_stopped, bool _cascade_stopped,
    std::uint32_t _routed_sources, bool _generation_published,
    status_code _first_error_code, bool _reconcile_requested,
    std::uint32_t _required_sources) noexcept;

// Stops/drains owners in dependency order, reports final metrics, and maps the complete
// generation outcome to the process-level result. All pointers are borrowed and must outlive
// this call.
[[nodiscard]] int vqec_vision_ai_appl_svshd_stop_and_report(
    service_shutdown_context& _context) noexcept;

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_SERVICE_SHUTDOWN_HPP
