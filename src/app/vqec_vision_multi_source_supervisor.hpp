#ifndef VQEC_VISION_AI_APP_MULTI_SOURCE_SUPERVISOR_HPP
#define VQEC_VISION_AI_APP_MULTI_SOURCE_SUPERVISOR_HPP

#include <array>
#include <cstdint>

#include "vqec_vision_source_session.hpp"
#include "vqec_vision_source_session_worker.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_deployment_config.hpp"

namespace vqec::vision::ai {

inline constexpr std::uint16_t g_invalid_source_index = UINT16_MAX;
// Bounded per-source fault channel so an isolated source error stays observable even
// though step() keeps returning pending to preserve fault isolation.
inline constexpr std::uint16_t g_max_supervisor_fault_events = 32;

enum class multi_source_supervisor_state {
    binding,
    running,
    stopping,
    stopped
};

struct multi_source_supervisor_config {
    std::uint64_t deployment_revision_{0};
    std::uint64_t catalog_revision_{0};
    std::uint16_t source_count_{0};
    // When true, each bound session runs on a `source_session_worker` so a blocking backend
    // call inside a session step cannot stall other sources. Default false keeps the
    // synchronous serialized behavior.
    bool use_session_workers_{false};
};

struct multi_source_progress_report {
    std::uint16_t source_index_{g_invalid_source_index};
    status source_status_;
    source_session_health source_health_;
    source_session_progress source_progress_;
    bool has_source_{false};
    bool has_result_{false};
};

struct multi_source_fault_event {
    std::uint16_t source_index_{g_invalid_source_index};
    status_code code_{status_code::ok};
    std::uint64_t at_ns_{0};
};

struct multi_source_supervisor_snapshot {
    multi_source_supervisor_state supervisor_state_{
        multi_source_supervisor_state::binding};
    std::uint64_t deployment_revision_{0};
    std::uint64_t catalog_revision_{0};
    std::uint16_t declared_sources_{0};
    std::uint16_t bound_sources_{0};
    std::uint16_t starting_sources_{0};
    std::uint16_t running_sources_{0};
    std::uint16_t stopping_sources_{0};
    std::uint16_t stopped_sources_{0};
    std::uint16_t recovery_sources_{0};
    std::uint16_t faulted_sources_{0};
    std::uint32_t fault_event_total_{0};
    std::array<status_code, deployment_limits::g_max_sources> source_fault_codes_{};
    status_code first_error_code_{status_code::ok};
};

// Fixed-capacity, serialized coordinator. Sessions are exclusively borrowed and must
// outlive the supervisor. bind/activate are cold-path operations; step is round-robin.
class multi_source_supervisor {
public:
    explicit multi_source_supervisor(multi_source_supervisor_config _config) noexcept;
    ~multi_source_supervisor() noexcept;
    multi_source_supervisor(const multi_source_supervisor& _other) = delete;
    multi_source_supervisor& operator=(const multi_source_supervisor& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_mssup_bind_session(
        std::uint16_t _source_index, source_session_port& _session);
    [[nodiscard]] status vqec_vision_ai_appl_mssup_activate();
    [[nodiscard]] status vqec_vision_ai_appl_mssup_step(
        std::uint64_t _steady_now_ns, tensor_result& _result,
        multi_source_progress_report& _report);
    [[nodiscard]] status vqec_vision_ai_appl_mssup_request_stop(
        std::uint64_t _steady_now_ns);
    [[nodiscard]] multi_source_supervisor_state
    vqec_vision_ai_appl_mssup_get_state() const noexcept;
    [[nodiscard]] multi_source_supervisor_snapshot
    vqec_vision_ai_appl_mssup_get_snapshot() const noexcept;
    // Pops the oldest recorded per-source fault event; pending when none is queued. This is
    // an independent channel, so an isolated source error is never an invisible pending.
    [[nodiscard]] status vqec_vision_ai_appl_mssup_take_fault(
        multi_source_fault_event& _fault);
    // Joins every session worker (async mode). Safe to call without activation or twice.
    [[nodiscard]] status vqec_vision_ai_appl_mssup_drain();

private:
    [[nodiscard]] status vqec_vision_ai_appl_mssup_check_time(
        std::uint64_t _steady_now_ns);
    void vqec_vision_ai_appl_mssup_refresh_state() noexcept;
    void vqec_vision_ai_appl_mssup_record_fault(
        std::uint16_t _source_index, status_code _code, std::uint64_t _at_ns) noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_mssup_step_async(
        std::uint64_t _steady_now_ns, tensor_result& _result,
        multi_source_progress_report& _report);

    multi_source_supervisor_config config_;
    std::array<source_session_port*, deployment_limits::g_max_sources> sessions_{};
    std::array<source_session_worker, deployment_limits::g_max_sources> workers_{};
    multi_source_supervisor_state state_{multi_source_supervisor_state::binding};
    std::array<status_code, deployment_limits::g_max_sources> source_fault_codes_{};
    std::array<multi_source_fault_event, g_max_supervisor_fault_events> fault_events_{};
    std::uint16_t bound_count_{0};
    std::uint16_t next_source_index_{0};
    std::uint16_t next_result_index_{0};
    bool async_mode_{false};
    std::uint16_t fault_event_head_{0};
    std::uint16_t fault_event_count_{0};
    std::uint32_t fault_event_total_{0};
    std::uint64_t last_now_ns_{0};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_MULTI_SOURCE_SUPERVISOR_HPP
