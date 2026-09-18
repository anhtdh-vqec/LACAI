#include "vqec_vision_multi_model_pump.hpp"

#include <limits>
#include <new>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {

multi_model_pump::multi_model_pump(raw_source_port& _source) : source_(_source) {}

multi_model_pump::~multi_model_pump() noexcept {
    vqec_vision_ai_appl_mmump_stop_model_workers();
}

status multi_model_pump::vqec_vision_ai_appl_mmump_configure(
    const model_cadence_config& _cadence,
    const std::array<multi_model_graph_binding,
        deployment_limits::g_max_models_per_source>& _bindings,
    std::uint16_t _binding_count, bool _use_model_workers) {
    if (is_configured_) {
        return {status_code::invalid_state, "multi-model pump is already configured"};
    }
    if (_binding_count == 0 ||
        _binding_count > deployment_limits::g_max_models_per_source ||
        _binding_count != _cadence.model_count_) {
        return {status_code::invalid_argument, "graph and cadence counts differ"};
    }
    std::array<multi_model_graph_binding, deployment_limits::g_max_models_per_source>
        bindings{};
    for (std::uint16_t slot = 0; slot < _binding_count; ++slot) {
        const auto& binding = _bindings[slot];
        if (binding.graph_ == nullptr || binding.cycle_id_ == 0 ||
            binding.job_timeout_ns_ == 0 ||
            binding.job_timeout_ns_ == std::numeric_limits<std::uint64_t>::max()) {
            return {status_code::invalid_argument, "invalid model graph binding"};
        }
        if ((binding.processor_ == nullptr) != (binding.plan_ == nullptr)) {
            return {status_code::invalid_argument,
                "preprocessing requires both a processor and an inference plan"};
        }
        if (_use_model_workers &&
            _cadence.dispatch_policies_[slot] != model_dispatch_policy::drop_if_busy) {
            return {status_code::unsupported,
                "parallel model workers require drop-if-busy dispatch"};
        }
        for (std::uint16_t prior = 0; prior < slot; ++prior) {
            if (_bindings[prior].graph_ == binding.graph_) {
                return {status_code::invalid_argument, "model graph is bound more than once"};
            }
        }
        bindings[slot] = binding;
    }
    model_cadence_scheduler cadence;
    const auto configured = cadence.vqec_vision_ai_sched_mdcad_configure(_cadence);
    if (configured.code_ != status_code::ok) {
        return configured;
    }
    bindings_ = bindings;
    cadence_ = cadence;
    model_count_ = _binding_count;
    has_cascade_root_ = false;
    for (std::uint16_t slot = 0; slot < _binding_count; ++slot) {
        has_cascade_root_ = has_cascade_root_ || bindings[slot].cascade_root_;
    }
    has_target_spec_.fill(false);
    use_model_workers_ = _use_model_workers;
    is_configured_ = true;
    return {};
}

status multi_model_pump::vqec_vision_ai_appl_mmump_bind_cascade_store(
    cascade_frame_store& _store, std::uint32_t _camera_id, std::uint32_t _channel_id) {
    if (cascade_store_ != nullptr || has_received_frame_) {
        return {status_code::invalid_state, "cascade frame store is already bound"};
    }
    cascade_store_ = &_store;
    camera_id_ = _camera_id;
    channel_id_ = _channel_id;
    return {};
}

status multi_model_pump::vqec_vision_ai_appl_mmump_resolve_targets() {
    if (!is_configured_) {
        return {status_code::invalid_state, "multi-model pump is not configured"};
    }
    for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
        if (bindings_[slot].processor_ == nullptr) {
            has_target_spec_[slot] = false;
            continue;
        }
        std::vector<tensor_spec> inputs;
        const auto specs =
            bindings_[slot].graph_->vqec_vision_ai_ports_infgr_get_input_specs(inputs);
        if (specs.code_ != status_code::ok) {
            return specs;
        }
        if (inputs.size() != 1) {
            return {status_code::unsupported,
                "tensor preprocessing requires exactly one model input"};
        }
        target_specs_[slot] = inputs[0];
        has_target_spec_[slot] = true;
    }
    return use_model_workers_ ? vqec_vision_ai_appl_mmump_start_model_workers() : status{};
}

