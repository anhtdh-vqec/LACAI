#ifndef VQEC_VISION_AI_APP_MULTI_MODEL_PUMP_HPP
#define VQEC_VISION_AI_APP_MULTI_MODEL_PUMP_HPP

#include <array>
#include <cstdint>

#include "vqec_vision_model_cadence.hpp"
#include "vqec/vision/ai/ports/vqec_vision_inference_graph.hpp"
#include "vqec/vision/ai/ports/vqec_vision_raw_source.hpp"

namespace vqec::vision::ai {

struct multi_model_graph_binding {
    inference_graph_port* graph_{nullptr};
    std::uint64_t cycle_id_{0};
    std::uint64_t job_timeout_ns_{0};
};

struct multi_model_pump_report {
    std::array<submission_ticket, deployment_limits::g_max_models_per_source>
        submitted_tickets_{};
    submission_ticket result_ticket_;
    std::uint64_t skipped_cadence_intervals_{0};
    std::uint16_t due_model_mask_{0};
    std::uint16_t submitted_model_mask_{0};
    std::uint16_t busy_model_mask_{0};
    std::uint16_t result_model_slot_{UINT16_MAX};
    std::uint16_t error_model_slot_{UINT16_MAX};
    bool has_result_{false};
};

// One serialized source executor. Bindings are borrowed and immutable after configure.
// A received owner is shared only with graph submissions accepted during the same step.
class multi_model_pump {
public:
    explicit multi_model_pump(raw_source_port& _source);
    multi_model_pump(const multi_model_pump& _other) = delete;
    multi_model_pump& operator=(const multi_model_pump& _other) = delete;
    [[nodiscard]] status vqec_vision_ai_appl_mmump_configure(
        const model_cadence_config& _cadence,
        const std::array<multi_model_graph_binding,
            deployment_limits::g_max_models_per_source>& _bindings,
        std::uint16_t _binding_count);
    [[nodiscard]] status vqec_vision_ai_appl_mmump_pump_step(
        std::uint64_t _steady_now_ns, tensor_result& _result,
        multi_model_pump_report& _report);
    void vqec_vision_ai_appl_mmump_begin_stop() noexcept;
    [[nodiscard]] std::uint16_t
    vqec_vision_ai_appl_mmump_get_model_count() const noexcept;
    [[nodiscard]] bool vqec_vision_ai_appl_mmump_has_failed() const noexcept;

private:
    [[nodiscard]] status vqec_vision_ai_appl_mmump_poll_result(
        std::uint64_t _steady_now_ns, tensor_result& _result,
        multi_model_pump_report& _report);
    raw_source_port& source_;
    model_cadence_scheduler cadence_;
    std::array<multi_model_graph_binding, deployment_limits::g_max_models_per_source>
        bindings_{};
    std::array<bool, deployment_limits::g_max_models_per_source> is_armed_{};
    std::uint64_t last_now_ns_{0};
    std::uint16_t model_count_{0};
    std::uint16_t result_cursor_{0};
    bool is_configured_{false};
    bool is_stopping_{false};
    bool is_failed_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_MULTI_MODEL_PUMP_HPP
