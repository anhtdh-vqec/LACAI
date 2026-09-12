#ifndef VQEC_VISION_AI_QCOM_QNN_INFERENCE_GRAPH_HPP
#define VQEC_VISION_AI_QCOM_QNN_INFERENCE_GRAPH_HPP

#include <cstdint>
#include <vector>

#include "vqec_vision_qnn_engine.hpp"
#include "vqec/vision/ai/ports/vqec_vision_inference_graph.hpp"

namespace vqec::vision::ai {

// inference_graph_port implementation over the LACAI-owned QNN engine. The engine is
// borrowed and must outlive this graph. Pixel preprocessing is not performed here: the
// caller submits preprocessed model input tensors with the source frame identity. A raw
// frame submission is rejected so the neutral preprocessing stage stays explicit.
class qnn_inference_graph final : public inference_graph_port {
public:
    explicit qnn_inference_graph(qnn_engine& _engine) noexcept;
    qnn_inference_graph(const qnn_inference_graph& _other) = delete;
    qnn_inference_graph& operator=(const qnn_inference_graph& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_ports_infgr_validate_activation() const override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_configure(
        const inference_plan& _plan) override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_load() override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_poll_state() override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_bind_source(
        const source_binding& _binding) override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_start(
        const std::vector<tensor_spec>& _outputs, std::uint64_t _max_output_bytes) override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_arm(
        std::uint64_t _cycle_id, std::uint64_t _source_epoch,
        std::uint64_t _job_timeout_ns) override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_submit_frame(
        const raw_frame& _frame, std::uint64_t _steady_now_ns,
        submission_ticket& _ticket) override;
    [[nodiscard]] status vqec_vision_ai_ports_infgr_submit_tensors(
        const preview_frame_key& _frame, const std::vector<tensor_blob>& _inputs,
        std::uint64_t _steady_now_ns, submission_ticket& _ticket) override;
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
    [[nodiscard]] inference_capabilities
    vqec_vision_ai_ports_infgr_get_capabilities() const noexcept override;

private:
    qnn_engine& engine_;
    inference_plan plan_;
    source_binding binding_;
    std::vector<tensor_spec> input_specs_;
    std::vector<tensor_spec> engine_outputs_;
    submission_window window_;
    submission_ticket pending_ticket_;
    tensor_result pending_result_;
    inference_graph_state state_{inference_graph_state::empty};
    bool is_prepared_{false};
    bool is_window_configured_{false};
    bool has_pending_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QCOM_QNN_INFERENCE_GRAPH_HPP