void multi_model_pump::vqec_vision_ai_appl_mmump_begin_stop() noexcept {
    is_stopping_ = true;
    vqec_vision_ai_appl_mmump_stop_model_workers();
    preview_frame_ = {};
    has_preview_frame_ = false;
    for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
        if (bindings_[slot].graph_ == nullptr ||
            bindings_[slot].graph_->vqec_vision_ai_ports_infgr_get_outstanding() == 0) {
            retained_frames_[slot] = {};
        }
    }
}

status multi_model_pump::vqec_vision_ai_appl_mmump_poll_result(
    std::uint64_t _steady_now_ns, tensor_result& _result,
    multi_model_pump_report& _report) {
    for (std::uint16_t offset = 0; offset < model_count_; ++offset) {
        const auto slot = static_cast<std::uint16_t>(
            (result_cursor_ + offset) % model_count_);
        auto& graph = *bindings_[slot].graph_;
        if (graph.vqec_vision_ai_ports_infgr_get_outstanding() == 0) {
            continue;
        }
        const auto ticket = graph.vqec_vision_ai_ports_infgr_get_pending_ticket();
        tensor_result candidate;
        const auto polled = graph.vqec_vision_ai_ports_infgr_poll_result(
            _steady_now_ns, candidate);
        if (polled.code_ == status_code::ok) {
            _result = std::move(candidate);
            _report.result_ticket_ = ticket;
            _report.result_model_slot_ = slot;
            _report.has_result_ = true;
            // Result delivery transfers this slot's retained source-frame ownership to
            // the serialized consumer. Keeping a second copy here can exhaust a bounded
            // FW producer when several model slots complete on different frames.
            _report.frame_ = std::move(retained_frames_[slot]);
            _report.has_frame_ = _report.frame_.owner_ != nullptr;
            result_cursor_ = static_cast<std::uint16_t>((slot + 1U) % model_count_);
            return {};
        }
        if (polled.code_ != status_code::pending) {
            _report.error_model_slot_ = slot;
            is_failed_ = true;
            return polled;
        }
    }
    return {status_code::pending, "no model result available"};
}

