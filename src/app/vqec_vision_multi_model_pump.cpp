#include "vqec_vision_multi_model_pump.hpp"

#include <limits>
#include <utility>

namespace vqec::vision::ai {

multi_model_pump::multi_model_pump(raw_source_port& _source) : source_(_source) {}

status multi_model_pump::vqec_vision_ai_appl_mmump_configure(
    const model_cadence_config& _cadence,
    const std::array<multi_model_graph_binding,
        deployment_limits::g_max_models_per_source>& _bindings,
    std::uint16_t _binding_count) {
    if (is_configured_) {
        return {status_code::invalid_state, "multi-model pump is already configured"};
    }
    if (_binding_count == 0 ||
        _binding_count > deployment_limits::g_max_models_per_source ||
        _binding_count != _cadence.model_count_) {
        return {status_code::invalid_argument, "graph and cadence counts differ"};
    }
    std::array<multi_model_graph_binding, deployment_limits::g_max_models_per_source>
        bindings{};
    for (std::uint16_t slot = 0; slot < _binding_count; ++slot) {
        const auto& binding = _bindings[slot];
        if (binding.graph_ == nullptr || binding.cycle_id_ == 0 ||
            binding.job_timeout_ns_ == 0 ||
            binding.job_timeout_ns_ == std::numeric_limits<std::uint64_t>::max()) {
            return {status_code::invalid_argument, "invalid model graph binding"};
        }
        if ((binding.processor_ == nullptr) != (binding.plan_ == nullptr)) {
            return {status_code::invalid_argument,
                "preprocessing requires both a processor and an inference plan"};
        }
        for (std::uint16_t prior = 0; prior < slot; ++prior) {
            if (_bindings[prior].graph_ == binding.graph_) {
                return {status_code::invalid_argument, "model graph is bound more than once"};
            }
        }
        bindings[slot] = binding;
    }
    model_cadence_scheduler cadence;
    const auto configured = cadence.vqec_vision_ai_sched_mdcad_configure(_cadence);
    if (configured.code_ != status_code::ok) {
        return configured;
    }
    bindings_ = bindings;
    cadence_ = cadence;
    model_count_ = _binding_count;
    has_target_spec_.fill(false);
    is_configured_ = true;
    return {};
}

void multi_model_pump::vqec_vision_ai_appl_mmump_begin_stop() noexcept {
    is_stopping_ = true;
}

status multi_model_pump::vqec_vision_ai_appl_mmump_poll_result(
    std::uint64_t _steady_now_ns, tensor_result& _result,
    multi_model_pump_report& _report) {
    for (std::uint16_t offset = 0; offset < model_count_; ++offset) {
        const auto slot = static_cast<std::uint16_t>(
            (result_cursor_ + offset) % model_count_);
        auto& graph = *bindings_[slot].graph_;
        if (graph.vqec_vision_ai_ports_infgr_get_outstanding() == 0) {
            continue;
        }
        const auto ticket = graph.vqec_vision_ai_ports_infgr_get_pending_ticket();
        tensor_result candidate;
        const auto polled = graph.vqec_vision_ai_ports_infgr_poll_result(
            _steady_now_ns, candidate);
        if (polled.code_ == status_code::ok) {
            _result = std::move(candidate);
            _report.result_ticket_ = ticket;
            _report.result_model_slot_ = slot;
            _report.has_result_ = true;
            result_cursor_ = static_cast<std::uint16_t>((slot + 1U) % model_count_);
            return {};
        }
        if (polled.code_ != status_code::pending) {
            _report.error_model_slot_ = slot;
            is_failed_ = true;
            return polled;
        }
    }
    return {status_code::pending, "no model result available"};
}

status multi_model_pump::vqec_vision_ai_appl_mmump_pump_step(
    std::uint64_t _steady_now_ns, tensor_result& _result,
    multi_model_pump_report& _report) {
    _report = {};
    if (!is_configured_) {
        return {status_code::invalid_state, "multi-model pump is not configured"};
    }
    if (_steady_now_ns == std::numeric_limits<std::uint64_t>::max() ||
        _steady_now_ns < last_now_ns_) {
        return {status_code::invalid_argument, "pump requires monotonic steady time"};
    }
    last_now_ns_ = _steady_now_ns;
    if (is_failed_) {
        return {status_code::invalid_state, "pump failed; source session must drain"};
    }

    const auto result = vqec_vision_ai_appl_mmump_poll_result(
        _steady_now_ns, _result, _report);
    if (result.code_ != status_code::pending) {
        return result;
    }
    if (is_stopping_) {
        return {status_code::pending, "pump stopped receiving; graph drain is external"};
    }
    if (source_.vqec_vision_ai_ports_rawsr_get_state() != raw_source_state::running) {
        is_failed_ = true;
        return {status_code::invalid_state, "pump requires a running RAW source"};
    }

    bool has_available_graph = false;
    for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
        const auto& graph = *bindings_[slot].graph_;
        if (graph.vqec_vision_ai_ports_infgr_get_state() !=
            inference_graph_state::running) {
            _report.error_model_slot_ = slot;
            is_failed_ = true;
            return {status_code::invalid_state, "pump requires every model graph running"};
        }
        has_available_graph = has_available_graph ||
            graph.vqec_vision_ai_ports_infgr_get_outstanding() == 0;
    }
    if (!has_available_graph) {
        return {status_code::pending, "all model graphs have outstanding jobs"};
    }

