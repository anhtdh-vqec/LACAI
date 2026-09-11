#include "vqec_vision_reference_graph.hpp"

#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_source_binding.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

tensor_result vqec_vision_ai_refer_rfgph_make_zero_result(
    const std::vector<float_tensor_spec>& _outputs, std::uint64_t _pipeline_pts_ns,
    std::uint64_t _max_output_bytes) {
    tensor_result result;
    result.pipeline_pts_ns_ = _pipeline_pts_ns;
    for (const auto& spec : _outputs) {
        std::uint64_t elements = 1;
        for (const auto dimension : spec.dimensions_) {
            elements *= dimension;
        }
        float_tensor_result tensor;
        tensor.spec_ = spec;
        if (elements <= _max_output_bytes / sizeof(float)) {
            tensor.values_.assign(static_cast<std::size_t>(elements), 0.0F);
        }
        result.tensors_.push_back(std::move(tensor));
    }
    return result;
}

}  // namespace

reference_inference_graph::reference_inference_graph(reference_graph_config _config) noexcept
    : config_(_config) {}

reference_inference_graph::~reference_inference_graph() noexcept = default;

status reference_inference_graph::vqec_vision_ai_ports_infgr_validate_activation() const {
    return {};
}

status reference_inference_graph::vqec_vision_ai_ports_infgr_configure(
    const inference_plan& _plan) {
    if (state_ != inference_graph_state::empty) {
        return {status_code::invalid_state, "reference graph is not empty"};
    }
    const auto valid = vqec_vision_ai_core_infpl_validate_plan(_plan);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    plan_ = _plan;
    state_ = inference_graph_state::configured;
    return {};
}

status reference_inference_graph::vqec_vision_ai_ports_infgr_load() {
    if (state_ != inference_graph_state::configured) {
        return {status_code::invalid_state, "reference graph must be configured to load"};
    }
    state_ = inference_graph_state::ready;
    return {};
}

status reference_inference_graph::vqec_vision_ai_ports_infgr_poll_state() {
    return {};
}

status reference_inference_graph::vqec_vision_ai_ports_infgr_bind_source(
    const source_binding& _binding) {
    if (state_ != inference_graph_state::ready) {
        return {status_code::invalid_state, "reference graph must be ready to bind"};
    }
    return vqec_vision_ai_core_srcbd_validate_binding(_binding, plan_);
}

status reference_inference_graph::vqec_vision_ai_ports_infgr_start(
    const std::vector<float_tensor_spec>& _outputs, std::uint64_t _max_output_bytes) {
    if (state_ != inference_graph_state::ready) {
        return {status_code::invalid_state, "reference graph must be bound to start"};
    }
    std::uint64_t required_bytes = 0;
    const auto valid = vqec_vision_ai_core_tnctr_validate_outputs(
        _outputs, _max_output_bytes, required_bytes);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    outputs_ = _outputs;
    max_output_bytes_ = _max_output_bytes;
    state_ = inference_graph_state::running;
    return {};
}

status reference_inference_graph::vqec_vision_ai_ports_infgr_arm(
    std::uint64_t _cycle_id, std::uint64_t _source_epoch, std::uint64_t _job_timeout_ns) {
    if (state_ != inference_graph_state::running || has_pending_) {
        return {status_code::invalid_state, "reference graph cannot arm in this state"};
    }
    submission_config config;
    config.cycle_id_ = _cycle_id;
    config.source_epoch_ = _source_epoch;
    config.pipeline_anchor_ns_ = config_.pipeline_anchor_ns_;
    config.job_timeout_ns_ = _job_timeout_ns;
    config.capacity_ = 1;
    const auto configured = window_.vqec_vision_ai_core_subwn_configure(config);
    if (configured.code_ != status_code::ok) {
        return configured;
    }
    is_window_configured_ = true;
    return {};
}

status reference_inference_graph::vqec_vision_ai_ports_infgr_submit_frame(
    const raw_frame& _frame, std::uint64_t _steady_now_ns, submission_ticket& _ticket) {
    if (state_ != inference_graph_state::running || !is_window_configured_) {
        return {status_code::invalid_state, "reference graph is not armed"};
    }
    if (has_pending_) {
        return {status_code::resource_exhausted, "reference graph already has a job"};
    }
    submission_ticket ticket;
    const auto reserved = window_.vqec_vision_ai_core_subwn_reserve(
        _frame.descriptor_.session_epoch_, _frame.descriptor_.buffer_id_,
        _frame.descriptor_.pts_ns_, _steady_now_ns, ticket);
    if (reserved.code_ != status_code::ok) {
        return reserved;
    }
    const auto committed = window_.vqec_vision_ai_core_subwn_commit(ticket.token_);
    if (committed.code_ != status_code::ok) {
        return committed;
    }
    pending_ticket_ = ticket;
    pending_result_ = vqec_vision_ai_refer_rfgph_make_zero_result(
        outputs_, ticket.pipeline_pts_ns_, max_output_bytes_);
    retained_owner_ = _frame.owner_;
    has_pending_ = true;
    _ticket = ticket;
    return {};
}

status reference_inference_graph::vqec_vision_ai_ports_infgr_poll_result(
    std::uint64_t _steady_now_ns, tensor_result& _result) {
    (void)_steady_now_ns;
    if (state_ != inference_graph_state::running && state_ != inference_graph_state::draining &&
        state_ != inference_graph_state::drained) {
        return {status_code::invalid_state, "reference graph cannot produce a result"};
    }
    if (!has_pending_) {
        return {status_code::pending, "reference graph has no pending result"};
    }
    _result = std::move(pending_result_);
    // The reference backend completes immediately; both ledger events are real for it.
    (void)window_.vqec_vision_ai_core_subwn_complete_input(pending_ticket_.token_);
    (void)window_.vqec_vision_ai_core_subwn_complete_result(pending_ticket_.token_);
    retained_owner_.reset();
    pending_ticket_ = {};
    has_pending_ = false;
    return {};
}

status reference_inference_graph::vqec_vision_ai_ports_infgr_request_drain() {
    if (state_ != inference_graph_state::running) {
        return {status_code::invalid_state, "reference graph is not running"};
    }
    state_ = inference_graph_state::drained;
    return {};
}

status reference_inference_graph::vqec_vision_ai_ports_infgr_unload() {
    if (has_pending_) {
        return {status_code::pending, "reference graph still holds a job"};
    }
    if (state_ != inference_graph_state::drained &&
        state_ != inference_graph_state::configured &&
        state_ != inference_graph_state::ready) {
        return {status_code::invalid_state, "reference graph cannot unload in this state"};
    }
    outputs_.clear();
    retained_owner_.reset();
    has_pending_ = false;
    is_window_configured_ = false;
    state_ = inference_graph_state::configured;
    return {};
}

inference_graph_state
reference_inference_graph::vqec_vision_ai_ports_infgr_get_state() const noexcept {
    return state_;
}

unsigned reference_inference_graph::vqec_vision_ai_ports_infgr_get_outstanding()
    const noexcept {
    return has_pending_ ? 1U : 0U;
}

submission_ticket reference_inference_graph::
vqec_vision_ai_ports_infgr_get_pending_ticket() const noexcept {
    return pending_ticket_;
}

}  // namespace vqec::vision::ai
