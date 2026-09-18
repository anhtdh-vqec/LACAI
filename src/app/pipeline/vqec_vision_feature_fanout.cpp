#include "vqec_vision_feature_fanout.hpp"

namespace vqec::vision::ai {

status feature_fanout::vqec_vision_ai_appl_ftfan_configure(
    const std::array<feature_stage*, feature_fanout_limits::g_max_feature_stages>& _stages,
    std::uint16_t _stage_count) {
    if (is_configured_) {
        return {status_code::invalid_state, "feature fan-out is already configured"};
    }
    if (_stage_count == 0 || _stage_count > feature_fanout_limits::g_max_feature_stages) {
        return {status_code::invalid_argument, "invalid feature stage count"};
    }
    for (std::uint16_t slot = 0; slot < _stage_count; ++slot) {
        if (_stages[slot] == nullptr ||
            !_stages[slot]->vqec_vision_ai_ftmgr_ftstg_is_active()) {
            return {status_code::invalid_state, "feature stage is absent or inactive"};
        }
        for (std::uint16_t previous = 0; previous < slot; ++previous) {
            if (_stages[previous] == _stages[slot]) {
                return {status_code::invalid_argument, "feature stage is bound more than once"};
            }
        }
    }
    stages_ = _stages;
    stage_count_ = _stage_count;
    is_configured_ = true;
    return {};
}

status feature_fanout::vqec_vision_ai_appl_ftfan_process(
    const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
    bool _is_source_gap,
    std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages>& _events,
    feature_fanout_report& _report) {
    _report = {};
    _report.first_error_slot_ = UINT16_MAX;
    if (!is_configured_) {
        return {status_code::invalid_state, "feature fan-out is not configured"};
    }
    const auto valid = vqec_vision_ai_core_obval_validate_batch(
        _tracked, _tracked.frame_, _tracked.geometry_);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (_now_monotonic_ns == UINT64_MAX ||
        _now_monotonic_ns < last_now_monotonic_ns_) {
        return {status_code::invalid_argument, "feature fan-out requires monotonic time"};
    }
    last_now_monotonic_ns_ = _now_monotonic_ns;

    status first_error;
    for (std::uint16_t slot = 0; slot < stage_count_; ++slot) {
        const auto bit = static_cast<std::uint32_t>(1U) << slot;
        const auto result = stages_[slot]->vqec_vision_ai_ftmgr_ftstg_process(
            _tracked, _now_monotonic_ns, _is_source_gap, _events[slot]);
        _report.status_codes_[slot] = result.code_;
        if (result.code_ == status_code::ok) {
            _report.processed_mask_ |= bit;
            continue;
        }
        _report.failed_mask_ |= bit;
        if (_report.first_error_slot_ == UINT16_MAX) {
            _report.first_error_slot_ = slot;
            first_error = result;
        }
    }
    return _report.failed_mask_ == 0 ? status{} : first_error;
}

std::uint16_t feature_fanout::vqec_vision_ai_appl_ftfan_get_stage_count() const noexcept {
    return stage_count_;
}

feature_stage* feature_fanout::vqec_vision_ai_appl_ftfan_get_stage(
    std::uint16_t _slot) noexcept {
    return _slot < stage_count_ ? stages_[_slot] : nullptr;
}

}  // namespace vqec::vision::ai