status multi_model_pump::vqec_vision_ai_appl_mmump_pump_step(
    std::uint64_t _steady_now_ns, tensor_result& _result,
    multi_model_pump_report& _report) {
    _report = {};
    if (!is_configured_) {
        return {status_code::invalid_state, "multi-model pump is not configured"};
    }
    if (has_cascade_root_ && cascade_store_ == nullptr) {
        return {status_code::invalid_state,
            "cascade-root binding requires a bound cascade frame store"};
    }
    if (_steady_now_ns == std::numeric_limits<std::uint64_t>::max() ||
        _steady_now_ns < last_now_ns_) {
        return {status_code::invalid_argument, "pump requires monotonic steady time"};
    }
    last_now_ns_ = _steady_now_ns;
    if (is_failed_) {
        return {status_code::invalid_state, "pump failed; source session must drain"};
    }

    const auto harvested = vqec_vision_ai_appl_mmump_harvest_model_work(_report);
    if (harvested.code_ != status_code::ok) {
        is_failed_ = true;
        return harvested;
    }

    const auto result = vqec_vision_ai_appl_mmump_poll_result(
        _steady_now_ns, _result, _report);
    if (result.code_ != status_code::pending) {
        return result;
    }
    if (is_stopping_) {
        return {status_code::pending, "pump stopped receiving; graph drain is external"};
    }
    if (source_.vqec_vision_ai_ports_rawsr_get_state() != raw_source_state::running) {
        is_failed_ = true;
        return {status_code::invalid_state, "pump requires a running RAW source"};
    }

    bool has_available_graph = false;
    for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
        const auto& graph = *bindings_[slot].graph_;
        if (graph.vqec_vision_ai_ports_infgr_get_state() !=
            inference_graph_state::running) {
            _report.error_model_slot_ = slot;
            is_failed_ = true;
            return {status_code::invalid_state, "pump requires every model graph running"};
        }
        has_available_graph = has_available_graph ||
            (graph.vqec_vision_ai_ports_infgr_get_outstanding() == 0 &&
             !vqec_vision_ai_appl_mmump_model_busy(slot));
    }
    if (!has_available_graph) {
        return {status_code::pending, "all model graphs have outstanding jobs"};
    }

    raw_frame frame;
    const auto received = source_.vqec_vision_ai_ports_rawsr_receive(frame, 0);
    if (received.code_ == status_code::timeout) {
        return {status_code::pending, "no RAW frame available"};
    }
    if (received.code_ != status_code::ok) {
        is_failed_ = true;
        return received;
    }
    if (!frame.owner_ || frame.descriptor_.buffer_id_ == 0 ||
        frame.descriptor_.session_epoch_ == 0) {
        is_failed_ = true;
        return {status_code::protocol_error, "RAW frame has no valid identity or owner"};
    }
    has_received_frame_ = true;
    if (last_source_epoch_ != 0 &&
        frame.descriptor_.session_epoch_ != last_source_epoch_) {
        // Source epoch changed: never submit parked old-epoch inputs into the new epoch.
        for (auto& parked : pending_) {
            parked.has_ = false;
            parked.blobs_.clear();
        }
    }
    last_source_epoch_ = frame.descriptor_.session_epoch_;
    preview_frame_ = frame;
    has_preview_frame_ = true;

    model_cadence_selection selection;
    const auto selected = cadence_.vqec_vision_ai_sched_mdcad_select(
        frame.descriptor_.buffer_id_, selection);
    if (selected.code_ != status_code::ok) {
        is_failed_ = true;
        return selected;
    }
    _report.due_model_mask_ = selection.due_model_mask_;
    _report.skipped_cadence_intervals_ = selection.skipped_intervals_;

    // Reserve every newly due graph before the first submit. In particular, a shared
    // backend retention domain must fail before any graph starts reading this frame.
    for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
        const auto bit = static_cast<std::uint16_t>(1U << slot);
        if ((selection.due_model_mask_ & bit) == 0) {
            continue;
        }
        auto& graph = *bindings_[slot].graph_;
        if (graph.vqec_vision_ai_ports_infgr_get_outstanding() != 0 ||
            vqec_vision_ai_appl_mmump_model_busy(slot)) {
            // drop_if_busy: skip. latest_wins/replace_pending: park the newest due input in
            // the one-slot mailbox and submit it once the graph frees up.
            model_dispatch_policy policy{model_dispatch_policy::drop_if_busy};
            bool parked = false;
            if (bindings_[slot].processor_ != nullptr &&
                vqec_vision_ai_appl_mmump_get_policy(slot, policy).code_ == status_code::ok &&
                (policy == model_dispatch_policy::latest_wins ||
                    policy == model_dispatch_policy::replace_pending)) {
                const auto stored = vqec_vision_ai_appl_mmump_store_pending(slot, frame);
                if (stored.code_ != status_code::ok) {
                    _report.error_model_slot_ = slot;
                    is_failed_ = true;
                    return stored;
                }
                parked = true;
            }
            if (parked) {
                _report.pending_model_mask_ = static_cast<std::uint16_t>(
                    _report.pending_model_mask_ | bit);
            } else {
                _report.busy_model_mask_ = static_cast<std::uint16_t>(
                    _report.busy_model_mask_ | bit);
            }
            continue;
        }
        if (!is_armed_[slot]) {
            const auto armed = graph.vqec_vision_ai_ports_infgr_arm(
                bindings_[slot].cycle_id_, frame.descriptor_.session_epoch_,
                bindings_[slot].job_timeout_ns_,
                submission_sequence_policy::unique_source_frames);
            if (armed.code_ != status_code::ok) {
                _report.error_model_slot_ = slot;
                is_failed_ = true;
                return armed;
            }
            is_armed_[slot] = true;
        }
    }

    // Retain the source frame once for every due cascade-root slot before any of them reads
    // it. All cascade models on one frame share a single store entry; if the store has no
    // admitted budget none of them can run.
    std::uint16_t cascade_active = 0;
    if (has_cascade_root_) {
        for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
            const auto bit = static_cast<std::uint16_t>(1U << slot);
            if (bindings_[slot].cascade_root_ && (selection.due_model_mask_ & bit) != 0 &&
                (_report.busy_model_mask_ & bit) == 0 &&
                (_report.pending_model_mask_ & bit) == 0) {
                cascade_active = static_cast<std::uint16_t>(cascade_active | bit);
            }
        }
    }
    preview_frame_key cascade_key;
    bool cascade_retained = false;
    if (cascade_active != 0) {
        cascade_key.camera_id_ = camera_id_;
        cascade_key.channel_id_ = channel_id_;
        cascade_key.source_epoch_ = frame.descriptor_.session_epoch_;
        cascade_key.frame_id_ = frame.descriptor_.buffer_id_;
        cascade_key.source_pts_ns_ = frame.descriptor_.pts_ns_;
        const auto retained = cascade_store_->vqec_vision_ai_sched_cfstr_retain(
            cascade_key, frame);
        if (retained.code_ == status_code::resource_exhausted) {
            // No budget for this frame: every cascade-root model for it is dropped, not just
            // one, because they all need the same pixels.
            _report.cascade_dropped_model_mask_ = cascade_active;
            cascade_active = 0;
        } else if (retained.code_ != status_code::ok) {
            for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
                if ((cascade_active & (1U << slot)) != 0) {
                    _report.error_model_slot_ = slot;
                    break;
                }
            }
            is_failed_ = true;
            return retained;
        } else {
            cascade_retained = true;
        }
    }
    bool cascade_submitted = false;

    for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
        const auto bit = static_cast<std::uint16_t>(1U << slot);
        if ((selection.due_model_mask_ & bit) == 0 ||
            (_report.busy_model_mask_ & bit) != 0 ||
            (_report.pending_model_mask_ & bit) != 0 ||
            (_report.cascade_dropped_model_mask_ & bit) != 0) {
            continue;
        }
        auto& graph = *bindings_[slot].graph_;
        if (use_model_workers_) {
            const auto queued = vqec_vision_ai_appl_mmump_submit_model_work(
                slot, frame, _steady_now_ns);
            if (queued.code_ != status_code::ok) {
                _report.error_model_slot_ = slot;
                is_failed_ = true;
                return queued;
            }
            if (bindings_[slot].cascade_root_) {
                cascade_submitted = true;
            }
            _report.pending_model_mask_ = static_cast<std::uint16_t>(
                _report.pending_model_mask_ | bit);
            continue;
        }
        submission_ticket ticket;
        status submitted;
        if (bindings_[slot].processor_ != nullptr) {
            auto& blobs = preprocess_buffers_[slot];
            const auto preprocessed = vqec_vision_ai_appl_mmump_preprocess(
                slot, frame, blobs);
            if (preprocessed.code_ != status_code::ok) {
                _report.error_model_slot_ = slot;
                is_failed_ = true;
                return preprocessed;
            }
            // A fresh submission supersedes any parked input for this slot.
            pending_[slot].has_ = false;
            pending_[slot].blobs_.clear();
            submitted = graph.vqec_vision_ai_ports_infgr_submit_tensors(
                frame.descriptor_.session_epoch_, frame.descriptor_.buffer_id_,
                frame.descriptor_.pts_ns_, blobs, _steady_now_ns, ticket);
        } else {
            submitted = graph.vqec_vision_ai_ports_infgr_submit_frame(
                frame, _steady_now_ns, ticket);
        }
        if (ticket.token_.job_id_ != 0) {
            if (bindings_[slot].cascade_root_) {
                cascade_submitted = true;
            }
            _report.submitted_tickets_[slot] = ticket;
            _report.submitted_model_mask_ = static_cast<std::uint16_t>(
                _report.submitted_model_mask_ | bit);
            retained_frames_[slot] = frame;
        }
        if (submitted.code_ == status_code::pending &&
            ticket.token_.job_id_ == 0) {
            _report.busy_model_mask_ = static_cast<std::uint16_t>(
                _report.busy_model_mask_ | bit);
            continue;
        }
        if (submitted.code_ != status_code::ok) {
            _report.error_model_slot_ = slot;
            is_failed_ = true;
            return submitted;
        }
    }
    if (cascade_retained && !cascade_submitted) {
        // No cascade-root graph accepted the frame: roll back so it is not left charged
        // without a matching result.
        (void)cascade_store_->vqec_vision_ai_sched_cfstr_retire(cascade_key);
        cascade_retained = false;
    }
    if (cascade_retained) {
        _report.cascade_retained_model_mask_ = cascade_active;
    }
    const auto flushed = vqec_vision_ai_appl_mmump_flush_pending(_steady_now_ns, _report);
    if (flushed.code_ != status_code::ok) {
        return flushed;
    }
    if (_report.submitted_model_mask_ != 0) {
        return {};
    }
    if (_report.pending_model_mask_ != 0) {
        return {status_code::pending, "due input parked until the graph frees up"};
    }
    return {status_code::pending, "RAW frame skipped because no due graph accepted it"};
}

