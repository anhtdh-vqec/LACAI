#ifndef VQEC_VISION_AI_APP_MULTI_MODEL_RESULT_ROUTER_HPP
#define VQEC_VISION_AI_APP_MULTI_MODEL_RESULT_ROUTER_HPP

#include <array>
#include <cstdint>

#include "vqec_vision_multi_model_pump.hpp"
#include "vqec_vision_perception_result_stage.hpp"

namespace vqec::vision::ai {

struct multi_model_result_report {
    std::uint16_t model_slot_{UINT16_MAX};
    bool has_tracked_{false};
    bool is_source_gap_{false};
};

// Serialized per source. Stage slots are borrowed and immutable after configure.
class multi_model_result_router final {
public:
    multi_model_result_router() = default;
    multi_model_result_router(const multi_model_result_router& _other) = delete;
    multi_model_result_router& operator=(const multi_model_result_router& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_mmrrt_configure(
        const perception_result_config& _source,
        const std::array<perception_result_stage*,
            deployment_limits::g_max_models_per_source>& _stages,
        std::uint16_t _stage_count);
    [[nodiscard]] status vqec_vision_ai_appl_mmrrt_route_result(
        const tensor_result& _result, const multi_model_pump_report& _pump_report,
        std::uint64_t _now_monotonic_ns,
        std::array<observation_batch, deployment_limits::g_max_models_per_source>&
            _tracked_by_model,
        multi_model_result_report& _report);
    [[nodiscard]] std::uint16_t
    vqec_vision_ai_appl_mmrrt_get_stage_count() const noexcept;

private:
    std::array<perception_result_stage*, deployment_limits::g_max_models_per_source>
        stages_{};
    std::array<std::uint64_t, deployment_limits::g_max_models_per_source> source_epochs_{};
    std::array<std::uint64_t, deployment_limits::g_max_models_per_source> frame_ids_{};
    std::array<std::uint64_t, deployment_limits::g_max_models_per_source> source_pts_ns_{};
    std::array<bool, deployment_limits::g_max_models_per_source> has_progress_{};
    std::uint16_t stage_count_{0};
    bool is_configured_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_MULTI_MODEL_RESULT_ROUTER_HPP
