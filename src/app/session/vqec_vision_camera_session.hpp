#ifndef VQEC_VISION_AI_APP_CAMERA_SESSION_HPP
#define VQEC_VISION_AI_APP_CAMERA_SESSION_HPP

#include <cstdint>
#include <memory>
#include <vector>

#include "vqec_vision_camera_graph_pump.hpp"
#include "vqec_vision_source_session.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_model_outputs.hpp"

namespace vqec::vision::ai {

enum class camera_session_state {
    idle, acquiring, configuring, loading, binding, starting, running,
    draining_graph, releasing_camera, stopped
};

struct camera_session_config {
    inference_plan plan_;
    source_binding binding_;
    std::vector<tensor_spec> outputs_;
    std::uint64_t max_output_bytes_{0};
    std::uint64_t cycle_id_{0};
    std::uint64_t job_timeout_ns_{1000000000};
    std::uint64_t startup_timeout_ns_{30000000000};
    std::uint64_t stop_timeout_ns_{10000000000};
    int rpc_timeout_ms_{1000};
};

struct camera_session_snapshot {
    camera_session_state session_state_{camera_session_state::idle};
    raw_source_state source_state_{raw_source_state::idle};
    inference_graph_state graph_state_{inference_graph_state::empty};
    unsigned graph_jobs_{0};
    unsigned source_readers_{0};
    bool is_recovery_required_{false};
    status_code first_error_code_{status_code::ok};
};

// Expected identity comes from trusted deployment selection, NOT the parsed JSON itself.
// This is correlation, not digest calculation, signature verification or SDK qualification.
struct model_output_selection {
    std::string model_id_;
    std::string model_version_;
    std::string artifact_sha256_;
    std::string decoder_contract_;
};

// Before constructing a session only. Atomically replaces outputs/budget on successful match.
// Other config fields stay unchanged. Failure preserves config; allocation failures propagate.
[[nodiscard]] status vqec_vision_ai_appl_camsn_bind_model_outputs(
    const model_outputs& _manifest, const model_output_selection& _selection,
    camera_session_config& _config);

// Exclusive borrowed source/graph, one cycle, serialized monotonic calls.
// Explicit stopped state required before owners are destroyed; no destructor shutdown.
class camera_session final : public source_session_port {
public:
    camera_session(raw_source_port& _source, inference_graph_port& _graph,
        camera_session_config _config);
    camera_session(const camera_session& _other) = delete;
    camera_session& operator=(const camera_session& _other) = delete;
    [[nodiscard]] status vqec_vision_ai_appl_camsn_step(
        std::uint64_t _steady_now_ns, tensor_result& _result, camera_pump_report& _report);
    [[nodiscard]] status vqec_vision_ai_appl_camsn_request_stop(std::uint64_t _steady_now_ns);
    [[nodiscard]] camera_session_state vqec_vision_ai_appl_camsn_get_state() const noexcept;
    [[nodiscard]] bool vqec_vision_ai_appl_camsn_is_recovery_required() const noexcept;
    [[nodiscard]] const status& vqec_vision_ai_appl_camsn_get_last_error() const noexcept;
    // Same serialized executor as step; no I/O or state transitions.
    [[nodiscard]] camera_session_snapshot vqec_vision_ai_appl_camsn_get_snapshot() const noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_srcsn_step(
        std::uint64_t _steady_now_ns, tensor_result& _result,
        source_session_progress& _progress) override;
    [[nodiscard]] status vqec_vision_ai_appl_srcsn_request_stop(
        std::uint64_t _steady_now_ns) override;
    [[nodiscard]] source_session_health
    vqec_vision_ai_appl_srcsn_get_health() const noexcept override;

private:
    [[nodiscard]] status vqec_vision_ai_appl_camsn_check_time(std::uint64_t _steady_now_ns);
    [[nodiscard]] status vqec_vision_ai_appl_camsn_stop_graph(std::uint64_t _steady_now_ns);
    raw_source_port& source_;
    inference_graph_port& graph_;
    camera_session_config config_;
    camera_graph_pump pump_;
    camera_session_state state_{camera_session_state::idle};
    status last_error_;
    std::uint64_t last_now_ns_{0};
    std::uint64_t start_ns_{0};
    std::uint64_t stop_ns_{0};
    bool is_recovery_required_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_CAMERA_SESSION_HPP
