#ifndef VQEC_VISION_AI_RUNTIME_SCHEDULER_MODEL_CADENCE_HPP
#define VQEC_VISION_AI_RUNTIME_SCHEDULER_MODEL_CADENCE_HPP

#include <array>
#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_model_catalog.hpp"

namespace vqec::vision::ai {

struct model_cadence_config {
    std::uint32_t source_fps_numerator_{0};
    std::uint32_t source_fps_denominator_{0};
    std::uint16_t model_count_{0};
    std::array<std::uint32_t, deployment_limits::g_max_models_per_source>
        model_fps_numerators_{};
    std::array<std::uint32_t, deployment_limits::g_max_models_per_source>
        model_fps_denominators_{};
};

struct model_cadence_selection {
    std::uint64_t frame_sequence_{0};
    std::uint64_t skipped_intervals_{0};
    std::uint16_t due_model_mask_{0};
    std::uint16_t model_count_{0};
};

// Cold-path composition. Model slot order is exactly source.model_ids order.
// Failure preserves output and performs no filesystem/vendor operation.
[[nodiscard]] status vqec_vision_ai_sched_mdcad_compose_config(
    const source_deployment_config& _source, const model_catalog& _catalog,
    model_cadence_config& _config);

[[nodiscard]] bool vqec_vision_ai_sched_mdcad_is_model_due(
    const model_cadence_selection& _selection, std::uint16_t _model_slot) noexcept;

// Serialized per logical source. configure is cold-path; select is allocation-free.
class model_cadence_scheduler {
public:
    [[nodiscard]] status vqec_vision_ai_sched_mdcad_configure(
        const model_cadence_config& _config);
    [[nodiscard]] status vqec_vision_ai_sched_mdcad_select(
        std::uint64_t _frame_sequence, model_cadence_selection& _selection);
    [[nodiscard]] bool vqec_vision_ai_sched_mdcad_is_configured() const noexcept;

private:
    std::array<std::uint64_t, deployment_limits::g_max_models_per_source> increments_{};
    std::array<std::uint64_t, deployment_limits::g_max_models_per_source> thresholds_{};
    std::array<std::uint64_t, deployment_limits::g_max_models_per_source> phases_{};
    std::uint64_t last_frame_sequence_{0};
    std::uint16_t model_count_{0};
    bool is_configured_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_RUNTIME_SCHEDULER_MODEL_CADENCE_HPP
