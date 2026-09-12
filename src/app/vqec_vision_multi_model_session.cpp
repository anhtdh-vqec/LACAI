#include "vqec_vision_multi_model_session.hpp"

#include <limits>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_appl_mmses_has_same_source(
    const inference_plan& _left, const inference_plan& _right) noexcept {
    return _left.source_width_ == _right.source_width_ &&
        _left.source_height_ == _right.source_height_ &&
        static_cast<std::uint64_t>(_left.fps_numerator_) * _right.fps_denominator_ ==
        static_cast<std::uint64_t>(_right.fps_numerator_) * _left.fps_denominator_;
}

std::uint16_t vqec_vision_ai_appl_mmses_find_first_slot(
    std::uint16_t _mask, std::uint16_t _model_count) noexcept {
    for (std::uint16_t slot = 0; slot < _model_count; ++slot) {
        if ((_mask & static_cast<std::uint16_t>(1U << slot)) != 0) {
            return slot;
        }
    }
    return g_invalid_model_slot;
}

}  // namespace

multi_model_session::multi_model_session(
    raw_source_port& _source, multi_model_session_config _config)
    : source_(_source), config_(std::move(_config)), pump_(source_) {}

status multi_model_session::vqec_vision_ai_appl_mmses_check_time(
    std::uint64_t _steady_now_ns) {
    if (_steady_now_ns == std::numeric_limits<std::uint64_t>::max() ||
        _steady_now_ns < last_now_ns_) {
        return {status_code::invalid_argument, "session requires monotonic steady time"};
    }
    last_now_ns_ = _steady_now_ns;
    return {};
}

void multi_model_session::vqec_vision_ai_appl_mmses_record_error(
    const status& _error) {
    if (last_error_.code_ == status_code::ok &&
        _error.code_ != status_code::ok && _error.code_ != status_code::pending) {
        last_error_ = _error;
    }
}

status multi_model_session::vqec_vision_ai_appl_mmses_prepare_activation() {
    if (config_.graph_count_ == 0 ||
        config_.graph_count_ > deployment_limits::g_max_models_per_source ||
        config_.cadence_.model_count_ != config_.graph_count_ ||
        config_.startup_timeout_ns_ == 0 ||
        config_.startup_timeout_ns_ == std::numeric_limits<std::uint64_t>::max() ||
        config_.stop_timeout_ns_ == 0 ||
        config_.stop_timeout_ns_ == std::numeric_limits<std::uint64_t>::max() ||
        config_.rpc_timeout_ms_ < 1 || config_.rpc_timeout_ms_ > 60000) {
        return {status_code::invalid_argument, "invalid multi-model session configuration"};
    }
    if (source_.vqec_vision_ai_ports_rawsr_get_state() != raw_source_state::idle) {
        return {status_code::invalid_state, "multi-model session requires an idle source"};
    }

    const auto& source_plan = config_.graphs_[0].plan_;
    if (config_.cadence_.source_fps_numerator_ != source_plan.fps_numerator_ ||
        config_.cadence_.source_fps_denominator_ != source_plan.fps_denominator_) {
        return {status_code::invalid_argument, "cadence source rate differs from graph plan"};
    }

    std::array<multi_model_graph_binding, deployment_limits::g_max_models_per_source>
        pump_bindings{};
    for (std::uint16_t slot = 0; slot < config_.graph_count_; ++slot) {
        const auto& graph_config = config_.graphs_[slot];
        if (graph_config.graph_ == nullptr || graph_config.cycle_id_ == 0 ||
            graph_config.job_timeout_ns_ == 0 ||
            graph_config.job_timeout_ns_ == std::numeric_limits<std::uint64_t>::max() ||
            !vqec_vision_ai_appl_mmses_has_same_source(source_plan, graph_config.plan_)) {
            return {status_code::invalid_argument, "invalid or mismatched graph configuration"};
        }
        const auto plan = vqec_vision_ai_core_infpl_validate_plan(graph_config.plan_);
        if (plan.code_ != status_code::ok) {
            return plan;
        }
        for (std::uint16_t prior = 0; prior < slot; ++prior) {
            if (config_.graphs_[prior].graph_ == graph_config.graph_ ||
                config_.graphs_[prior].cycle_id_ == graph_config.cycle_id_) {
                return {status_code::invalid_argument, "graph or cycle identity is duplicated"};
            }
        }
        if (graph_config.graph_->vqec_vision_ai_ports_infgr_get_state() !=
            inference_graph_state::empty) {
            return {status_code::invalid_state, "every model graph must initially be empty"};
        }
        const auto activation =
            graph_config.graph_->vqec_vision_ai_ports_infgr_validate_activation();
        if (activation.code_ != status_code::ok) {
            return activation;
        }
        const auto binding = vqec_vision_ai_core_srcbd_validate_binding(
            graph_config.binding_, graph_config.plan_);
        if (binding.code_ != status_code::ok) {
            return binding;
        }
        std::uint64_t required_bytes = 0;
        const auto outputs = vqec_vision_ai_core_tnctr_validate_outputs(
            graph_config.outputs_, graph_config.max_output_bytes_, required_bytes);
        if (outputs.code_ != status_code::ok) {
            return outputs;
        }
        pump_bindings[slot].graph_ = graph_config.graph_;
        pump_bindings[slot].cycle_id_ = graph_config.cycle_id_;
        pump_bindings[slot].job_timeout_ns_ = graph_config.job_timeout_ns_;
        pump_bindings[slot].processor_ = graph_config.processor_;
        pump_bindings[slot].plan_ = graph_config.processor_ != nullptr ?
            &config_.graphs_[slot].plan_ : nullptr;
    }
    return pump_.vqec_vision_ai_appl_mmump_configure(
        config_.cadence_, pump_bindings, config_.graph_count_);
}