status multi_model_pump::vqec_vision_ai_appl_mmump_take_preview_frame(
    raw_frame& _frame) {
    if (!has_preview_frame_) {
        return {status_code::pending, "no preview frame is retained"};
    }
    _frame = std::move(preview_frame_);
    preview_frame_ = {};
    has_preview_frame_ = false;
    return {};
}

status multi_model_pump::vqec_vision_ai_appl_mmump_ensure_target(std::uint16_t _slot) {
    if (has_target_spec_[_slot]) {
        return {};
    }
    std::vector<tensor_spec> inputs;
    const auto specs =
        bindings_[_slot].graph_->vqec_vision_ai_ports_infgr_get_input_specs(inputs);
    if (specs.code_ != status_code::ok) {
        return specs;
    }
    if (inputs.size() != 1) {
        return {status_code::unsupported,
            "tensor preprocessing requires exactly one model input"};
    }
    const auto bytes = vqec_vision_ai_core_tnctr_shape_bytes(inputs[0]);
    if (bytes == 0) {
        return {status_code::unsupported, "model input tensor has no bytes"};
    }
    target_specs_[_slot] = inputs[0];
    has_target_spec_[_slot] = true;
    auto& buffer = preprocess_buffers_[_slot];
    buffer.clear();
    buffer.push_back(tensor_blob{});
    buffer[0].spec_ = target_specs_[_slot];
    buffer[0].bytes_.assign(static_cast<std::size_t>(bytes), 0U);
    return {};
}

