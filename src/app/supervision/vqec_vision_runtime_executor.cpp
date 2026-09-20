#include "vqec_vision_runtime_executor.hpp"
#include "vqec_vision_feature_event_dispatch.hpp"

#include <limits>
#include <new>
#include <utility>

namespace vqec::vision::ai {

runtime_executor::runtime_executor(application_composition& _composition,
    const std::array<multi_model_feature_pipeline*, deployment_limits::g_max_sources>&
        _pipelines,
    const std::array<std::uint16_t, deployment_limits::g_max_sources>&
        _cascade_root_slots,
    const std::array<std::uint32_t, deployment_limits::g_max_sources>& _camera_ids,
    const std::array<std::uint32_t, deployment_limits::g_max_sources>& _channel_ids,
    std::uint16_t _source_count) noexcept
    : composition_(_composition), pipelines_(_pipelines),
      cascade_root_slots_(_cascade_root_slots), camera_ids_(_camera_ids),
      channel_ids_(_channel_ids), source_count_(_source_count) {}

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
    for (std::uint16_t source = 0; source < source_count_; ++source) {
        auto* worker = cascade_workers_[source];
        if (worker == nullptr) {
            continue;
        }
        cascade_worker_completion completion;
        const auto polled = worker->vqec_vision_ai_appl_cxwrk_poll_completion(completion);
        if (polled.code_ == status_code::pending) {
            continue;
        }
        if (polled.code_ != status_code::ok) {
            return polled;
        }
        if (!cascade_active_[source]) {
            metrics_.cascade_tasks_accepted_ += completion.report_.accepted_;
            metrics_.cascade_embeddings_ += completion.report_.embedded_;
            metrics_.cascade_tasks_failed_ += completion.report_.failed_;
            continue;
        }
        const auto composition_snapshot =
            composition_.vqec_vision_ai_cntr_acomp_get_snapshot();
        pending_tracked_ = {};
        pending_events_ = {};
        pending_tracked_[cascade_root_slots_[source]] = std::move(completion.tracked_);
        cascade_embeddings_ = std::move(completion.embeddings_);
        pending_report_ = {};
        pending_report_.source_index_ = source;
        pending_report_.model_slot_ = cascade_root_slots_[source];
        pending_report_.captured_policy_revision_ = completion.policy_revision_;
        pending_report_.captured_catalog_revision_ = composition_snapshot.catalog_revision_;
        pending_report_.captured_deployment_revision_ =
            composition_snapshot.deployment_revision_;
        pending_report_.cascade_ = completion.report_;
        pending_report_.has_tracked_ = true;
        pending_report_.has_cascade_ = true;
        pending_report_.first_error_code_ = completion.task_status_.code_;
        metrics_.cascade_tasks_accepted_ += completion.report_.accepted_;
        metrics_.cascade_embeddings_ += completion.report_.embedded_;
        metrics_.cascade_tasks_failed_ += completion.report_.failed_;
        has_pending_ = true;
        ++metrics_.results_routed_;
        _report = pending_report_;
        return {};
    }
    for (std::uint16_t source = 0; source < source_count_; ++source) {
        if (cascade_root_slots_[source] != g_invalid_model_slot &&
            cascade_coordinators_[source] == nullptr &&
            cascade_workers_[source] == nullptr) {
            return {status_code::invalid_state,
                "catalog cascade root has no bound coordinator"};
        }
    }
    ++metrics_.steps_;
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
    const auto cascade_root_slot = cascade_root_slots_[source_index];
    auto* cascade = cascade_coordinators_[source_index];
    auto* cascade_worker = cascade_workers_[source_index];
    if (!pipeline_report.result_.has_tracked_) {
        // A failed primary decode still closes admission for the exact retained frame.
        // Otherwise the source could never drain after one malformed model result.
        if (cascade != nullptr &&
            pump_report.result_model_slot_ == cascade_root_slot) {
            observation_batch empty;
            empty.frame_ = {camera_ids_[source_index], channel_ids_[source_index],
                pump_report.result_ticket_.source_epoch_,
                pump_report.result_ticket_.source_frame_id_,
                pump_report.result_ticket_.source_pts_ns_};
            cascade_coordinator_report discarded_report;
            const auto retired = cascade_active_[source_index]
                ? cascade->vqec_vision_ai_appl_cscrd_process(
                    _steady_now_ns, empty, cascade_aligned_, cascade_embeddings_,
                    discarded_report)
                : cascade->vqec_vision_ai_appl_cscrd_retire(empty);
            if (retired.code_ != status_code::ok) {
                return retired;
            }
        }
        if (cascade_worker != nullptr &&
            pump_report.result_model_slot_ == cascade_root_slot) {
            observation_batch empty;
            empty.frame_ = {camera_ids_[source_index], channel_ids_[source_index],
                pump_report.result_ticket_.source_epoch_,
                pump_report.result_ticket_.source_frame_id_,
                pump_report.result_ticket_.source_pts_ns_};
            const auto retired = cascade_active_[source_index]
                ? cascade_worker->vqec_vision_ai_appl_cxwrk_schedule(
                    _steady_now_ns, empty)
                : cascade_worker->vqec_vision_ai_appl_cxwrk_retire(empty);
            if (retired.code_ != status_code::ok) {
                return retired;
            }
        }
        // The decode/track layer failed, so there is no valid routed output and nothing
        // is retained. Surface the original routing error.
        return processed;
    }
    // The result was routed successfully even if one or more feature stages failed. Keep
    // the valid tracked observations and every successful feature batch exactly once and
    // record the first feature error instead of discarding healthy work.
    const auto composition_snapshot =
        composition_.vqec_vision_ai_cntr_acomp_get_snapshot();
    pending_report_ = {};
    pending_report_.source_index_ = source_index;
    pending_report_.model_slot_ = pipeline_report.result_.model_slot_;
    pending_report_.features_ = pipeline_report.features_;
    pending_report_.captured_catalog_revision_ = composition_snapshot.catalog_revision_;
    pending_report_.captured_deployment_revision_ = composition_snapshot.deployment_revision_;
    pending_report_.captured_policy_revision_ = delivery_gate_ != nullptr ?
        delivery_gate_->vqec_vision_ai_core_otgat_get_revision() : 0;
    pending_report_.has_tracked_ = true;
    pending_report_.has_feature_fanout_ = pipeline_report.has_feature_fanout_;
    if (cascade != nullptr && pipeline_report.result_.model_slot_ == cascade_root_slot) {
        const auto cascaded = cascade_active_[source_index]
            ? cascade->vqec_vision_ai_appl_cscrd_process(
                _steady_now_ns, pending_tracked_[cascade_root_slot], cascade_aligned_,
                cascade_embeddings_, pending_report_.cascade_)
            : cascade->vqec_vision_ai_appl_cscrd_retire(
                pending_tracked_[cascade_root_slot]);
        pending_report_.has_cascade_ = cascade_active_[source_index];
        if (cascade_active_[source_index]) {
            metrics_.cascade_tasks_accepted_ += pending_report_.cascade_.accepted_;
            metrics_.cascade_embeddings_ += pending_report_.cascade_.embedded_;
            metrics_.cascade_tasks_failed_ += pending_report_.cascade_.failed_;
        }
        if (cascaded.code_ != status_code::ok &&
            pending_report_.first_error_code_ == status_code::ok) {
            pending_report_.first_error_code_ = cascaded.code_;
        }
    }
    if (cascade_worker != nullptr &&
        pipeline_report.result_.model_slot_ == cascade_root_slot) {
        const auto scheduled = cascade_active_[source_index]
            ? cascade_worker->vqec_vision_ai_appl_cxwrk_schedule(
                _steady_now_ns, pending_tracked_[cascade_root_slot],
                pending_report_.captured_policy_revision_)
            : cascade_worker->vqec_vision_ai_appl_cxwrk_retire(
                pending_tracked_[cascade_root_slot]);
        if (scheduled.code_ != status_code::ok &&
            pending_report_.first_error_code_ == status_code::ok) {
            pending_report_.first_error_code_ = scheduled.code_;
        }
    }
    // Latency is measured entirely in the steady-clock domain: from the monotonic time the
    // job was reserved to the monotonic time its result is routed here. result.pipeline_pts_ns_
    // belongs to the vendor/pipeline clock and must not be mixed into this arithmetic.
    const auto submitted_steady_ns = pump_report.result_ticket_.submitted_steady_ns_;
    if (submitted_steady_ns != 0 && _steady_now_ns >= submitted_steady_ns) {
        const auto latency = _steady_now_ns - submitted_steady_ns;
        metrics_.end_to_end_ns_sum_ += latency;
        metrics_.end_to_end_ns_max_ =
            latency > metrics_.end_to_end_ns_max_ ? latency : metrics_.end_to_end_ns_max_;
        metrics_.end_to_end_ns_min_ =
            latency < metrics_.end_to_end_ns_min_ ? latency : metrics_.end_to_end_ns_min_;
        ++metrics_.end_to_end_samples_;
    }
    if (pending_report_.first_error_code_ == status_code::ok) {
        pending_report_.first_error_code_ = processed.code_;
    }
    has_pending_ = true;
    ++metrics_.results_routed_;
    _report = pending_report_;
    return {};
}