    raw_frame frame;
    const auto received = source_.vqec_vision_ai_ports_rawsr_receive(frame, 0);
    if (received.code_ == status_code::timeout) {
        return {status_code::pending, "no RAW frame available"};
    }
    if (received.code_ != status_code::ok) {
        is_failed_ = true;
        return received;
    }
    if (!frame.owner_ || frame.descriptor_.buffer_id_ == 0 ||
        frame.descriptor_.session_epoch_ == 0) {
        is_failed_ = true;
        return {status_code::protocol_error, "RAW frame has no valid identity or owner"};
    }

    model_cadence_selection selection;
    const auto selected = cadence_.vqec_vision_ai_sched_mdcad_select(
        frame.descriptor_.buffer_id_, selection);
    if (selected.code_ != status_code::ok) {
        is_failed_ = true;
        return selected;
    }
    _report.due_model_mask_ = selection.due_model_mask_;
    _report.skipped_cadence_intervals_ = selection.skipped_intervals_;

    // Reserve every newly due graph before the first submit. In particular, a shared
    // backend retention domain must fail before any graph starts reading this frame.
    for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
        const auto bit = static_cast<std::uint16_t>(1U << slot);
        if ((selection.due_model_mask_ & bit) == 0) {
            continue;
        }
        auto& graph = *bindings_[slot].graph_;
        if (graph.vqec_vision_ai_ports_infgr_get_outstanding() != 0) {
            _report.busy_model_mask_ = static_cast<std::uint16_t>(
                _report.busy_model_mask_ | bit);
            continue;
        }
        if (!is_armed_[slot]) {
            const auto armed = graph.vqec_vision_ai_ports_infgr_arm(
                bindings_[slot].cycle_id_, frame.descriptor_.session_epoch_,
                bindings_[slot].job_timeout_ns_);
            if (armed.code_ != status_code::ok) {
                _report.error_model_slot_ = slot;
                is_failed_ = true;
                return armed;
            }
            is_armed_[slot] = true;
        }
    }

    for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
        const auto bit = static_cast<std::uint16_t>(1U << slot);
        if ((selection.due_model_mask_ & bit) == 0 ||
            (_report.busy_model_mask_ & bit) != 0) {
            continue;
        }
        auto& graph = *bindings_[slot].graph_;
        submission_ticket ticket;
        status submitted;
        if (bindings_[slot].processor_ != nullptr) {
            if (!has_target_spec_[slot]) {
                std::vector<tensor_spec> inputs;
                const auto specs = graph.vqec_vision_ai_ports_infgr_get_input_specs(inputs);
                if (specs.code_ != status_code::ok || inputs.size() != 1) {
                    _report.error_model_slot_ = slot;
                    is_failed_ = true;
                    return specs.code_ == status_code::ok ?
                        status{status_code::unsupported,
                            "tensor preprocessing requires exactly one model input"} :
                        specs;
                }
                target_specs_[slot] = inputs[0];
                has_target_spec_[slot] = true;
            }
            std::vector<tensor_blob> blobs;
            const auto preprocessed =
                bindings_[slot].processor_->vqec_vision_ai_ports_imgpr_preprocess(
                    frame, *bindings_[slot].plan_, target_specs_[slot], blobs);
            if (preprocessed.code_ != status_code::ok) {
                _report.error_model_slot_ = slot;
                is_failed_ = true;
                return preprocessed;
            }
            submitted = graph.vqec_vision_ai_ports_infgr_submit_tensors(
                frame.descriptor_.session_epoch_, frame.descriptor_.buffer_id_,
                frame.descriptor_.pts_ns_, blobs, _steady_now_ns, ticket);
        } else {
            submitted = graph.vqec_vision_ai_ports_infgr_submit_frame(
                frame, _steady_now_ns, ticket);
        }
        if (ticket.token_.job_id_ != 0) {
            _report.submitted_tickets_[slot] = ticket;
            _report.submitted_model_mask_ = static_cast<std::uint16_t>(
                _report.submitted_model_mask_ | bit);
        }
        if (submitted.code_ == status_code::pending &&
            ticket.token_.job_id_ == 0) {
            _report.busy_model_mask_ = static_cast<std::uint16_t>(
                _report.busy_model_mask_ | bit);
            continue;
        }
        if (submitted.code_ != status_code::ok) {
            _report.error_model_slot_ = slot;
            is_failed_ = true;
            return submitted;
        }
    }
    if (_report.submitted_model_mask_ != 0) {
        return {};
    }
    return {status_code::pending, "RAW frame skipped because no due graph accepted it"};
}

std::uint16_t multi_model_pump::vqec_vision_ai_appl_mmump_get_model_count()
    const noexcept {
    return model_count_;
}

bool multi_model_pump::vqec_vision_ai_appl_mmump_has_failed() const noexcept {
    return is_failed_;
}

}  // namespace vqec::vision::ai
