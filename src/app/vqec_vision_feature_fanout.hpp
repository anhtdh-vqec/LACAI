#ifndef VQEC_VISION_AI_APPL_FEATURE_FANOUT_HPP
#define VQEC_VISION_AI_APPL_FEATURE_FANOUT_HPP

#include <array>
#include <cstdint>

#include "vqec_vision_feature_stage.hpp"

namespace vqec::vision::ai {

namespace feature_fanout_limits {
inline constexpr std::uint16_t g_max_feature_stages = 32;
}  // namespace feature_fanout_limits

struct feature_fanout_report {
    std::array<status_code, feature_fanout_limits::g_max_feature_stages> status_codes_{};
    std::uint32_t processed_mask_{0};
    std::uint32_t failed_mask_{0};
    std::uint16_t first_error_slot_{UINT16_MAX};
};

class feature_fanout final {
public:
    feature_fanout() = default;
    feature_fanout(const feature_fanout& _other) = delete;
    feature_fanout& operator=(const feature_fanout& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_ftfan_configure(
        const std::array<feature_stage*, feature_fanout_limits::g_max_feature_stages>& _stages,
        std::uint16_t _stage_count);
    [[nodiscard]] status vqec_vision_ai_appl_ftfan_process(
        const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap,
        std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages>& _events,
        feature_fanout_report& _report);
    [[nodiscard]] std::uint16_t vqec_vision_ai_appl_ftfan_get_stage_count() const noexcept;

private:
    std::array<feature_stage*, feature_fanout_limits::g_max_feature_stages> stages_{};
    std::uint64_t last_now_monotonic_ns_{0};
    std::uint16_t stage_count_{0};
    bool is_configured_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_FEATURE_FANOUT_HPP
