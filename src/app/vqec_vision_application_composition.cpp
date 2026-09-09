#include "vqec_vision_application_composition.hpp"

#include <limits>
#include <utility>

namespace vqec::vision::ai {

application_composition::application_composition(
    std::uint64_t _deployment_revision, std::uint64_t _catalog_revision,
    std::uint16_t _declared_sources) noexcept
    : supervisor_({_deployment_revision, _catalog_revision, _declared_sources}) {
    snapshot_.deployment_revision_ = _deployment_revision;
    snapshot_.catalog_revision_ = _catalog_revision;
    snapshot_.declared_sources_ = _declared_sources;
}

status application_composition::vqec_vision_ai_appl_acomp_bind_session(
    std::uint16_t _source_index, source_session_port& _session) {
    if (snapshot_.state_ != application_composition_state::idle) {
        return {status_code::invalid_state,
            "sessions can only bind before composition validation"};
    }
    const auto bound = supervisor_.vqec_vision_ai_appl_mssup_bind_session(
        _source_index, _session);
    return bound;
}

status application_composition::vqec_vision_ai_cntr_acomp_validate() {
    if (snapshot_.state_ != application_composition_state::idle) {
        return {status_code::invalid_state, "composition validation is activation-only"};
    }
    if (snapshot_.deployment_revision_ == 0 || snapshot_.catalog_revision_ == 0 ||
        snapshot_.declared_sources_ == 0 ||
        snapshot_.declared_sources_ > deployment_limits::g_max_sources ||
        snapshot_.deployment_revision_ == UINT64_MAX ||
        snapshot_.catalog_revision_ == UINT64_MAX) {
        snapshot_.first_error_code_ = status_code::invalid_argument;
        snapshot_.state_ = application_composition_state::faulted;
        return {status_code::invalid_argument, "composition requires validated revisions and sources"};
    }
    const auto supervisor_snapshot = supervisor_.
        vqec_vision_ai_appl_mssup_get_snapshot();
    if (supervisor_snapshot.bound_sources_ != snapshot_.declared_sources_) {
        snapshot_.first_error_code_ = status_code::invalid_argument;
        snapshot_.state_ = application_composition_state::faulted;
        return {status_code::invalid_argument,
            "composition requires every source session to be bound"};
    }
    snapshot_.state_ = application_composition_state::validating;
    return {};
}

status application_composition::vqec_vision_ai_cntr_acomp_activate() {
    if (snapshot_.state_ != application_composition_state::validating) {
        return {status_code::invalid_state, "composition must be validated before activation"};
    }
    const auto activated = supervisor_.vqec_vision_ai_appl_mssup_activate();
    if (activated.code_ != status_code::ok) {
        snapshot_.first_error_code_ = activated.code_;
        snapshot_.state_ = application_composition_state::faulted;
        return activated;
    }
    snapshot_.admitted_sources_ = snapshot_.declared_sources_;
    snapshot_.state_ = application_composition_state::activated;
    return {};
}

status application_composition::vqec_vision_ai_cntr_acomp_step(
    std::uint64_t _steady_now_ns) {
    if (_steady_now_ns == std::numeric_limits<std::uint64_t>::max() ||
        _steady_now_ns < last_now_ns_) {
        return {status_code::invalid_argument, "composition requires monotonic time"};
    }
    last_now_ns_ = _steady_now_ns;
    if (snapshot_.state_ == application_composition_state::activated) {
        snapshot_.state_ = application_composition_state::running;
    }
    if (snapshot_.state_ != application_composition_state::running &&
        snapshot_.state_ != application_composition_state::stopping) {
        return {status_code::invalid_state, "composition has no running sessions"};
    }
    if (has_pending_result_) {
        return {status_code::pending, "consume the pending composition result before progress"};
    }
    const auto progressed = supervisor_.vqec_vision_ai_appl_mssup_step(
        _steady_now_ns, pending_result_, pending_report_);
    has_pending_result_ = pending_report_.has_result_;
    vqec_vision_ai_appl_acomp_refresh_snapshot();
    return progressed;
}

status application_composition::vqec_vision_ai_cntr_acomp_request_stop(
    std::uint64_t _steady_now_ns) {
    if (_steady_now_ns == UINT64_MAX || _steady_now_ns < last_now_ns_) {
        return {status_code::invalid_argument, "composition requires monotonic time"};
    }
    last_now_ns_ = _steady_now_ns;
    if (snapshot_.state_ == application_composition_state::stopped) {
        return {};
    }
    if (snapshot_.state_ != application_composition_state::running &&
        snapshot_.state_ != application_composition_state::activated &&
        snapshot_.state_ != application_composition_state::stopping) {
        return {status_code::invalid_state,
            "composition can only stop after activation"};
    }
    snapshot_.state_ = application_composition_state::stopping;
    const auto stopped = supervisor_.vqec_vision_ai_appl_mssup_request_stop(
        _steady_now_ns);
    vqec_vision_ai_appl_acomp_refresh_snapshot();
    if (stopped.code_ != status_code::ok && stopped.code_ != status_code::pending &&
        snapshot_.first_error_code_ == status_code::ok) {
        snapshot_.first_error_code_ = stopped.code_;
    }
    return stopped.code_ == status_code::ok &&
        snapshot_.state_ != application_composition_state::stopped ?
        status{status_code::pending, "source sessions are draining"} : stopped;
}

void application_composition::vqec_vision_ai_appl_acomp_refresh_snapshot() noexcept {
    const auto current = supervisor_.vqec_vision_ai_appl_mssup_get_snapshot();
    snapshot_.is_recovery_required_ = current.recovery_sources_ != 0;
    if (snapshot_.first_error_code_ == status_code::ok) {
        snapshot_.first_error_code_ = current.first_error_code_;
    }
    if (current.supervisor_state_ == multi_source_supervisor_state::stopped) {
        snapshot_.state_ = application_composition_state::stopped;
    }
}

status application_composition::vqec_vision_ai_appl_acomp_take_result(
    tensor_result& _result, multi_source_progress_report& _report) {
    if (!has_pending_result_) {
        return {status_code::pending, "composition has no result"};
    }
    _result = std::move(pending_result_);
    _report = std::move(pending_report_);
    has_pending_result_ = false;
    return {};
}

application_composition_snapshot
application_composition::vqec_vision_ai_cntr_acomp_get_snapshot() const noexcept {
    return snapshot_;
}

}  // namespace vqec::vision::ai
