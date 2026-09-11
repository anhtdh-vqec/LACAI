#include "vqec_vision_runtime_executor.hpp"

#include <limits>
#include <utility>

namespace vqec::vision::ai {

runtime_executor::runtime_executor(application_composition& _composition,
    const std::array<multi_model_feature_pipeline*, deployment_limits::g_max_sources>&
        _pipelines,
    std::uint16_t _source_count) noexcept
    : composition_(_composition), pipelines_(_pipelines), source_count_(_source_count) {}

status runtime_executor::vqec_vision_ai_appl_rtexe_step(
    std::uint64_t _steady_now_ns, runtime_executor_report& _report) {
    _report = {};
    if (_steady_now_ns == std::numeric_limits<std::uint64_t>::max() ||
        _steady_now_ns < last_now_ns_) {
        return {status_code::invalid_argument, "executor requires monotonic steady time"};
    }
    last_now_ns_ = _steady_now_ns;
    if (has_pending_) {
        return {status_code::pending, "consume the routed result before progress"};
    }
    const auto stepped = composition_.vqec_vision_ai_cntr_acomp_step(_steady_now_ns);

    tensor_result result;
    multi_source_progress_report progress;
    const auto taken = composition_.vqec_vision_ai_appl_acomp_take_result(result, progress);
    if (taken.code_ != status_code::ok) {
        return stepped.code_ == status_code::ok ?
            status{status_code::pending, "composition produced no routeable result"} : stepped;
    }
    const auto source_index = progress.source_index_;
    if (source_index >= source_count_ || pipelines_[source_index] == nullptr) {
        return {status_code::invalid_state, "executor has no pipeline for the result source"};
    }

    // The session progress already carries the routed model slot and correlated ticket;
    // reconstruct the pump-shaped report the neutral router expects without re-deriving
    // source identity from the tensor.
    multi_model_pump_report pump_report;
    pump_report.has_result_ = progress.source_progress_.has_result_;
    pump_report.result_model_slot_ = progress.source_progress_.model_slot_;
    pump_report.result_ticket_ = progress.source_progress_.ticket_;
    pump_report.error_model_slot_ = progress.source_progress_.error_model_slot_;

    multi_model_feature_pipeline_report pipeline_report;
    const auto processed = pipelines_[source_index]->
        vqec_vision_ai_appl_mmfpl_process_result(
            result, pump_report, _steady_now_ns, pending_tracked_, pending_events_,
            pipeline_report);
    if (processed.code_ != status_code::ok) {
        return processed;
    }
    pending_report_.source_index_ = source_index;
    pending_report_.model_slot_ = pipeline_report.result_.model_slot_;
    pending_report_.features_ = pipeline_report.features_;
    pending_report_.has_tracked_ = pipeline_report.result_.has_tracked_;
    pending_report_.has_feature_fanout_ = pipeline_report.has_feature_fanout_;
    has_pending_ = true;
    _report = pending_report_;
    return {};
}

status runtime_executor::vqec_vision_ai_appl_rtexe_take_result(
    std::array<observation_batch, deployment_limits::g_max_models_per_source>& _tracked,
    std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages>& _events,
    runtime_executor_report& _report) {
    if (!has_pending_) {
        return {status_code::pending, "executor has no routed result"};
    }
    _tracked = std::move(pending_tracked_);
    _events = std::move(pending_events_);
    _report = pending_report_;
    has_pending_ = false;
    return {};
}

status runtime_executor::vqec_vision_ai_appl_rtexe_request_stop(
    std::uint64_t _steady_now_ns) {
    if (_steady_now_ns == std::numeric_limits<std::uint64_t>::max() ||
        _steady_now_ns < last_now_ns_) {
        return {status_code::invalid_argument, "executor requires monotonic steady time"};
    }
    last_now_ns_ = _steady_now_ns;
    return composition_.vqec_vision_ai_cntr_acomp_request_stop(_steady_now_ns);
}

application_composition_snapshot
runtime_executor::vqec_vision_ai_appl_rtexe_get_snapshot() const noexcept {
    return composition_.vqec_vision_ai_cntr_acomp_get_snapshot();
}

bool runtime_executor::vqec_vision_ai_appl_rtexe_has_pending() const noexcept {
    return has_pending_;
}

}  // namespace vqec::vision::ai
