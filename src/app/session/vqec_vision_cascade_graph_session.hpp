#ifndef VQEC_VISION_AI_APPL_CASCADE_GRAPH_SESSION_HPP
#define VQEC_VISION_AI_APPL_CASCADE_GRAPH_SESSION_HPP

#include <cstdint>
#include <vector>

#include "vqec/vision/ai/contracts/inference/vqec_vision_inference_plan.hpp"
#include "vqec/vision/ai/contracts/inference/vqec_vision_source_binding.hpp"
#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/inference/vqec_vision_tensor_contract.hpp"
#include "vqec/vision/ai/ports/inference/vqec_vision_inference_graph.hpp"

namespace vqec::vision::ai {

enum class cascade_graph_session_state {
    idle,
    configuring,
    loading,
    binding,
    starting,
    running,
    draining,
    unloading,
    stopped,
    faulted
};

struct cascade_graph_session_config {
    inference_graph_port* graph_{nullptr};
    inference_plan plan_;
    source_binding binding_;
    std::vector<tensor_spec> outputs_;
    std::uint64_t max_output_bytes_{0};
    std::uint64_t startup_timeout_ns_{0};
    std::uint64_t stop_timeout_ns_{0};
};

// Serialized lifecycle owner for a secondary tensor graph. The graph is borrowed and must
// outlive this session. Epoch arm/submission belongs to cascade_coordinator after running.
class cascade_graph_session final {
public:
    explicit cascade_graph_session(cascade_graph_session_config _config);
    cascade_graph_session(const cascade_graph_session& _other) = delete;
    cascade_graph_session& operator=(const cascade_graph_session& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_cgses_step(std::uint64_t _steady_now_ns);
    [[nodiscard]] status vqec_vision_ai_appl_cgses_request_stop(
        std::uint64_t _steady_now_ns);
    [[nodiscard]] cascade_graph_session_state
    vqec_vision_ai_appl_cgses_get_state() const noexcept;
    [[nodiscard]] const status& vqec_vision_ai_appl_cgses_get_last_error() const noexcept;
    [[nodiscard]] bool vqec_vision_ai_appl_cgses_is_recovery_required() const noexcept;

private:
    [[nodiscard]] status vqec_vision_ai_appl_cgses_check_time(
        std::uint64_t _steady_now_ns);
    [[nodiscard]] status vqec_vision_ai_appl_cgses_validate_activation();
    [[nodiscard]] status vqec_vision_ai_appl_cgses_step_start();
    [[nodiscard]] status vqec_vision_ai_appl_cgses_step_stop(
        std::uint64_t _steady_now_ns);
    [[nodiscard]] status vqec_vision_ai_appl_cgses_fault(status _failure);

    cascade_graph_session_config config_;
    cascade_graph_session_state state_{cascade_graph_session_state::idle};
    status last_error_;
    std::uint64_t last_now_ns_{0};
    std::uint64_t start_ns_{0};
    std::uint64_t stop_ns_{0};
    bool is_recovery_required_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_CASCADE_GRAPH_SESSION_HPP
