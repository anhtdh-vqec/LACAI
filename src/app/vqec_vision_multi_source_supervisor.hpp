#ifndef VQEC_VISION_AI_APP_MULTI_SOURCE_SUPERVISOR_HPP
#define VQEC_VISION_AI_APP_MULTI_SOURCE_SUPERVISOR_HPP

#include <array>
#include <cstdint>

#include "vqec_vision_source_session.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_deployment_config.hpp"

namespace vqec::vision::ai {

inline constexpr std::uint16_t g_invalid_source_index = UINT16_MAX;

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
};

struct multi_source_progress_report {
    std::uint16_t source_index_{g_invalid_source_index};
    status source_status_;
    source_session_health source_health_;
    source_session_progress source_progress_;
    bool has_source_{false};
    bool has_result_{false};
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
    status_code first_error_code_{status_code::ok};
};

// Fixed-capacity, serialized coordinator. Sessions are exclusively borrowed and must
// outlive the supervisor. bind/activate are cold-path operations; step is round-robin.
class multi_source_supervisor {
public:
    explicit multi_source_supervisor(multi_source_supervisor_config _config) noexcept;
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

private:
    [[nodiscard]] status vqec_vision_ai_appl_mssup_check_time(
        std::uint64_t _steady_now_ns);
    void vqec_vision_ai_appl_mssup_refresh_state() noexcept;

    multi_source_supervisor_config config_;
    std::array<source_session_port*, deployment_limits::g_max_sources> sessions_{};
    multi_source_supervisor_state state_{multi_source_supervisor_state::binding};
    std::uint16_t bound_count_{0};
    std::uint16_t next_source_index_{0};
    std::uint64_t last_now_ns_{0};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_MULTI_SOURCE_SUPERVISOR_HPP
