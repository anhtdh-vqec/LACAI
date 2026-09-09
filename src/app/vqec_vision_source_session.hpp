#ifndef VQEC_VISION_AI_APP_SOURCE_SESSION_HPP
#define VQEC_VISION_AI_APP_SOURCE_SESSION_HPP

#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_submission_window.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"

namespace vqec::vision::ai {

inline constexpr std::uint16_t g_invalid_model_slot = UINT16_MAX;

enum class source_session_phase {
    idle,
    starting,
    running,
    draining,
    stopped
};

struct source_session_progress {
    std::uint16_t model_slot_{g_invalid_model_slot};
    std::uint16_t due_model_mask_{0};
    std::uint16_t submitted_model_mask_{0};
    std::uint16_t busy_model_mask_{0};
    std::uint16_t error_model_slot_{g_invalid_model_slot};
    submission_ticket ticket_;
    bool has_submission_{false};
    bool has_result_{false};
};

struct source_session_health {
    source_session_phase phase_{source_session_phase::idle};
    std::uint16_t model_graph_count_{0};
    std::uint16_t running_graph_count_{0};
    unsigned outstanding_jobs_{0};
    unsigned source_readers_{0};
    bool is_recovery_required_{false};
    status_code first_error_code_{status_code::ok};
};

// One exclusively serialized logical RAW-source owner. Implementations may contain one
// or many model graphs, but must acquire the FW source once and retain shared frames until
// every submitted graph has completed reading.
class source_session_port {
public:
    virtual ~source_session_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_appl_srcsn_step(
        std::uint64_t _steady_now_ns, tensor_result& _result,
        source_session_progress& _progress) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_appl_srcsn_request_stop(
        std::uint64_t _steady_now_ns) = 0;
    [[nodiscard]] virtual source_session_health
    vqec_vision_ai_appl_srcsn_get_health() const noexcept = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_SOURCE_SESSION_HPP
