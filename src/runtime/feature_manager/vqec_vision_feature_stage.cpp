#include "vqec_vision_feature_stage.hpp"

#include <utility>

namespace vqec::vision::ai {

feature_stage::feature_stage(
    feature_processor_port& _processor, feature_processor_config _config)
    : processor_(_processor), config_(std::move(_config)) {}

status feature_stage::vqec_vision_ai_ftmgr_ftstg_activate() {
    if (is_active_) {
        return {status_code::invalid_state, "feature stage is already active"};
    }
    const auto valid = vqec_vision_ai_core_ftevt_validate_processor_config(config_);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    status accepted;
    try {
        accepted = processor_.vqec_vision_ai_ports_ftpro_validate_activation(config_);
    } catch (...) {
        return {status_code::io_error, "feature activation validation raised an exception"};
    }
    if (accepted.code_ != status_code::ok) {
        return accepted;
    }
    is_active_ = true;
    return {};
}

status feature_stage::vqec_vision_ai_ftmgr_ftstg_process(
    const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
    bool _is_source_gap, feature_event_batch& _events) {
    if (!is_active_) {
        return {status_code::invalid_state, "feature stage is not active"};
    }
    const auto valid_input = vqec_vision_ai_core_obval_validate_batch(
        _tracked, _tracked.frame_, _tracked.geometry_);
    if (valid_input.code_ != status_code::ok) {
        return valid_input;
    }
    if (_now_monotonic_ns == UINT64_MAX ||
        _now_monotonic_ns < last_now_monotonic_ns_) {
        return {status_code::invalid_argument, "feature stage requires monotonic time"};
    }
    const auto incoming_epoch = _tracked.frame_.source_epoch_;
    if (incoming_epoch != source_epoch_) {
        if (source_epoch_ != 0 && incoming_epoch < source_epoch_) {
            return {status_code::invalid_state, "feature stage rejected a stale source epoch"};
        }
        source_epoch_ = incoming_epoch;
        last_now_monotonic_ns_ = _now_monotonic_ns;
        is_faulted_ = true;
        status reset;
        try {
            reset = processor_.vqec_vision_ai_ports_ftpro_reset_epoch(incoming_epoch);
        } catch (...) {
            is_faulted_ = true;
            return {status_code::io_error, "feature epoch reset raised an exception"};
        }
        if (reset.code_ != status_code::ok) {
            is_faulted_ = true;
            return reset;
        }
        source_epoch_ = incoming_epoch;
        is_faulted_ = false;
    } else if (is_faulted_) {
        return {status_code::invalid_state, "feature processor is faulted for this epoch"};
    }
    last_now_monotonic_ns_ = _now_monotonic_ns;

    feature_event_batch candidate;
    status processed;
    try {
        processed = processor_.vqec_vision_ai_ports_ftpro_process_observations(
            _tracked, _now_monotonic_ns, _is_source_gap, candidate);
    } catch (...) {
        is_faulted_ = true;
        return {status_code::io_error, "feature processing raised an exception"};
    }
    if (processed.code_ != status_code::ok) {
        is_faulted_ = true;
        return processed;
    }
    const auto valid_output = vqec_vision_ai_core_ftevt_validate_batch(
        candidate, _tracked.frame_, _tracked.geometry_, config_);
    if (valid_output.code_ != status_code::ok) {
        is_faulted_ = true;
        return valid_output;
    }
    _events = std::move(candidate);
    return {};
}

bool feature_stage::vqec_vision_ai_ftmgr_ftstg_is_active() const noexcept {
    return is_active_;
}

bool feature_stage::vqec_vision_ai_ftmgr_ftstg_is_faulted() const noexcept {
    return is_faulted_;
}

}  // namespace vqec::vision::ai
