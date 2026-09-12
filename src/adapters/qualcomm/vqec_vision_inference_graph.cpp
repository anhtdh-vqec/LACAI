#include "vqec_vision_inference_graph.hpp"

#include <limits>
#include <utility>

namespace vqec::vision::ai {
namespace {

inference_graph_state vqec_vision_ai_qcom_ifgr_map_state(
    plugin_graph_state _state) noexcept {
    switch (_state) {
        case plugin_graph_state::empty:
            return inference_graph_state::empty;
        case plugin_graph_state::configured:
            return inference_graph_state::configured;
        case plugin_graph_state::loading:
            return inference_graph_state::loading;
        case plugin_graph_state::ready:
            return inference_graph_state::ready;
        case plugin_graph_state::starting:
            return inference_graph_state::starting;
        case plugin_graph_state::playing:
            return inference_graph_state::running;
        case plugin_graph_state::draining:
            return inference_graph_state::draining;
        case plugin_graph_state::drained:
            return inference_graph_state::drained;
        case plugin_graph_state::unloading:
            return inference_graph_state::unloading;
        case plugin_graph_state::faulted:
            return inference_graph_state::faulted;
    }
    return inference_graph_state::faulted;
}

}  // namespace

qualcomm_inference_graph::qualcomm_inference_graph(
    plugin_graph& _graph, std::shared_ptr<graph_retention> _retention)
    : graph_(_graph), retention_(std::move(_retention)) {}

status qualcomm_inference_graph::vqec_vision_ai_ports_infgr_validate_activation()
    const {
    if (!retention_) {
        return {status_code::invalid_argument,
                "Qualcomm inference graph requires a retention domain"};
    }
    if (graph_.vqec_vision_ai_qcom_plgr_get_state() != plugin_graph_state::empty) {
        return {status_code::invalid_state,
                "Qualcomm inference graph must be empty before activation"};
    }
    return {};
}

status qualcomm_inference_graph::vqec_vision_ai_ports_infgr_configure(
    const inference_plan& _plan) {
    return graph_.vqec_vision_ai_qcom_plgr_configure_graph(_plan);
}

status qualcomm_inference_graph::vqec_vision_ai_ports_infgr_load() {
    return graph_.vqec_vision_ai_qcom_plgr_load_model();
}

status qualcomm_inference_graph::vqec_vision_ai_ports_infgr_poll_state() {
    return graph_.vqec_vision_ai_qcom_plgr_poll_state();
}

status qualcomm_inference_graph::vqec_vision_ai_ports_infgr_bind_source(
    const source_binding& _binding) {
    return graph_.vqec_vision_ai_qcom_plgr_bind_source(_binding);
}

status qualcomm_inference_graph::vqec_vision_ai_ports_infgr_start(
    const std::vector<tensor_spec>& _outputs,
    std::uint64_t _max_output_bytes) {
    return graph_.vqec_vision_ai_qcom_plgr_start_stream(_outputs, _max_output_bytes);
}

status qualcomm_inference_graph::vqec_vision_ai_ports_infgr_arm(
    std::uint64_t _cycle_id, std::uint64_t _source_epoch,
    std::uint64_t _job_timeout_ns) {
    return graph_.vqec_vision_ai_qcom_plgr_arm_submission(
        _cycle_id, _source_epoch, _job_timeout_ns, retention_);
}

status qualcomm_inference_graph::vqec_vision_ai_ports_infgr_submit_frame(
    const raw_frame& _frame, std::uint64_t _steady_now_ns,
    submission_ticket& _ticket) {
    if (_frame.native_handle_ < 0 ||
        _frame.native_handle_ > std::numeric_limits<int>::max() || !_frame.owner_) {
        return {status_code::unsupported,
                "Qualcomm graph requires a valid Linux DMA-BUF handle"};
    }
    return graph_.vqec_vision_ai_qcom_plgr_submit_frame(
        _frame.descriptor_, static_cast<int>(_frame.native_handle_), _frame.owner_,
        _steady_now_ns, _ticket);
}

status qualcomm_inference_graph::vqec_vision_ai_ports_infgr_poll_result(
    std::uint64_t _steady_now_ns, tensor_result& _result) {
    return graph_.vqec_vision_ai_qcom_plgr_poll_result(_steady_now_ns, _result);
}

status qualcomm_inference_graph::vqec_vision_ai_ports_infgr_request_drain() {
    return graph_.vqec_vision_ai_qcom_plgr_request_drain();
}

status qualcomm_inference_graph::vqec_vision_ai_ports_infgr_unload() {
    return graph_.vqec_vision_ai_qcom_plgr_unload_model();
}

inference_graph_state qualcomm_inference_graph::vqec_vision_ai_ports_infgr_get_state()
    const noexcept {
    return vqec_vision_ai_qcom_ifgr_map_state(
        graph_.vqec_vision_ai_qcom_plgr_get_state());
}

unsigned qualcomm_inference_graph::vqec_vision_ai_ports_infgr_get_outstanding()
    const noexcept {
    return graph_.vqec_vision_ai_qcom_plgr_get_outstanding();
}

submission_ticket qualcomm_inference_graph::vqec_vision_ai_ports_infgr_get_pending_ticket()
    const noexcept {
    return graph_.vqec_vision_ai_qcom_plgr_get_pending_ticket();
}

}  // namespace vqec::vision::ai