status multi_model_pump::vqec_vision_ai_appl_mmump_preprocess(
    std::uint16_t _slot, const raw_frame& _frame, std::vector<tensor_blob>& _blobs) {
    if (bindings_[_slot].processor_ == nullptr || bindings_[_slot].plan_ == nullptr) {
        return {status_code::invalid_state, "binding has no preprocessing stage"};
    }
    const auto target = vqec_vision_ai_appl_mmump_ensure_target(_slot);
    if (target.code_ != status_code::ok) {
        return target;
    }
    return bindings_[_slot].processor_->vqec_vision_ai_ports_imgpr_preprocess(
        _frame, *bindings_[_slot].plan_, target_specs_[_slot], _blobs);
}

status multi_model_pump::vqec_vision_ai_appl_mmump_get_policy(
    std::uint16_t _slot, model_dispatch_policy& _policy) const noexcept {
    return cadence_.vqec_vision_ai_sched_mdcad_get_dispatch_policy(_slot, _policy);
}

status multi_model_pump::vqec_vision_ai_appl_mmump_store_pending(
    std::uint16_t _slot, const raw_frame& _frame) {
    std::vector<tensor_blob> blobs;
    const auto preprocessed = vqec_vision_ai_appl_mmump_preprocess(_slot, _frame, blobs);
    if (preprocessed.code_ != status_code::ok) {
        return preprocessed;
    }
    auto& pending = pending_[_slot];
    pending.blobs_ = std::move(blobs);
    pending.source_epoch_ = _frame.descriptor_.session_epoch_;
    pending.source_frame_id_ = _frame.descriptor_.buffer_id_;
    pending.source_pts_ns_ = _frame.descriptor_.pts_ns_;
    pending.has_ = true;
    return {};
}

