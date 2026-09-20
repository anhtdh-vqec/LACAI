#include "vqec_vision_cascade_graph_session.hpp"

#include <limits>
#include <utility>

namespace vqec::vision::ai {

cascade_graph_session::cascade_graph_session(cascade_graph_session_config _config)
    : config_(std::move(_config)) {}

status cascade_graph_session::vqec_vision_ai_appl_cgses_check_time(
    std::uint64_t _steady_now_ns) {
    if (_steady_now_ns == std::numeric_limits<std::uint64_t>::max() ||
        _steady_now_ns < last_now_ns_) {
        return {status_code::invalid_argument,
            "cascade graph session requires monotonic steady time"};
    }
    last_now_ns_ = _steady_now_ns;
    return {};
}

status cascade_graph_session::vqec_vision_ai_appl_cgses_fault(status _failure) {
    if (_failure.code_ == status_code::ok || _failure.code_ == status_code::pending) {
        _failure = {status_code::invalid_state, "cascade graph entered an invalid state"};
    }
    if (last_error_.code_ == status_code::ok) {
        last_error_ = _failure;
    }
    state_ = cascade_graph_session_state::faulted;
    if (config_.graph_ != nullptr) {
        const auto graph_state = config_.graph_->vqec_vision_ai_ports_infgr_get_state();
        is_recovery_required_ = graph_state != inference_graph_state::empty &&
            graph_state != inference_graph_state::configured;
    }
    return _failure;
}

status cascade_graph_session::vqec_vision_ai_appl_cgses_validate_activation() {
    if (config_.graph_ == nullptr || config_.startup_timeout_ns_ == 0 ||
        config_.startup_timeout_ns_ == std::numeric_limits<std::uint64_t>::max() ||
        config_.stop_timeout_ns_ == 0 ||
        config_.stop_timeout_ns_ == std::numeric_limits<std::uint64_t>::max()) {
        return {status_code::invalid_argument,
            "invalid cascade graph session configuration"};
    }
    if (config_.graph_->vqec_vision_ai_ports_infgr_get_state() !=
        inference_graph_state::empty) {
        return {status_code::invalid_state, "cascade graph must initially be empty"};
    }
    const auto activation = config_.graph_->vqec_vision_ai_ports_infgr_validate_activation();
    if (activation.code_ != status_code::ok) {
        return activation;
    }
    const auto plan = vqec_vision_ai_core_infpl_validate_plan(config_.plan_);
    if (plan.code_ != status_code::ok) {
        return plan;
    }
    const auto binding = vqec_vision_ai_core_srcbd_validate_binding(
        config_.binding_, config_.plan_);
    if (binding.code_ != status_code::ok) {
        return binding;
    }
    std::uint64_t required_output_bytes = 0;
    return vqec_vision_ai_core_tnctr_validate_outputs(
        config_.outputs_, config_.max_output_bytes_, required_output_bytes);
}

status cascade_graph_session::vqec_vision_ai_appl_cgses_step_start() {
    auto& graph = *config_.graph_;
    status progress;
    switch (state_) {
        case cascade_graph_session_state::configuring:
            progress = graph.vqec_vision_ai_ports_infgr_configure(config_.plan_);
            if (progress.code_ == status_code::ok) {
                state_ = cascade_graph_session_state::loading;
            }
            break;
        case cascade_graph_session_state::loading:
            progress = graph.vqec_vision_ai_ports_infgr_get_state() ==
                    inference_graph_state::configured ?
                graph.vqec_vision_ai_ports_infgr_load() :
                graph.vqec_vision_ai_ports_infgr_poll_state();
            if (graph.vqec_vision_ai_ports_infgr_get_state() ==
                inference_graph_state::ready) {
                state_ = cascade_graph_session_state::binding;
            }
            break;
        case cascade_graph_session_state::binding:
            progress = graph.vqec_vision_ai_ports_infgr_bind_source(config_.binding_);
            if (progress.code_ == status_code::ok) {
                state_ = cascade_graph_session_state::starting;
            }
            break;
        case cascade_graph_session_state::starting:
            progress = graph.vqec_vision_ai_ports_infgr_get_state() ==
                    inference_graph_state::ready ?
                graph.vqec_vision_ai_ports_infgr_start(
                    config_.outputs_, config_.max_output_bytes_) :
                graph.vqec_vision_ai_ports_infgr_poll_state();
            if (graph.vqec_vision_ai_ports_infgr_get_state() ==
                inference_graph_state::running) {
                state_ = cascade_graph_session_state::running;
                return {};
            }
            break;
        default:
            return {status_code::invalid_state,
                "cascade graph session is not starting"};
    }
    if (progress.code_ != status_code::ok && progress.code_ != status_code::pending) {
        return vqec_vision_ai_appl_cgses_fault(progress);
    }
    return {status_code::pending, "cascade graph startup is in progress"};
}

status cascade_graph_session::vqec_vision_ai_appl_cgses_step_stop(
    std::uint64_t _steady_now_ns) {
    auto& graph = *config_.graph_;
    const auto graph_state = graph.vqec_vision_ai_ports_infgr_get_state();
    if (graph_state == inference_graph_state::empty ||
        graph_state == inference_graph_state::configured) {
        state_ = cascade_graph_session_state::stopped;
        return {};
    }
    status progress;
    if (graph_state == inference_graph_state::running) {
        progress = graph.vqec_vision_ai_ports_infgr_request_drain();
    } else if (graph.vqec_vision_ai_ports_infgr_get_outstanding() != 0) {
        tensor_result discarded;
        progress = graph.vqec_vision_ai_ports_infgr_poll_result(_steady_now_ns, discarded);
    } else if (graph_state == inference_graph_state::loading ||
               graph_state == inference_graph_state::starting ||
               graph_state == inference_graph_state::draining ||
               graph_state == inference_graph_state::unloading) {
        progress = graph.vqec_vision_ai_ports_infgr_poll_state();
    } else if (graph_state == inference_graph_state::ready ||
               graph_state == inference_graph_state::drained) {
        state_ = cascade_graph_session_state::unloading;
        progress = graph.vqec_vision_ai_ports_infgr_unload();
    } else {
        return vqec_vision_ai_appl_cgses_fault(
            {status_code::invalid_state, "cascade graph cannot be reconciled"});
    }
    if (progress.code_ != status_code::ok && progress.code_ != status_code::pending) {
        return vqec_vision_ai_appl_cgses_fault(progress);
    }
    return {status_code::pending, "cascade graph stop is in progress"};
}

status cascade_graph_session::vqec_vision_ai_appl_cgses_step(
    std::uint64_t _steady_now_ns) {
    const auto time = vqec_vision_ai_appl_cgses_check_time(_steady_now_ns);
    if (time.code_ != status_code::ok) {
        return time;
    }
    if (state_ == cascade_graph_session_state::faulted) {
        return last_error_;
    }
    if (state_ == cascade_graph_session_state::stopped ||
        state_ == cascade_graph_session_state::running) {
        return {};
    }
    if (state_ == cascade_graph_session_state::idle) {
        const auto valid = vqec_vision_ai_appl_cgses_validate_activation();
        if (valid.code_ != status_code::ok) {
            return vqec_vision_ai_appl_cgses_fault(valid);
        }
        start_ns_ = _steady_now_ns;
        state_ = cascade_graph_session_state::configuring;
        return {status_code::pending, "cascade graph validation completed"};
    }
    if (state_ == cascade_graph_session_state::draining ||
        state_ == cascade_graph_session_state::unloading) {
        if (_steady_now_ns - stop_ns_ >= config_.stop_timeout_ns_) {
            return vqec_vision_ai_appl_cgses_fault(
                {status_code::timeout, "cascade graph stop deadline elapsed"});
        }
        return vqec_vision_ai_appl_cgses_step_stop(_steady_now_ns);
    }
    if (_steady_now_ns - start_ns_ >= config_.startup_timeout_ns_) {
        return vqec_vision_ai_appl_cgses_fault(
            {status_code::timeout, "cascade graph startup deadline elapsed"});
    }
    return vqec_vision_ai_appl_cgses_step_start();
}

status cascade_graph_session::vqec_vision_ai_appl_cgses_request_stop(
    std::uint64_t _steady_now_ns) {
    return vqec_vision_ai_appl_cgses_request_active(false, _steady_now_ns);
}

status cascade_graph_session::vqec_vision_ai_appl_cgses_validate_active(
    bool _desired_active, std::uint64_t _steady_now_ns) const {
    if (_steady_now_ns == std::numeric_limits<std::uint64_t>::max() ||
        _steady_now_ns < last_now_ns_) {
        return {status_code::invalid_argument,
            "cascade activation delta requires monotonic steady time"};
    }
    if (state_ == cascade_graph_session_state::faulted) {
        return last_error_;
    }
    const bool is_starting = state_ == cascade_graph_session_state::configuring ||
        state_ == cascade_graph_session_state::loading ||
        state_ == cascade_graph_session_state::binding ||
        state_ == cascade_graph_session_state::starting;
    const bool is_stopping = state_ == cascade_graph_session_state::draining ||
        state_ == cascade_graph_session_state::unloading;
    if ((is_starting || is_stopping) && _desired_active != desired_active_) {
        return {status_code::invalid_state,
            "cascade activation delta is already in progress"};
    }
    return {};
}

status cascade_graph_session::vqec_vision_ai_appl_cgses_request_active(
    bool _desired_active, std::uint64_t _steady_now_ns) {
    const auto valid = vqec_vision_ai_appl_cgses_validate_active(
        _desired_active, _steady_now_ns);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    const auto time = vqec_vision_ai_appl_cgses_check_time(_steady_now_ns);
    if (time.code_ != status_code::ok) {
        return time;
    }
    if (_desired_active) {
        desired_active_ = true;
        if (state_ == cascade_graph_session_state::running ||
            state_ == cascade_graph_session_state::idle ||
            state_ == cascade_graph_session_state::configuring ||
            state_ == cascade_graph_session_state::loading ||
            state_ == cascade_graph_session_state::binding ||
            state_ == cascade_graph_session_state::starting) {
            return state_ == cascade_graph_session_state::running
                ? status{} : status{status_code::pending,
                    "cascade graph activation is pending"};
        }
        if (config_.graph_ == nullptr) {
            return {status_code::invalid_argument,
                "cascade graph session has no graph"};
        }
        const auto graph_state = config_.graph_->vqec_vision_ai_ports_infgr_get_state();
        if (graph_state != inference_graph_state::empty &&
            graph_state != inference_graph_state::configured) {
            return {status_code::invalid_state,
                "stopped cascade graph is not restartable"};
        }
        last_error_ = {};
        is_recovery_required_ = false;
        start_ns_ = _steady_now_ns;
        state_ = graph_state == inference_graph_state::empty
            ? cascade_graph_session_state::configuring
            : cascade_graph_session_state::loading;
        return {status_code::pending, "cascade graph activation accepted"};
    }
    desired_active_ = false;
    if (state_ == cascade_graph_session_state::stopped ||
        state_ == cascade_graph_session_state::draining ||
        state_ == cascade_graph_session_state::unloading) {
        return {};
    }
    if (state_ == cascade_graph_session_state::idle) {
        state_ = cascade_graph_session_state::stopped;
        return {};
    }
    stop_ns_ = _steady_now_ns;
    state_ = cascade_graph_session_state::draining;
    return {};
}

cascade_graph_session_state cascade_graph_session::
vqec_vision_ai_appl_cgses_get_state() const noexcept {
    return state_;
}

const status& cascade_graph_session::vqec_vision_ai_appl_cgses_get_last_error()
    const noexcept {
    return last_error_;
}

bool cascade_graph_session::vqec_vision_ai_appl_cgses_is_recovery_required()
    const noexcept {
    return is_recovery_required_;
}

}  // namespace vqec::vision::ai