status multi_model_session::vqec_vision_ai_appl_mmses_start_graph() {
    auto& graph_config = config_.graphs_[active_graph_slot_];
    auto& graph = *graph_config.graph_;
    status progress;
    switch (state_) {
        case multi_model_session_state::configuring:
            progress = graph.vqec_vision_ai_ports_infgr_configure(graph_config.plan_);
            if (progress.code_ == status_code::ok) {
                state_ = multi_model_session_state::loading;
            }
            break;
        case multi_model_session_state::loading:
            progress = graph.vqec_vision_ai_ports_infgr_get_state() ==
                    inference_graph_state::configured ?
                graph.vqec_vision_ai_ports_infgr_load() :
                graph.vqec_vision_ai_ports_infgr_poll_state();
            if (graph.vqec_vision_ai_ports_infgr_get_state() ==
                inference_graph_state::ready) {
                state_ = multi_model_session_state::binding;
            }
            break;
        case multi_model_session_state::binding:
            progress = graph.vqec_vision_ai_ports_infgr_bind_source(
                graph_config.binding_);
            if (progress.code_ == status_code::ok) {
                state_ = multi_model_session_state::starting;
            }
            break;
        case multi_model_session_state::starting:
            progress = graph.vqec_vision_ai_ports_infgr_get_state() ==
                    inference_graph_state::ready ?
                graph.vqec_vision_ai_ports_infgr_start(
                    graph_config.outputs_, graph_config.max_output_bytes_) :
                graph.vqec_vision_ai_ports_infgr_poll_state();
            if (graph.vqec_vision_ai_ports_infgr_get_state() ==
                inference_graph_state::running) {
                ++active_graph_slot_;
                if (active_graph_slot_ == config_.graph_count_) {
                    // Every graph is loaded; resolve preprocessing targets now so no frame
                    // is received before the model input identity is known (S04/O07).
                    progress = pump_.vqec_vision_ai_appl_mmump_resolve_targets();
                    if (progress.code_ != status_code::ok) {
                        break;
                    }
                    state_ = multi_model_session_state::running;
                } else {
                    state_ = multi_model_session_state::configuring;
                }
            }
            break;
        default:
            return {status_code::invalid_state, "session is not starting a graph"};
    }
    return progress;
}