status multi_model_pump::vqec_vision_ai_appl_mmump_flush_pending(
    std::uint64_t _steady_now_ns, multi_model_pump_report& _report) {
    for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
        auto& pending = pending_[slot];
        if (!pending.has_) {
            continue;
        }
        auto& graph = *bindings_[slot].graph_;
        if (graph.vqec_vision_ai_ports_infgr_get_outstanding() != 0 ||
            !is_armed_[slot]) {
            continue;
        }
        submission_ticket ticket;
        const auto submitted = graph.vqec_vision_ai_ports_infgr_submit_tensors(
            pending.source_epoch_, pending.source_frame_id_, pending.source_pts_ns_,
            pending.blobs_, _steady_now_ns, ticket);
        if (submitted.code_ == status_code::ok) {
            pending.has_ = false;
            pending.blobs_.clear();
            if (ticket.token_.job_id_ != 0) {
                _report.submitted_tickets_[slot] = ticket;
                _report.submitted_model_mask_ = static_cast<std::uint16_t>(
                    _report.submitted_model_mask_ | (1U << slot));
            }
        } else if (submitted.code_ != status_code::pending) {
            _report.error_model_slot_ = slot;
            is_failed_ = true;
            return submitted;
        }
    }
    return {};
}

void multi_model_pump::vqec_vision_ai_appl_mmump_model_worker_main(
    std::uint16_t _slot) noexcept {
    while (true) {
        raw_frame frame;
        std::uint64_t steady_now_ns = 0;
        {
            std::unique_lock<std::mutex> lock(model_worker_mutex_);
            model_worker_conditions_[_slot].wait(lock, [this, _slot]() {
                return model_worker_jobs_[_slot].exiting_ ||
                    model_worker_jobs_[_slot].pending_;
            });
            auto& job = model_worker_jobs_[_slot];
            if (job.exiting_ && !job.pending_) {
                return;
            }
            frame = job.frame_;
            steady_now_ns = job.steady_now_ns_;
            job.pending_ = false;
            job.running_ = true;
        }

        status submitted;
        submission_ticket ticket;
        try {
            auto& graph = *bindings_[_slot].graph_;
            if (bindings_[_slot].processor_ != nullptr) {
                auto& blobs = preprocess_buffers_[_slot];
                const auto preprocessed = vqec_vision_ai_appl_mmump_preprocess(
                    _slot, frame, blobs);
                if (preprocessed.code_ == status_code::ok) {
                    submitted = graph.vqec_vision_ai_ports_infgr_submit_tensors(
                        frame.descriptor_.session_epoch_, frame.descriptor_.buffer_id_,
                        frame.descriptor_.pts_ns_, blobs, steady_now_ns, ticket);
                } else {
                    submitted = preprocessed;
                }
            } else {
                submitted = graph.vqec_vision_ai_ports_infgr_submit_frame(
                    frame, steady_now_ns, ticket);
            }
        } catch (const std::bad_alloc&) {
            submitted = {status_code::resource_exhausted,
                "model worker allocation failed"};
        } catch (...) {
            submitted = {status_code::io_error, "model worker raised an exception"};
        }

        std::lock_guard<std::mutex> lock(model_worker_mutex_);
        auto& job = model_worker_jobs_[_slot];
        job.result_ = std::move(submitted);
        job.ticket_ = ticket;
        job.running_ = false;
        job.completed_ = true;
    }
}

status multi_model_pump::vqec_vision_ai_appl_mmump_start_model_workers() {
    if (model_workers_started_ || !use_model_workers_) {
        return model_workers_started_ ?
            status{status_code::invalid_state, "model workers are already started"} : status{};
    }
    try {
        for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
            model_workers_[slot] = std::thread(
                [this, slot]() { vqec_vision_ai_appl_mmump_model_worker_main(slot); });
        }
    } catch (...) {
        {
            std::lock_guard<std::mutex> lock(model_worker_mutex_);
            for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
                model_worker_jobs_[slot].exiting_ = true;
                model_worker_jobs_[slot].pending_ = false;
                model_worker_jobs_[slot].frame_ = {};
            }
        }
        for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
            model_worker_conditions_[slot].notify_all();
            if (model_workers_[slot].joinable()) {
                model_workers_[slot].join();
            }
        }
        return {status_code::resource_exhausted, "cannot start model workers"};
    }
    model_workers_started_ = true;
    return {};
}

