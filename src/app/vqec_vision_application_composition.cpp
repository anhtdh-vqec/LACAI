#include "vqec_vision_application_composition.hpp"

#include <limits>

namespace vqec::vision::ai {

application_composition::application_composition(
    std::uint64_t _deployment_revision, std::uint64_t _catalog_revision,
    std::uint16_t _declared_sources) noexcept {
    snapshot_.deployment_revision_ = _deployment_revision;
    snapshot_.catalog_revision_ = _catalog_revision;
    snapshot_.declared_sources_ = _declared_sources;
}

status application_composition::vqec_vision_ai_cntr_acomp_validate() {
    if (snapshot_.state_ != application_composition_state::idle ||
        snapshot_.deployment_revision_ == 0 || snapshot_.catalog_revision_ == 0 ||
        snapshot_.declared_sources_ == 0) {
        snapshot_.first_error_code_ = status_code::invalid_argument;
        snapshot_.state_ = application_composition_state::faulted;
        return {status_code::invalid_argument, "composition requires validated revisions and sources"};
    }
    snapshot_.state_ = application_composition_state::validating;
    return {};
}

status application_composition::vqec_vision_ai_cntr_acomp_activate() {
    if (snapshot_.state_ != application_composition_state::validating) {
        return {status_code::invalid_state, "composition must be validated before activation"};
    }
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
    if (snapshot_.state_ != application_composition_state::running) {
        return {status_code::invalid_state, "composition has no running sessions"};
    }
    return {status_code::unsupported, "session factories are not installed"};
}

status application_composition::vqec_vision_ai_cntr_acomp_request_stop(
    std::uint64_t _steady_now_ns) {
    if (_steady_now_ns < last_now_ns_) {
        return {status_code::invalid_argument, "composition requires monotonic time"};
    }
    last_now_ns_ = _steady_now_ns;
    if (snapshot_.state_ == application_composition_state::stopped) {
        return {};
    }
    snapshot_.state_ = application_composition_state::stopping;
    snapshot_.is_recovery_required_ = true;
    return {status_code::pending, "session owners must drain before composition stops"};
}

application_composition_snapshot
application_composition::vqec_vision_ai_cntr_acomp_get_snapshot() const noexcept {
    return snapshot_;
}

}  // namespace vqec::vision::ai
