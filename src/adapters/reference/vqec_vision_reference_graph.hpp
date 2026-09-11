#ifndef VQEC_VISION_AI_REFER_REFERENCE_GRAPH_HPP
#define VQEC_VISION_AI_REFER_REFERENCE_GRAPH_HPP

#include <cstdint>
#include <memory>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_submission_window.hpp"
#include "vqec/vision/ai/ports/vqec_vision_inference_graph.hpp"

namespace vqec::vision::ai {

struct reference_graph_config {
    // Pipeline anchor used to map source PTS onto the ticket PTS domain.
    std::uint64_t pipeline_anchor_ns_{1000000};
};

// Device-free development backend: performs no inference. It implements the neutral
// graph lifecycle and produces deterministic zero-valued tensors shaped like the
// declared outputs, so upstream composition, decode and tracking can be exercised
// without a board. It is not a model, does not load artifacts and proves nothing about
// Qualcomm support, accuracy or hardware completion. One outstanding job per graph.
class reference_inference_graph final : public inference_graph_port {
public:
    explicit reference_inference_graph(reference_graph_config _config = {}) noexcept;
    reference_inference_graph(const reference_inference_graph& _other) = delete;
    reference_inference_graph& operator=(const reference_inference_graph& _other) = delete;
    ~reference_inference_graph() noexcept override;

    [[nodiscard]] status vqec_vision_ai_ports_infgr_validate_activation() const override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_configure(
        const inference_plan& _plan) override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_load() override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_poll_state() override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_bind_source(
        const source_binding& _binding) override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_start(
        const std::vector<float_tensor_spec>& _outputs,
        std::uint64_t _max_output_bytes) override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_arm(
        std::uint64_t _cycle_id, std::uint64_t _source_epoch,
        std::uint64_t _job_timeout_ns) override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_submit_frame(
        const raw_frame& _frame, std::uint64_t _steady_now_ns,
        submission_ticket& _ticket) override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_poll_result(
        std::uint64_t _steady_now_ns, tensor_result& _result) override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_request_drain() override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_unload() override;
    [[nodiscard]] inference_graph_state
    vqec_vision_ai_ports_infgr_get_state() const noexcept override;
    [[nodiscard]] unsigned
    vqec_vision_ai_ports_infgr_get_outstanding() const noexcept override;
    [[nodiscard]] submission_ticket
    vqec_vision_ai_ports_infgr_get_pending_ticket() const noexcept override;

private:
    reference_graph_config config_;
    inference_plan plan_;
    std::vector<float_tensor_spec> outputs_;
    submission_window window_;
    inference_graph_state state_{inference_graph_state::empty};
    submission_ticket pending_ticket_;
    tensor_result pending_result_;
    std::shared_ptr<const void> retained_owner_;
    std::uint64_t max_output_bytes_{0};
    bool is_window_configured_{false};
    bool has_pending_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_REFER_REFERENCE_GRAPH_HPP