void multi_model_pump::vqec_vision_ai_appl_mmump_stop_model_workers() noexcept {
    if (!model_workers_started_) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(model_worker_mutex_);
        for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
            auto& job = model_worker_jobs_[slot];
            job.exiting_ = true;
            if (job.pending_) {
                job.pending_ = false;
                if (bindings_[slot].cascade_root_ && cascade_store_ != nullptr &&
                    job.frame_.owner_ != nullptr) {
                    const preview_frame_key key{camera_id_, channel_id_,
                        job.frame_.descriptor_.session_epoch_,
                        job.frame_.descriptor_.buffer_id_,
                        job.frame_.descriptor_.pts_ns_};
                    (void)cascade_store_->vqec_vision_ai_sched_cfstr_retire(key);
                }
                job.frame_ = {};
            }
        }
    }
    for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
        model_worker_conditions_[slot].notify_all();
    }
    for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
        if (model_workers_[slot].joinable()) {
            model_workers_[slot].join();
        }
    }
    std::lock_guard<std::mutex> lock(model_worker_mutex_);
    for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
        model_worker_jobs_[slot] = {};
    }
    model_workers_started_ = false;
}

bool multi_model_pump::vqec_vision_ai_appl_mmump_model_busy(
    std::uint16_t _slot) const noexcept {
    if (!use_model_workers_) {
        return false;
    }
    std::lock_guard<std::mutex> lock(model_worker_mutex_);
    const auto& job = model_worker_jobs_[_slot];
    return job.pending_ || job.running_ || job.completed_;
}

status multi_model_pump::vqec_vision_ai_appl_mmump_submit_model_work(
    std::uint16_t _slot, const raw_frame& _frame, std::uint64_t _steady_now_ns) {
    std::lock_guard<std::mutex> lock(model_worker_mutex_);
    if (!model_workers_started_ || _slot >= model_count_) {
        return {status_code::invalid_state, "model worker is unavailable"};
    }
    auto& job = model_worker_jobs_[_slot];
    if (job.pending_ || job.running_ || job.completed_ || job.exiting_) {
        return {status_code::resource_exhausted, "model worker is busy"};
    }
    job.frame_ = _frame;
    job.steady_now_ns_ = _steady_now_ns;
    job.pending_ = true;
    model_worker_conditions_[_slot].notify_one();
    return {};
}

status multi_model_pump::vqec_vision_ai_appl_mmump_harvest_model_work(
    multi_model_pump_report& _report) {
    if (!use_model_workers_) {
        return {};
    }
    for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
        status completed;
        submission_ticket ticket;
        raw_frame frame;
        {
            std::lock_guard<std::mutex> lock(model_worker_mutex_);
            auto& job = model_worker_jobs_[slot];
            if (!job.completed_) {
                continue;
            }
            completed = std::move(job.result_);
            ticket = job.ticket_;
            frame = std::move(job.frame_);
            job.result_ = {};
            job.ticket_ = {};
            job.steady_now_ns_ = 0;
            job.completed_ = false;
        }
        if (completed.code_ != status_code::ok) {
            _report.error_model_slot_ = slot;
            return completed;
        }
        if (ticket.token_.job_id_ == 0) {
            _report.error_model_slot_ = slot;
            return {status_code::protocol_error,
                "model worker completed without a submission ticket"};
        }
        const auto bit = static_cast<std::uint16_t>(1U << slot);
        _report.submitted_tickets_[slot] = ticket;
        _report.submitted_model_mask_ = static_cast<std::uint16_t>(
            _report.submitted_model_mask_ | bit);
        retained_frames_[slot] = std::move(frame);
    }
    return {};
}

std::uint16_t multi_model_pump::vqec_vision_ai_appl_mmump_get_model_count()
    const noexcept {
    return model_count_;
}

bool multi_model_pump::vqec_vision_ai_appl_mmump_has_failed() const noexcept {
    return is_failed_;
}

}  // namespace vqec::vision::ai