status multi_model_session::vqec_vision_ai_appl_mmses_request_stop(
    std::uint64_t _steady_now_ns) {
    const auto time = vqec_vision_ai_appl_mmses_check_time(_steady_now_ns);
    if (time.code_ != status_code::ok) {
        return time;
    }
    if (state_ == multi_model_session_state::stopped ||
        state_ == multi_model_session_state::draining_graphs ||
        state_ == multi_model_session_state::releasing_source) {
        return {};
    }
    pump_.vqec_vision_ai_appl_mmump_begin_stop();
    if (state_ == multi_model_session_state::idle) {
        state_ = multi_model_session_state::stopped;
        return {};
    }
    stop_ns_ = _steady_now_ns;
    drain_graph_slot_ = 0;
    state_ = multi_model_session_state::draining_graphs;
    return {};
}

status multi_model_session::vqec_vision_ai_appl_mmses_stop_graph(
    std::uint64_t _steady_now_ns) {
    if (drain_graph_slot_ >= config_.graph_count_) {
        state_ = multi_model_session_state::releasing_source;
        return {status_code::pending, "all graphs reconciled; source release is next"};
    }
    auto& graph = *config_.graphs_[drain_graph_slot_].graph_;
    auto graph_state = graph.vqec_vision_ai_ports_infgr_get_state();
    if (graph_state == inference_graph_state::running) {
        return graph.vqec_vision_ai_ports_infgr_request_drain();
    }
    if (graph.vqec_vision_ai_ports_infgr_get_outstanding() != 0) {
        tensor_result discarded;
        const auto result = graph.vqec_vision_ai_ports_infgr_poll_result(
            _steady_now_ns, discarded);
        if (graph.vqec_vision_ai_ports_infgr_get_outstanding() != 0) {
            return result.code_ == status_code::ok ?
                status{status_code::pending, "waiting for graph readers"} : result;
        }
        if (result.code_ != status_code::ok && result.code_ != status_code::pending) {
            return result;
        }
    }
    graph_state = graph.vqec_vision_ai_ports_infgr_get_state();
    if (graph_state == inference_graph_state::empty ||
        graph_state == inference_graph_state::configured) {
        ++drain_graph_slot_;
        return {status_code::pending, "graph slot reconciled"};
    }
    if (graph_state == inference_graph_state::loading ||
        graph_state == inference_graph_state::starting ||
        graph_state == inference_graph_state::draining ||
        graph_state == inference_graph_state::unloading) {
        return graph.vqec_vision_ai_ports_infgr_poll_state();
    }
    return graph.vqec_vision_ai_ports_infgr_unload();
}

