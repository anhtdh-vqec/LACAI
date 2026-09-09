#ifndef VQEC_VISION_AI_QUALCOMM_INFERENCE_GRAPH_HPP
#define VQEC_VISION_AI_QUALCOMM_INFERENCE_GRAPH_HPP

#include <memory>

#include "vqec_vision_plugin_graph.hpp"
#include "vqec/vision/ai/ports/vqec_vision_inference_graph.hpp"

namespace vqec::vision::ai {

// Borrowed graph plus shared safety-retention domain. Graph must outlive this adapter.
class qualcomm_inference_graph final : public inference_graph_port {
public:
    qualcomm_inference_graph(plugin_graph& _graph,
        std::shared_ptr<graph_retention> _retention);

    [[nodiscard]] status vqec_vision_ai_ports_infgr_validate_activation()
        const override;
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
    plugin_graph& graph_;
    std::shared_ptr<graph_retention> retention_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QUALCOMM_INFERENCE_GRAPH_HPP
