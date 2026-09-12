#include "vqec_vision_qnn_inference_graph.hpp"

#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_source_binding.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_qnn_pipeline_anchor_ns = 1000000;

bool vqec_vision_ai_qcom_qnig_same_spec(
    const tensor_spec& _left, const tensor_spec& _right) noexcept {
    return _left.name_ == _right.name_ && _left.dimensions_ == _right.dimensions_ &&
        _left.dtype_ == _right.dtype_;
}

}  // namespace

qnn_inference_graph::qnn_inference_graph(qnn_engine& _engine) noexcept
    : engine_(_engine) {}

status qnn_inference_graph::vqec_vision_ai_ports_infgr_validate_activation() const {
    if (!engine_.vqec_vision_ai_qcom_qneng_is_open()) {
        return {status_code::invalid_state, "QNN engine is not open"};
    }
    if (state_ != inference_graph_state::empty) {
        return {status_code::invalid_state, "QNN graph must be empty before activation"};
    }
    return {};
}

status qnn_inference_graph::vqec_vision_ai_ports_infgr_configure(
    const inference_plan& _plan) {
    if (state_ != inference_graph_state::empty) {
        return {status_code::invalid_state, "QNN graph is not empty"};
    }
    const auto valid = vqec_vision_ai_core_infpl_validate_plan(_plan);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    plan_ = _plan;
    state_ = inference_graph_state::configured;
    return {};
}

status qnn_inference_graph::vqec_vision_ai_ports_infgr_load() {
    if (state_ != inference_graph_state::configured) {
        return {status_code::invalid_state, "QNN graph must be configured to load"};
    }
    const auto prepared = engine_.vqec_vision_ai_qcom_qneng_prepare(plan_.model_path_);
    if (prepared.code_ != status_code::ok) {
        return prepared;
    }
    const auto tensors = engine_.vqec_vision_ai_qcom_qneng_get_tensors(
        input_specs_, engine_outputs_);
    if (tensors.code_ != status_code::ok) {
        return tensors;
    }
    is_prepared_ = true;
    state_ = inference_graph_state::ready;
    return {};
}

status qnn_inference_graph::vqec_vision_ai_ports_infgr_poll_state() {
    return {};
}

status qnn_inference_graph::vqec_vision_ai_ports_infgr_bind_source(
    const source_binding& _binding) {
    if (state_ != inference_graph_state::ready) {
        return {status_code::invalid_state, "QNN graph must be ready to bind a source"};
    }
    const auto valid = vqec_vision_ai_core_srcbd_validate_binding(_binding, plan_);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    binding_ = _binding;
    return {};
}

status qnn_inference_graph::vqec_vision_ai_ports_infgr_start(
    const std::vector<tensor_spec>& _outputs, std::uint64_t _max_output_bytes) {
    if (state_ != inference_graph_state::ready || !is_prepared_) {
        return {status_code::invalid_state, "QNN graph must be bound before start"};
    }
    (void)_max_output_bytes;
    if (_outputs.size() != engine_outputs_.size()) {
        return {status_code::unsupported,
            "declared output count differs from the composed model graph"};
    }
    for (std::size_t index = 0; index < _outputs.size(); ++index) {
        if (!vqec_vision_ai_qcom_qnig_same_spec(_outputs[index], engine_outputs_[index])) {
            return {status_code::unsupported,
                "declared output identity differs from the composed model graph"};
        }
    }
    state_ = inference_graph_state::running;
    return {};
}

status qnn_inference_graph::vqec_vision_ai_ports_infgr_arm(
    std::uint64_t _cycle_id, std::uint64_t _source_epoch, std::uint64_t _job_timeout_ns) {
    if (state_ != inference_graph_state::running || has_pending_) {
        return {status_code::invalid_state, "QNN graph cannot arm in this state"};
    }
    submission_config config;
    config.cycle_id_ = _cycle_id;
    config.source_epoch_ = _source_epoch;
    config.pipeline_anchor_ns_ = g_qnn_pipeline_anchor_ns;
    config.job_timeout_ns_ = _job_timeout_ns;
    config.capacity_ = 1;
    const auto configured = window_.vqec_vision_ai_core_subwn_configure(config);
    if (configured.code_ != status_code::ok) {
        return configured;
    }
    is_window_configured_ = true;
    return {};
}

status qnn_inference_graph::vqec_vision_ai_ports_infgr_submit_frame(
    const raw_frame& _frame, std::uint64_t _steady_now_ns, submission_ticket& _ticket) {
    (void)_frame;
    (void)_steady_now_ns;
    (void)_ticket;
    return {status_code::unsupported,
        "QNN graph requires preprocessed tensor submission from a neutral preprocessing stage"};
}

