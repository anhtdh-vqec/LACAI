#ifndef VQEC_VISION_AI_APP_MULTI_MODEL_SESSION_HPP
#define VQEC_VISION_AI_APP_MULTI_MODEL_SESSION_HPP

#include <array>
#include <cstdint>
#include <vector>

#include "vqec_vision_multi_model_pump.hpp"
#include "vqec_vision_source_session.hpp"

namespace vqec::vision::ai {

enum class multi_model_session_state {
    idle,
    acquiring,
    configuring,
    loading,
    binding,
    starting,
    running,
    draining_graphs,
    releasing_source,
    stopped
};

struct multi_model_graph_config {
    inference_graph_port* graph_{nullptr};
    inference_plan plan_;
    source_binding binding_;
    std::vector<float_tensor_spec> outputs_;
    std::uint64_t max_output_bytes_{0};
    std::uint64_t cycle_id_{0};
    std::uint64_t job_timeout_ns_{1000000000};
};

struct multi_model_session_config {
    std::array<multi_model_graph_config, deployment_limits::g_max_models_per_source>
        graphs_{};
    model_cadence_config cadence_;
    std::uint64_t startup_timeout_ns_{30000000000};
    std::uint64_t stop_timeout_ns_{10000000000};
    std::uint16_t graph_count_{0};
    int rpc_timeout_ms_{1000};
};

struct multi_model_session_snapshot {
    std::array<inference_graph_state, deployment_limits::g_max_models_per_source>
        graph_states_{};
    multi_model_session_state session_state_{multi_model_session_state::idle};
    raw_source_state source_state_{raw_source_state::idle};
    status_code first_error_code_{status_code::ok};
    unsigned outstanding_jobs_{0};
    unsigned source_readers_{0};
    std::uint16_t graph_count_{0};
    std::uint16_t running_graph_count_{0};
    bool is_recovery_required_{false};
};

// One acquisition cycle for 1..16 borrowed graphs. Calls are serialized and monotonic.
// Explicit stopped state is required before destroying source, graph or retention owners.
class multi_model_session final : public source_session_port {
public:
    multi_model_session(raw_source_port& _source, multi_model_session_config _config);
    multi_model_session(const multi_model_session& _other) = delete;
    multi_model_session& operator=(const multi_model_session& _other) = delete;
    [[nodiscard]] status vqec_vision_ai_appl_mmses_step(
        std::uint64_t _steady_now_ns, tensor_result& _result,
        source_session_progress& _progress);
    [[nodiscard]] status vqec_vision_ai_appl_mmses_request_stop(
        std::uint64_t _steady_now_ns);
    [[nodiscard]] multi_model_session_snapshot
    vqec_vision_ai_appl_mmses_get_snapshot() const noexcept;
    [[nodiscard]] const status&
    vqec_vision_ai_appl_mmses_get_last_error() const noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_srcsn_step(
        std::uint64_t _steady_now_ns, tensor_result& _result,
        source_session_progress& _progress) override;
    [[nodiscard]] status vqec_vision_ai_appl_srcsn_request_stop(
        std::uint64_t _steady_now_ns) override;
    [[nodiscard]] source_session_health
    vqec_vision_ai_appl_srcsn_get_health() const noexcept override;

private:
    [[nodiscard]] status vqec_vision_ai_appl_mmses_check_time(
        std::uint64_t _steady_now_ns);
    [[nodiscard]] status vqec_vision_ai_appl_mmses_prepare_activation();
    [[nodiscard]] status vqec_vision_ai_appl_mmses_start_graph();
    [[nodiscard]] status vqec_vision_ai_appl_mmses_stop_graph(
        std::uint64_t _steady_now_ns);
    void vqec_vision_ai_appl_mmses_record_error(const status& _error);
    raw_source_port& source_;
    multi_model_session_config config_;
    multi_model_pump pump_;
    multi_model_session_state state_{multi_model_session_state::idle};
    status last_error_;
    std::uint64_t last_now_ns_{0};
    std::uint64_t start_ns_{0};
    std::uint64_t stop_ns_{0};
    std::uint16_t active_graph_slot_{0};
    std::uint16_t drain_graph_slot_{0};
    bool is_recovery_required_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_MULTI_MODEL_SESSION_HPP
