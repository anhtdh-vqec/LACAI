#include "vqec_vision_multi_model_result_router.hpp"

#include <utility>

namespace vqec::vision::ai {

status multi_model_result_router::vqec_vision_ai_appl_mmrrt_configure(
    const perception_result_config& _source,
    const std::array<perception_result_stage*,
        deployment_limits::g_max_models_per_source>& _stages,
    std::uint16_t _stage_count) {
    if (is_configured_) {
        return {status_code::invalid_state, "multi-model result router is already configured"};
    }
    if (_stage_count == 0 || _stage_count > _stages.size()) {
        return {status_code::invalid_argument, "multi-model result stage count is invalid"};
    }
    for (std::uint16_t slot = 0; slot < _stage_count; ++slot) {
        if (_stages[slot] == nullptr) {
            return {status_code::invalid_argument, "multi-model result stage is null"};
        }
        const auto valid_config =
            _stages[slot]->vqec_vision_ai_appl_prstg_validate_config(_source);
        if (valid_config.code_ != status_code::ok) {
            return valid_config;
        }
        for (std::uint16_t prior = 0; prior < slot; ++prior) {
            if (_stages[prior] == _stages[slot]) {
                return {status_code::invalid_argument,
                    "multi-model result stages must have unique owners"};
            }
        }
    }
    stages_ = _stages;
    stage_count_ = _stage_count;
    is_configured_ = true;
    return {};
}

status multi_model_result_router::vqec_vision_ai_appl_mmrrt_route_result(
    const tensor_result& _result, const multi_model_pump_report& _pump_report,
    std::uint64_t _now_monotonic_ns,
    std::array<observation_batch, deployment_limits::g_max_models_per_source>&
        _tracked_by_model,
    multi_model_result_report& _report) {
    _report = {};
    if (!is_configured_) {
        return {status_code::invalid_state, "multi-model result router is not configured"};
    }
    if (!_pump_report.has_result_) {
        return {status_code::pending, "multi-model pump has no result"};
    }
    const auto slot = _pump_report.result_model_slot_;
    _report.model_slot_ = slot;
    if (slot >= stage_count_ || _pump_report.error_model_slot_ != UINT16_MAX) {
        return {status_code::invalid_argument, "multi-model pump result report is inconsistent"};
    }

    const auto& ticket = _pump_report.result_ticket_;
    bool is_source_gap = false;
    if (has_progress_[slot]) {
        if (ticket.source_epoch_ < source_epochs_[slot]) {
            return {status_code::invalid_state, "multi-model result source epoch moved backward"};
        }
        if (ticket.source_epoch_ == source_epochs_[slot]) {
            if (ticket.source_frame_id_ <= frame_ids_[slot] ||
                ticket.source_pts_ns_ <= source_pts_ns_[slot]) {
                return {status_code::invalid_state,
                    "multi-model result source progress is not monotonic"};
            }
            is_source_gap = frame_ids_[slot] != UINT64_MAX &&
                ticket.source_frame_id_ != frame_ids_[slot] + 1;
        }
    }

    observation_batch candidate;
    const auto processed = stages_[slot]->vqec_vision_ai_appl_prstg_process(
        _result, ticket, _now_monotonic_ns, is_source_gap, candidate);
    if (processed.code_ != status_code::ok) {
        return processed;
    }
    _tracked_by_model[slot] = std::move(candidate);
    source_epochs_[slot] = ticket.source_epoch_;
    frame_ids_[slot] = ticket.source_frame_id_;
    source_pts_ns_[slot] = ticket.source_pts_ns_;
    has_progress_[slot] = true;
    _report.has_tracked_ = true;
    _report.is_source_gap_ = is_source_gap;
    return {};
}

std::uint16_t multi_model_result_router::
vqec_vision_ai_appl_mmrrt_get_stage_count() const noexcept {
    return stage_count_;
}

}  // namespace vqec::vision::ai
