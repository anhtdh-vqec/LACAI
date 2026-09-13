#include "vqec_vision_multi_source_supervisor.hpp"

#include <limits>
#include <utility>

namespace vqec::vision::ai {

multi_source_supervisor::multi_source_supervisor(
    multi_source_supervisor_config _config) noexcept
    : config_(_config) {}

status multi_source_supervisor::vqec_vision_ai_appl_mssup_check_time(
    std::uint64_t _steady_now_ns) {
    if (_steady_now_ns == std::numeric_limits<std::uint64_t>::max() ||
        _steady_now_ns < last_now_ns_) {
        return {status_code::invalid_argument,
                "supervisor requires monotonic steady time"};
    }
    last_now_ns_ = _steady_now_ns;
    return {};
}

status multi_source_supervisor::vqec_vision_ai_appl_mssup_bind_session(
    std::uint16_t _source_index, source_session_port& _session) {
    if (state_ != multi_source_supervisor_state::binding) {
        return {status_code::invalid_state, "sessions can only bind before activation"};
    }
    if (config_.source_count_ == 0 ||
        config_.source_count_ > deployment_limits::g_max_sources ||
        _source_index >= config_.source_count_) {
        return {status_code::invalid_argument, "source index is outside declared range"};
    }
    if (sessions_[_source_index] != nullptr) {
        return {status_code::invalid_state, "source index is already bound"};
    }
    if (_session.vqec_vision_ai_appl_srcsn_get_health().phase_ !=
        source_session_phase::idle) {
        return {status_code::invalid_state, "bound session must be idle"};
    }
    for (std::uint16_t index = 0; index < config_.source_count_; ++index) {
        if (sessions_[index] == &_session) {
            return {status_code::invalid_argument,
                    "session is already bound to another source"};
        }
    }
    sessions_[_source_index] = &_session;
    ++bound_count_;
    return {};
}

status multi_source_supervisor::vqec_vision_ai_appl_mssup_activate() {
    if (state_ != multi_source_supervisor_state::binding) {
        return {status_code::invalid_state, "supervisor is already activated"};
    }
    if (config_.deployment_revision_ == 0 || config_.catalog_revision_ == 0 ||
        config_.source_count_ == 0 ||
        config_.source_count_ > deployment_limits::g_max_sources ||
        bound_count_ != config_.source_count_) {
        return {status_code::invalid_argument,
                "incomplete source binding or invalid activation revision"};
    }
    for (std::uint16_t index = 0; index < config_.source_count_; ++index) {
        if (sessions_[index] == nullptr ||
            sessions_[index]->vqec_vision_ai_appl_srcsn_get_health().phase_ !=
                source_session_phase::idle) {
            return {status_code::invalid_state, "activation requires every session idle"};
        }
    }
    next_source_index_ = 0;
    state_ = multi_source_supervisor_state::running;
    return {};
}

void multi_source_supervisor::vqec_vision_ai_appl_mssup_refresh_state() noexcept {
    if (state_ == multi_source_supervisor_state::binding ||
        state_ == multi_source_supervisor_state::stopped) {
        return;
    }
    for (std::uint16_t index = 0; index < config_.source_count_; ++index) {
        if (sessions_[index] == nullptr ||
            sessions_[index]->vqec_vision_ai_appl_srcsn_get_health().phase_ !=
                source_session_phase::stopped) {
            return;
        }
    }
    state_ = multi_source_supervisor_state::stopped;
}

status multi_source_supervisor::vqec_vision_ai_appl_mssup_step(
    std::uint64_t _steady_now_ns, tensor_result& _result,
    multi_source_progress_report& _report) {
    _report = {};
    const auto time = vqec_vision_ai_appl_mssup_check_time(_steady_now_ns);
    if (time.code_ != status_code::ok) {
        return time;
    }
    if (state_ == multi_source_supervisor_state::binding) {
        return {status_code::invalid_state, "supervisor must be activated before progress"};
    }
    if (state_ == multi_source_supervisor_state::stopped) {
        return {};
    }

    source_session_port* selected = nullptr;
    std::uint16_t selected_index = g_invalid_source_index;
    for (std::uint16_t offset = 0; offset < config_.source_count_; ++offset) {
        const auto index = static_cast<std::uint16_t>(
            (static_cast<unsigned>(next_source_index_) + offset) % config_.source_count_);
        if (sessions_[index]->vqec_vision_ai_appl_srcsn_get_health().phase_ !=
            source_session_phase::stopped) {
            selected = sessions_[index];
            selected_index = index;
            break;
        }
    }
    if (selected == nullptr) {
        state_ = multi_source_supervisor_state::stopped;
        return {};
    }

    next_source_index_ = static_cast<std::uint16_t>(
        (static_cast<unsigned>(selected_index) + 1U) % config_.source_count_);
    source_session_progress progress;
    auto source_status = selected->vqec_vision_ai_appl_srcsn_step(
        _steady_now_ns, _result, progress);
    const auto source_code = source_status.code_;
    _report.source_index_ = selected_index;
    _report.source_status_ = std::move(source_status);
    _report.source_health_ = selected->vqec_vision_ai_appl_srcsn_get_health();
    _report.source_progress_ = progress;
    _report.has_source_ = true;
    _report.has_result_ = progress.has_result_;
    if (source_code != status_code::ok && source_code != status_code::pending) {
        // Keep fault isolation (one source does not stop the rest) but publish the error on
        // the independent fault channel so it is never an invisible pending.
        vqec_vision_ai_appl_mssup_record_fault(
            selected_index, source_code, _steady_now_ns);
    }
    vqec_vision_ai_appl_mssup_refresh_state();

    if (state_ == multi_source_supervisor_state::stopped) {
        return {};
    }
    if (source_code != status_code::ok && source_code != status_code::pending) {
        return {status_code::pending, {}};
    }
    return source_code == status_code::ok && progress.has_result_ ?
        status{} : status{status_code::pending, {}};
}

status multi_source_supervisor::vqec_vision_ai_appl_mssup_request_stop(
    std::uint64_t _steady_now_ns) {
    const auto time = vqec_vision_ai_appl_mssup_check_time(_steady_now_ns);
    if (time.code_ != status_code::ok) {
        return time;
    }
    if (state_ == multi_source_supervisor_state::binding) {
        return {status_code::invalid_state,
                "cannot stop a supervisor before activation"};
    }
    if (state_ == multi_source_supervisor_state::stopped) {
        return {};
    }

    status first_error;
    state_ = multi_source_supervisor_state::stopping;
    for (std::uint16_t index = 0;
         index < config_.source_count_ && index < sessions_.size(); ++index) {
        const auto stopped = sessions_[index]->vqec_vision_ai_appl_srcsn_request_stop(
            _steady_now_ns);
        if (first_error.code_ == status_code::ok && stopped.code_ != status_code::ok) {
            first_error = stopped;
        }
    }
    vqec_vision_ai_appl_mssup_refresh_state();
    return first_error;
}

multi_source_supervisor_state
multi_source_supervisor::vqec_vision_ai_appl_mssup_get_state() const noexcept {
    return state_;
}

void multi_source_supervisor::vqec_vision_ai_appl_mssup_record_fault(
    std::uint16_t _source_index, status_code _code, std::uint64_t _at_ns) noexcept {
    if (_source_index < deployment_limits::g_max_sources) {
        source_fault_codes_[_source_index] = _code;
    }
    multi_source_fault_event event;
    event.source_index_ = _source_index;
    event.code_ = _code;
    event.at_ns_ = _at_ns;
    if (fault_event_count_ < g_max_supervisor_fault_events) {
        fault_events_[(static_cast<std::size_t>(fault_event_head_) + fault_event_count_) %
            g_max_supervisor_fault_events] = event;
        ++fault_event_count_;
    } else {
        // Bounded ring: overwrite the oldest retained event.
        fault_events_[fault_event_head_] = event;
        fault_event_head_ = static_cast<std::uint16_t>(
            (fault_event_head_ + 1U) % g_max_supervisor_fault_events);
    }
    ++fault_event_total_;
}

status multi_source_supervisor::vqec_vision_ai_appl_mssup_take_fault(
    multi_source_fault_event& _fault) {
    if (fault_event_count_ == 0) {
        return {status_code::pending, "no source fault event is queued"};
    }
    _fault = fault_events_[fault_event_head_];
    fault_events_[fault_event_head_] = {};
    fault_event_head_ = static_cast<std::uint16_t>(
        (fault_event_head_ + 1U) % g_max_supervisor_fault_events);
    --fault_event_count_;
    return {};
}

multi_source_supervisor_snapshot
multi_source_supervisor::vqec_vision_ai_appl_mssup_get_snapshot() const noexcept {
    multi_source_supervisor_snapshot snapshot;
    snapshot.supervisor_state_ = state_;
    snapshot.deployment_revision_ = config_.deployment_revision_;
    snapshot.catalog_revision_ = config_.catalog_revision_;
    snapshot.declared_sources_ = config_.source_count_;
    snapshot.bound_sources_ = bound_count_;
    snapshot.fault_event_total_ = fault_event_total_;
    for (std::uint16_t index = 0;
         index < config_.source_count_ && index < sessions_.size(); ++index) {
        if (sessions_[index] == nullptr) {
            continue;
        }
        const auto source = sessions_[index]->vqec_vision_ai_appl_srcsn_get_health();
        switch (source.phase_) {
            case source_session_phase::running:
                ++snapshot.running_sources_;
                break;
            case source_session_phase::draining:
                ++snapshot.stopping_sources_;
                break;
            case source_session_phase::stopped:
                ++snapshot.stopped_sources_;
                break;
            default:
                ++snapshot.starting_sources_;
                break;
        }
        if (source.is_recovery_required_) {
            ++snapshot.recovery_sources_;
        }
        if (source_fault_codes_[index] != status_code::ok) {
            snapshot.source_fault_codes_[index] = source_fault_codes_[index];
            ++snapshot.faulted_sources_;
        }
        if (snapshot.first_error_code_ == status_code::ok &&
            source.first_error_code_ != status_code::ok) {
            snapshot.first_error_code_ = source.first_error_code_;
        }
    }
    return snapshot;
}

}  // namespace vqec::vision::ai