void runtime_executor::vqec_vision_ai_appl_rtexe_discard_pending() noexcept {
    has_pending_ = false;
    cascade_aligned_.clear();
    cascade_embeddings_.clear();
}

status runtime_executor::vqec_vision_ai_appl_rtexe_take_result(
    std::array<observation_batch, deployment_limits::g_max_models_per_source>& _tracked,
    std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages>& _events,
    runtime_executor_report& _report) {
    std::vector<embedding_result> discarded_embeddings;
    return vqec_vision_ai_appl_rtexe_take_result_with_embeddings(
        _tracked, _events, discarded_embeddings, _report);
}

status runtime_executor::vqec_vision_ai_appl_rtexe_take_result_with_embeddings(
    std::array<observation_batch, deployment_limits::g_max_models_per_source>& _tracked,
    std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages>& _events,
    std::vector<embedding_result>& _embeddings, runtime_executor_report& _report) {
    _embeddings.clear();
    if (!has_pending_) {
        return {status_code::pending, "executor has no routed result"};
    }
    _tracked = std::move(pending_tracked_);
    _events = std::move(pending_events_);
    _embeddings = std::move(cascade_embeddings_);
    _report = pending_report_;
    has_pending_ = false;
    cascade_aligned_.clear();
    cascade_embeddings_.clear();
    return {};
}

