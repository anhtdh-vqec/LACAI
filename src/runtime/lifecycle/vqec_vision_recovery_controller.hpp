#ifndef VQEC_VISION_AI_RUNTIME_LIFECYCLE_RECOVERY_CONTROLLER_HPP
#define VQEC_VISION_AI_RUNTIME_LIFECYCLE_RECOVERY_CONTROLLER_HPP

#include <array>
#include <cstdint>

#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_deployment_config.hpp"
#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace recovery_policy_limits {
inline constexpr std::uint32_t g_max_retries = 64;
inline constexpr std::uint64_t g_max_backoff_ns = 600000000000ULL;  // 10 minutes
}  // namespace recovery_policy_limits

// What the caller should do after a transient source fault. The controller never sleeps
// or retries itself; the caller owns the executor/clock and applies the decision.
enum class recovery_decision { retry, wait, exhausted };

struct recovery_policy {
    std::uint32_t max_retries_{5};
    std::uint64_t initial_backoff_ns_{100000000ULL};
    std::uint64_t max_backoff_ns_{5000000000ULL};
    // Exponential multiplier as an exact rational, checked for overflow.
    std::uint32_t multiplier_numerator_{2};
    std::uint32_t multiplier_denominator_{1};
};

struct recovery_snapshot {
    std::uint32_t attempts_{0};
    std::uint64_t next_retry_ns_{0};
    bool exhausted_{false};
};

// Per-source exponential backoff with a bounded retry budget. Time is caller-supplied
// (monotonic nanoseconds), so tests need no wall-clock and no sleeping. A source fault
// never blocks another source: each slot has independent state.
class source_recovery_controller {
public:
    [[nodiscard]] status vqec_vision_ai_life_rcvr_configure(
        std::uint16_t _source_count, const recovery_policy& _policy);
    [[nodiscard]] bool vqec_vision_ai_life_rcvr_is_configured() const noexcept;
    // Records one transient fault at _now_ns and returns retry/wait/exhausted.
    [[nodiscard]] status vqec_vision_ai_life_rcvr_on_fault(
        std::uint16_t _source_slot, std::uint64_t _now_ns,
        recovery_decision& _decision);
    // A successful cycle clears attempts and backoff for that source.
    [[nodiscard]] status vqec_vision_ai_life_rcvr_on_success(std::uint16_t _source_slot);
    [[nodiscard]] status vqec_vision_ai_life_rcvr_get_snapshot(
        std::uint16_t _source_slot, recovery_snapshot& _snapshot) const;

private:
    std::array<std::uint32_t, deployment_limits::g_max_sources> attempts_{};
    std::array<std::uint64_t, deployment_limits::g_max_sources> next_retry_ns_{};
    std::array<bool, deployment_limits::g_max_sources> exhausted_{};
    recovery_policy policy_;
    std::uint16_t source_count_{0};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_RUNTIME_LIFECYCLE_RECOVERY_CONTROLLER_HPP