status multi_model_session::vqec_vision_ai_appl_mmses_step(
    std::uint64_t _steady_now_ns, tensor_result& _result,
    source_session_progress& _progress) {
    _progress = {};
    const auto time = vqec_vision_ai_appl_mmses_check_time(_steady_now_ns);
    if (time.code_ != status_code::ok) {
        return time;
    }
    if (state_ == multi_model_session_state::stopped) {
        return {};
    }
    if (state_ == multi_model_session_state::idle) {
        const auto prepared = vqec_vision_ai_appl_mmses_prepare_activation();
        if (prepared.code_ != status_code::ok) {
            return prepared;
        }
        start_ns_ = _steady_now_ns;
        state_ = multi_model_session_state::acquiring;
        return {status_code::pending, "validated; source acquisition is next"};
    }

    const bool is_stopping = state_ == multi_model_session_state::draining_graphs ||
        state_ == multi_model_session_state::releasing_source;
    if (!is_stopping && state_ != multi_model_session_state::running &&
        _steady_now_ns - start_ns_ >= config_.startup_timeout_ns_) {
        const status expired{status_code::timeout, "multi-model startup deadline elapsed"};
        vqec_vision_ai_appl_mmses_record_error(expired);
        const auto stopped = vqec_vision_ai_appl_mmses_request_stop(_steady_now_ns);
        (void)stopped;
        return expired;
    }

    status progress;
    if (state_ == multi_model_session_state::acquiring) {
        progress = source_.vqec_vision_ai_ports_rawsr_start(config_.rpc_timeout_ms_);
        if (progress.code_ == status_code::ok) {
            const auto profile = source_.vqec_vision_ai_ports_rawsr_get_profile();
            const auto& plan = config_.graphs_[0].plan_;
            if (profile.width_ != plan.source_width_ ||
                profile.height_ != plan.source_height_ ||
                static_cast<std::uint64_t>(profile.fps_numerator_) *
                        plan.fps_denominator_ !=
                    static_cast<std::uint64_t>(plan.fps_numerator_) *
                        profile.fps_denominator_) {
                progress = {status_code::unsupported,
                            "FW effective profile differs from graph plans"};
            } else {
                state_ = multi_model_session_state::configuring;
            }
        } else if ((progress.code_ == status_code::timeout ||
                    progress.code_ == status_code::source_lost) &&
                   source_.vqec_vision_ai_ports_rawsr_get_state() !=
                       raw_source_state::draining) {
            vqec_vision_ai_appl_mmses_record_error(progress);
            return {status_code::pending, "source acquisition remains pending"};
        }
    } else if (state_ == multi_model_session_state::configuring ||
               state_ == multi_model_session_state::loading ||
               state_ == multi_model_session_state::binding ||
               state_ == multi_model_session_state::starting) {
        progress = vqec_vision_ai_appl_mmses_start_graph();
    } else if (state_ == multi_model_session_state::running) {
        multi_model_pump_report report;
        progress = pump_.vqec_vision_ai_appl_mmump_pump_step(
            _steady_now_ns, _result, report);
        _progress.due_model_mask_ = report.due_model_mask_;
        _progress.submitted_model_mask_ = report.submitted_model_mask_;
        _progress.busy_model_mask_ = report.busy_model_mask_;
        _progress.error_model_slot_ = report.error_model_slot_;
        _progress.has_submission_ = report.submitted_model_mask_ != 0;
        _progress.has_result_ = report.has_result_;
        _progress.model_slot_ = report.has_result_ ? report.result_model_slot_ :
            vqec_vision_ai_appl_mmses_find_first_slot(
                report.submitted_model_mask_, config_.graph_count_);
        if (report.has_result_) {
            _progress.ticket_ = report.result_ticket_;
        } else if (_progress.model_slot_ != g_invalid_model_slot) {
            _progress.ticket_ = report.submitted_tickets_[_progress.model_slot_];
        }
    } else if (state_ == multi_model_session_state::draining_graphs) {
        progress = vqec_vision_ai_appl_mmses_stop_graph(_steady_now_ns);
    } else if (state_ == multi_model_session_state::releasing_source) {
        progress = source_.vqec_vision_ai_ports_rawsr_stop(config_.rpc_timeout_ms_);
        if (source_.vqec_vision_ai_ports_rawsr_get_state() == raw_source_state::stopped) {
            state_ = multi_model_session_state::stopped;
            is_recovery_required_ = false;
            return {};
        }
    } else {
        return {status_code::invalid_state, "unexpected multi-model session state"};
    }

    if (progress.code_ != status_code::ok && progress.code_ != status_code::pending) {
        vqec_vision_ai_appl_mmses_record_error(progress);
        if (!is_stopping) {
            const auto stopped = vqec_vision_ai_appl_mmses_request_stop(_steady_now_ns);
            (void)stopped;
        }
    }
    if (is_stopping && _steady_now_ns - stop_ns_ >= config_.stop_timeout_ns_) {
        is_recovery_required_ = true;
        const status expired{status_code::timeout,
                             "stop deadline elapsed; retain resources for recovery"};
        vqec_vision_ai_appl_mmses_record_error(expired);
        return expired;
    }
    if (progress.code_ == status_code::ok &&
        state_ != multi_model_session_state::running &&
        state_ != multi_model_session_state::stopped) {
        return {status_code::pending, "multi-model lifecycle transition is pending"};
    }
    return progress;
}

