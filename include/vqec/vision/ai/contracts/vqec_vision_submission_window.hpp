#ifndef VQEC_VISION_AI_CONTRACTS_SUBMISSION_WINDOW_HPP
#define VQEC_VISION_AI_CONTRACTS_SUBMISSION_WINDOW_HPP

#include <array>
#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace submission_limits {
// AI ledger ceiling, not the Camera producer's independent FD retention budget.
inline constexpr unsigned g_max_jobs = 4;
// Configurable fallback only; platform/workload admission must set a measured deadline.
inline constexpr std::uint64_t g_default_job_timeout_ns = 1000000000ULL;
}  // namespace submission_limits

struct submission_token {
    std::uint64_t cycle_id_{0};
    std::uint64_t job_id_{0};
};

struct submission_ticket {
    submission_token token_;
    std::uint64_t source_pts_ns_{0};
    std::uint64_t pipeline_pts_ns_{0};
};

struct submission_config {
    std::uint64_t cycle_id_{0};
    std::uint64_t source_epoch_{0};
    std::uint64_t pipeline_anchor_ns_{0};
    std::uint64_t job_timeout_ns_{submission_limits::g_default_job_timeout_ns};
    unsigned capacity_{1};
};

// Serialized bookkeeping only; no resource ownership or hardware cancellation.
// One configuration per object; caller reconciles all jobs before destruction.
class submission_window {
public:
    submission_window() = default;
    submission_window(const submission_window&) = delete;
    submission_window& operator=(const submission_window&) = delete;
    [[nodiscard]] status vqec_vision_ai_core_subwn_configure(const submission_config& _config);
    [[nodiscard]] status vqec_vision_ai_core_subwn_reserve(
        std::uint64_t _source_epoch, std::uint64_t _source_pts_ns,
        std::uint64_t _steady_now_ns, submission_ticket& _ticket);
    [[nodiscard]] status vqec_vision_ai_core_subwn_commit(submission_token _token);
    [[nodiscard]] status vqec_vision_ai_core_subwn_cancel_reserved(submission_token _token);
    // Backend must prove these events; a timeout or FD close is not such proof.
    [[nodiscard]] status vqec_vision_ai_core_subwn_complete_input(submission_token _token);
    [[nodiscard]] status vqec_vision_ai_core_subwn_complete_result(submission_token _token);
    [[nodiscard]] status vqec_vision_ai_core_subwn_check_deadlines(std::uint64_t _steady_now_ns);
    void vqec_vision_ai_core_subwn_begin_drain() noexcept;
    void vqec_vision_ai_core_subwn_mark_fault() noexcept;
    [[nodiscard]] unsigned vqec_vision_ai_core_subwn_get_outstanding() const noexcept;
    [[nodiscard]] bool vqec_vision_ai_core_subwn_is_accepting() const noexcept;

private:
    struct slot {
        std::uint64_t job_id_{0};
        std::uint64_t deadline_ns_{0};
        bool committed_{false};
        bool input_done_{false};
        bool result_done_{false};
    };
    [[nodiscard]] slot* vqec_vision_ai_core_subwn_find_slot(submission_token _token) noexcept;
    [[nodiscard]] status vqec_vision_ai_core_subwn_complete(
        submission_token _token, bool _input);
    submission_config config_;
    std::array<slot, submission_limits::g_max_jobs> slots_{};
    std::uint64_t next_job_id_{1};
    std::uint64_t first_source_pts_ns_{0};
    std::uint64_t last_source_pts_ns_{0};
    bool configured_{false};
    bool has_timestamp_{false};
    bool draining_{false};
    bool faulted_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_SUBMISSION_WINDOW_HPP
