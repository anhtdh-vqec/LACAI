#ifndef VQEC_VISION_AI_APP_MULTI_MODEL_FEATURE_PIPELINE_HPP
#define VQEC_VISION_AI_APP_MULTI_MODEL_FEATURE_PIPELINE_HPP

#include <array>
#include <cstdint>

#include "vqec_vision_feature_fanout.hpp"
#include "vqec_vision_multi_model_result_router.hpp"

namespace vqec::vision::ai {

struct multi_model_feature_pipeline_report {
    multi_model_result_report result_;
    feature_fanout_report features_;
    bool has_feature_fanout_{false};
};

// Serialized per source. One optional, exclusive feature fan-out is bound per model slot.
class multi_model_feature_pipeline final {
public:
    explicit multi_model_feature_pipeline(multi_model_result_router& _result_router) noexcept;
    multi_model_feature_pipeline(const multi_model_feature_pipeline& _other) = delete;
    multi_model_feature_pipeline& operator=(const multi_model_feature_pipeline& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_mmfpl_configure(
        const std::array<feature_fanout*, deployment_limits::g_max_models_per_source>&
            _feature_fanouts,
        std::uint16_t _model_count);
    [[nodiscard]] status vqec_vision_ai_appl_mmfpl_process_result(
        const tensor_result& _result, const multi_model_pump_report& _pump_report,
        std::uint64_t _now_monotonic_ns,
        std::array<observation_batch, deployment_limits::g_max_models_per_source>&
            _tracked_by_model,
        std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages>&
            _events,
        multi_model_feature_pipeline_report& _report);

private:
    multi_model_result_router& result_router_;
    std::array<feature_fanout*, deployment_limits::g_max_models_per_source>
        feature_fanouts_{};
    std::uint16_t model_count_{0};
    bool is_configured_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_MULTI_MODEL_FEATURE_PIPELINE_HPP