multi_model_session_snapshot
multi_model_session::vqec_vision_ai_appl_mmses_get_snapshot() const noexcept {
    multi_model_session_snapshot snapshot;
    snapshot.session_state_ = state_;
    snapshot.source_state_ = source_.vqec_vision_ai_ports_rawsr_get_state();
    snapshot.first_error_code_ = last_error_.code_;
    snapshot.source_readers_ = source_.vqec_vision_ai_ports_rawsr_get_outstanding();
    snapshot.graph_count_ = config_.graph_count_;
    snapshot.is_recovery_required_ = is_recovery_required_;
    for (std::uint16_t slot = 0;
         slot < config_.graph_count_ &&
         slot < deployment_limits::g_max_models_per_source; ++slot) {
        if (config_.graphs_[slot].graph_ == nullptr) {
            continue;
        }
        const auto& graph = *config_.graphs_[slot].graph_;
        snapshot.graph_states_[slot] =
            graph.vqec_vision_ai_ports_infgr_get_state();
        if (snapshot.graph_states_[slot] == inference_graph_state::running) {
            ++snapshot.running_graph_count_;
        }
        const auto outstanding = graph.vqec_vision_ai_ports_infgr_get_outstanding();
        if (outstanding > std::numeric_limits<unsigned>::max() -
                snapshot.outstanding_jobs_) {
            snapshot.outstanding_jobs_ = std::numeric_limits<unsigned>::max();
        } else {
            snapshot.outstanding_jobs_ += outstanding;
        }
    }
    return snapshot;
}

const status& multi_model_session::vqec_vision_ai_appl_mmses_get_last_error()
    const noexcept {
    return last_error_;
}

status multi_model_session::vqec_vision_ai_appl_srcsn_step(
    std::uint64_t _steady_now_ns, tensor_result& _result,
    source_session_progress& _progress) {
    return vqec_vision_ai_appl_mmses_step(_steady_now_ns, _result, _progress);
}

status multi_model_session::vqec_vision_ai_appl_srcsn_request_stop(
    std::uint64_t _steady_now_ns) {
    return vqec_vision_ai_appl_mmses_request_stop(_steady_now_ns);
}

source_session_health
multi_model_session::vqec_vision_ai_appl_srcsn_get_health() const noexcept {
    const auto snapshot = vqec_vision_ai_appl_mmses_get_snapshot();
    source_session_health health;
    if (snapshot.session_state_ == multi_model_session_state::idle) {
        health.phase_ = source_session_phase::idle;
    } else if (snapshot.session_state_ == multi_model_session_state::running) {
        health.phase_ = source_session_phase::running;
    } else if (snapshot.session_state_ == multi_model_session_state::draining_graphs ||
               snapshot.session_state_ == multi_model_session_state::releasing_source) {
        health.phase_ = source_session_phase::draining;
    } else if (snapshot.session_state_ == multi_model_session_state::stopped) {
        health.phase_ = source_session_phase::stopped;
    } else {
        health.phase_ = source_session_phase::starting;
    }
    health.model_graph_count_ = snapshot.graph_count_;
    health.running_graph_count_ = snapshot.running_graph_count_;
    health.outstanding_jobs_ = snapshot.outstanding_jobs_;
    health.source_readers_ = snapshot.source_readers_;
    health.is_recovery_required_ = snapshot.is_recovery_required_;
    health.first_error_code_ = snapshot.first_error_code_;
    return health;
}

}  // namespace vqec::vision::ai