status runtime_executor::vqec_vision_ai_appl_rtexe_request_stop(
    std::uint64_t _steady_now_ns) {
    if (_steady_now_ns == std::numeric_limits<std::uint64_t>::max() ||
        _steady_now_ns < last_now_ns_) {
        return {status_code::invalid_argument, "executor requires monotonic steady time"};
    }
    last_now_ns_ = _steady_now_ns;
    for (auto* coordinator : cascade_coordinators_) {
        if (coordinator != nullptr) {
            (void)coordinator->vqec_vision_ai_appl_cscrd_request_stop(_steady_now_ns);
        }
    }
    for (auto* worker : cascade_workers_) {
        if (worker != nullptr) {
            (void)worker->vqec_vision_ai_appl_cxwrk_request_stop(_steady_now_ns);
        }
    }
    return composition_.vqec_vision_ai_cntr_acomp_request_stop(_steady_now_ns);
}

void runtime_executor::vqec_vision_ai_appl_rtexe_bind_event_delivery(
    output_gate& _gate, feature_event_sink_port& _sink) noexcept {
    delivery_gate_ = &_gate;
    delivery_sink_ = &_sink;
}

status runtime_executor::vqec_vision_ai_appl_rtexe_bind_cascade(
    std::uint16_t _source_index, std::uint16_t _model_slot,
    cascade_coordinator& _coordinator, std::size_t _max_results) {
    if (_source_index >= source_count_ ||
        cascade_root_slots_[_source_index] == g_invalid_model_slot ||
        cascade_root_slots_[_source_index] != _model_slot ||
        cascade_coordinators_[_source_index] != nullptr ||
        !_coordinator.vqec_vision_ai_appl_cscrd_is_configured() ||
        _max_results == 0 || _max_results > image_alignment_limits::g_max_points ||
        metrics_.steps_ != 0) {
        return {status_code::invalid_argument, "invalid runtime cascade binding"};
    }
    try {
        cascade_aligned_.reserve(_max_results);
        cascade_embeddings_.reserve(_max_results);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "runtime cascade result reservation failed"};
    }
    cascade_coordinators_[_source_index] = &_coordinator;
    cascade_active_[_source_index] = true;
    return {};
}

status runtime_executor::vqec_vision_ai_appl_rtexe_bind_cascade_worker(
    std::uint16_t _source_index, std::uint16_t _model_slot,
    cascade_execution_worker& _worker) {
    if (_source_index >= source_count_ ||
        cascade_root_slots_[_source_index] == g_invalid_model_slot ||
        cascade_root_slots_[_source_index] != _model_slot ||
        cascade_coordinators_[_source_index] != nullptr ||
        cascade_workers_[_source_index] != nullptr ||
        !_worker.vqec_vision_ai_appl_cxwrk_is_running() ||
        metrics_.steps_ != 0) {
        return {status_code::invalid_argument, "invalid runtime cascade worker binding"};
    }
    cascade_workers_[_source_index] = &_worker;
    cascade_active_[_source_index] = true;
    return {};
}