status qnn_inference_graph::vqec_vision_ai_ports_infgr_submit_tensors(
    const preview_frame_key& _frame, const std::vector<tensor_blob>& _inputs,
    std::uint64_t _steady_now_ns, submission_ticket& _ticket) {
    if (state_ != inference_graph_state::running || !is_window_configured_) {
        return {status_code::invalid_state, "QNN graph is not armed"};
    }
    if (has_pending_) {
        return {status_code::resource_exhausted, "QNN graph already has a job"};
    }
    if (_inputs.size() != input_specs_.size()) {
        return {status_code::invalid_argument,
            "tensor input count differs from the model graph"};
    }
    for (std::size_t index = 0; index < _inputs.size(); ++index) {
        if (!vqec_vision_ai_qcom_qnig_same_spec(_inputs[index].spec_, input_specs_[index]) ||
            _inputs[index].bytes_.size() !=
                vqec_vision_ai_core_tnctr_shape_bytes(input_specs_[index])) {
            return {status_code::invalid_argument,
                "tensor input does not match the model graph tensor"};
        }
    }
    submission_ticket ticket;
    const auto reserved = window_.vqec_vision_ai_core_subwn_reserve(
        _frame.source_epoch_, _frame.frame_id_, _frame.source_pts_ns_, _steady_now_ns,
        ticket);
    if (reserved.code_ != status_code::ok) {
        return reserved;
    }
    tensor_result result;
    const auto executed = engine_.vqec_vision_ai_qcom_qneng_execute(_inputs, result.tensors_);
    if (executed.code_ != status_code::ok) {
        (void)window_.vqec_vision_ai_core_subwn_cancel_reserved(ticket.token_);
        return executed;
    }
    const auto committed = window_.vqec_vision_ai_core_subwn_commit(ticket.token_);
    if (committed.code_ != status_code::ok) {
        return committed;
    }
    result.pipeline_pts_ns_ = ticket.pipeline_pts_ns_;
    pending_ticket_ = ticket;
    pending_result_ = std::move(result);
    has_pending_ = true;
    _ticket = ticket;
    return {};
}

status qnn_inference_graph::vqec_vision_ai_ports_infgr_poll_result(
    std::uint64_t _steady_now_ns, tensor_result& _result) {
    (void)_steady_now_ns;
    if (state_ != inference_graph_state::running &&
        state_ != inference_graph_state::draining &&
        state_ != inference_graph_state::drained) {
        return {status_code::invalid_state, "QNN graph cannot produce a result"};
    }
    if (!has_pending_) {
        return {status_code::pending, "QNN graph has no pending result"};
    }
    _result = std::move(pending_result_);
    (void)window_.vqec_vision_ai_core_subwn_complete_input(pending_ticket_.token_);
    (void)window_.vqec_vision_ai_core_subwn_complete_result(pending_ticket_.token_);
    pending_ticket_ = {};
    pending_result_ = {};
    has_pending_ = false;
    return {};
}

status qnn_inference_graph::vqec_vision_ai_ports_infgr_request_drain() {
    if (state_ != inference_graph_state::running) {
        return {status_code::invalid_state, "QNN graph is not running"};
    }
    state_ = inference_graph_state::drained;
    return {};
}

status qnn_inference_graph::vqec_vision_ai_ports_infgr_unload() {
    if (has_pending_) {
        return {status_code::pending, "QNN graph still holds a job"};
    }
    if (state_ != inference_graph_state::drained &&
        state_ != inference_graph_state::configured &&
        state_ != inference_graph_state::ready) {
        return {status_code::invalid_state, "QNN graph cannot unload in this state"};
    }
    is_window_configured_ = false;
    is_prepared_ = false;
    state_ = inference_graph_state::configured;
    return {};
}

inference_graph_state
qnn_inference_graph::vqec_vision_ai_ports_infgr_get_state() const noexcept {
    return state_;
}

unsigned qnn_inference_graph::vqec_vision_ai_ports_infgr_get_outstanding() const noexcept {
    return has_pending_ ? 1U : 0U;
}

submission_ticket qnn_inference_graph::
vqec_vision_ai_ports_infgr_get_pending_ticket() const noexcept {
    return pending_ticket_;
}

inference_capabilities qnn_inference_graph::
vqec_vision_ai_ports_infgr_get_capabilities() const noexcept {
    inference_capabilities capabilities;
    if (engine_.vqec_vision_ai_qcom_qneng_probe_capabilities(capabilities).code_ ==
        status_code::ok) {
        return capabilities;
    }
    return inference_graph_port::vqec_vision_ai_ports_infgr_get_capabilities();
}

}  // namespace vqec::vision::ai