status runtime_executor::vqec_vision_ai_appl_rtexe_set_cascade_active(
    std::uint16_t _source_index, bool _active) noexcept {
    if (_source_index >= source_count_ ||
        (cascade_coordinators_[_source_index] == nullptr &&
         cascade_workers_[_source_index] == nullptr)) {
        return {status_code::invalid_argument,
            "cascade activation source is not bound"};
    }
    cascade_active_[_source_index] = _active;
    return {};
}

status runtime_executor::vqec_vision_ai_appl_rtexe_dispatch_events(
    const std::array<feature_event_batch,
        feature_fanout_limits::g_max_feature_stages>& _events,
    std::uint16_t _source_index, std::uint16_t _model_slot,
    std::uint64_t _policy_revision, std::uint32_t _success_mask,
    std::uint64_t _steady_now_ns, feature_dispatch_report& _report) {
    _report = {};
    if (delivery_gate_ == nullptr || delivery_sink_ == nullptr) {
        return {status_code::invalid_state, "executor has no bound event delivery"};
    }
    if (_source_index >= source_count_ || pipelines_[_source_index] == nullptr) {
        return {status_code::invalid_argument, "dispatch source index is invalid"};
    }
    if (_policy_revision == 0 || _policy_revision == UINT64_MAX) {
        return {status_code::invalid_argument, "dispatch policy revision is invalid"};
    }
    auto* fanout = pipelines_[_source_index]->
        vqec_vision_ai_appl_mmfpl_get_fanout(_model_slot);
    if (fanout == nullptr) {
        return {status_code::pending, "dispatch model slot has no feature fan-out"};
    }
    const auto stage_count = fanout->vqec_vision_ai_appl_ftfan_get_stage_count();
    for (std::uint16_t ordinal = 0; ordinal < stage_count; ++ordinal) {
        const auto bit = static_cast<std::uint32_t>(1U) << ordinal;
        if ((_success_mask & bit) == 0) {
            continue;  // Failed/unprocessed stages are never published.
        }
        auto* stage = fanout->vqec_vision_ai_appl_ftfan_get_stage(ordinal);
        if (stage == nullptr) {
            continue;
        }
        const auto& config = stage->vqec_vision_ai_ftmgr_ftstg_get_config();
        const auto& batch = _events[ordinal];
        for (std::size_t index = 0; index < batch.events_.size(); ++index) {
            ++_report.attempted_;
            const auto delivered = vqec_vision_ai_outpt_ftdsp_dispatch_event(
                batch, index, config, _policy_revision, _steady_now_ns,
                *delivery_gate_, *delivery_sink_);
            if (delivered.code_ == status_code::ok) {
                ++_report.accepted_;
                continue;
            }
            if (delivered.code_ == status_code::unauthorized) {
                ++_report.denied_;
            } else {
                ++_report.failed_;
            }
            if (_report.first_error_code_ == status_code::ok) {
                _report.first_error_code_ = delivered.code_;
                _report.first_error_slot_ = ordinal;
            }
        }
    }
    metrics_.events_accepted_ += _report.accepted_;
    metrics_.events_denied_ += _report.denied_;
    metrics_.events_failed_ += _report.failed_;
    return _report.first_error_code_ == status_code::ok ?
        status{} :
        status{_report.first_error_code_, "feature event delivery rejected an output"};
}

application_composition_snapshot
runtime_executor::vqec_vision_ai_appl_rtexe_get_snapshot() const noexcept {
    return composition_.vqec_vision_ai_cntr_acomp_get_snapshot();
}

runtime_executor_metrics
runtime_executor::vqec_vision_ai_appl_rtexe_get_metrics() const noexcept {
    return metrics_;
}

bool runtime_executor::vqec_vision_ai_appl_rtexe_has_pending() const noexcept {
    return has_pending_;
}

bool runtime_executor::vqec_vision_ai_appl_rtexe_is_activation_quiescent() const {
    return !has_pending_ &&
        composition_.vqec_vision_ai_appl_acomp_is_activation_quiescent();
}

}  // namespace vqec::vision::ai
