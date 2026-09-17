#include "vqec_vision_cascade_execution_worker.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_color.hpp"

namespace vqec::vision::ai {
namespace {

status vqec_vision_ai_appl_cxwrk_quantize(
    const alignment_result& _aligned, const tensor_spec& _target,
    const std::array<float, 3>& _offset, const std::array<float, 3>& _scale,
    std::vector<std::uint16_t>& _workspace, tensor_blob& _blob) {
    const auto& spec = _aligned.tensor_.spec_;
    if (spec.dtype_ != tensor_element_type::uint8 || spec.dimensions_.size() != 4U ||
        spec.dimensions_[0] != 1U || spec.dimensions_[3] != 3U) {
        return {status_code::unsupported, "aligned tensor is not a uint8 RGB patch"};
    }
    const std::uint32_t width = spec.dimensions_[2];
    const std::uint32_t height = spec.dimensions_[1];
    const std::size_t expected = static_cast<std::size_t>(width) * height * 3U;
    if (_aligned.tensor_.bytes_.size() != expected) {
        return {status_code::unsupported, "aligned tensor byte count is inconsistent"};
    }
    if (_target.dtype_ != tensor_element_type::uint16 ||
        _target.dimensions_.size() != 4U || _target.dimensions_[0] != 1U ||
        _target.dimensions_[1] != height || _target.dimensions_[2] != width ||
        _target.dimensions_[3] != 3U) {
        return {status_code::invalid_argument,
            "aligned patch does not match the model input spec"};
    }
    if (_workspace.size() != expected || _blob.spec_.name_ != _target.name_ ||
        _blob.bytes_.size() != expected * sizeof(std::uint16_t)) {
        return {status_code::invalid_state,
            "embedding input workspace does not match the loaded graph"};
    }
    const auto converted = vqec_vision_ai_core_color_quantize_rgb8_to_uint16(
        _aligned.tensor_.bytes_.data(), width, height, width * 3U, _offset, _scale,
        _target.quantization_.scale_, _target.quantization_.zero_point_, _workspace.data());
    if (converted.code_ != status_code::ok) {
        return converted;
    }
    std::memcpy(_blob.bytes_.data(), _workspace.data(), _blob.bytes_.size());
    return {};
}

}  // namespace

cascade_execution_worker::~cascade_execution_worker() noexcept {
    const auto drained = vqec_vision_ai_appl_cxwrk_drain_and_join(
        cascade_worker_limits::g_default_join_timeout_ns);
    // A timed-out worker still borrows this object and its hardware ports. The process
    // must fail-stop rather than detach the thread and destroy those owners underneath it.
    if (drained.code_ != status_code::ok) {
        std::terminate();
    }
}

status cascade_execution_worker::vqec_vision_ai_appl_cxwrk_configure(
    const cascade_coordinator_config& _config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_configured_) {
        return {status_code::invalid_state, "cascade worker is already configured"};
    }
    if (_config.aligner_ == nullptr || _config.lease_ == nullptr ||
        _config.max_tasks_per_frame_ == 0 ||
        _config.max_tasks_per_frame_ > image_alignment_limits::g_max_points) {
        return {status_code::invalid_argument, "invalid cascade worker configuration"};
    }
    const bool wants_embedding =
        _config.embedding_graph_ != nullptr || _config.embedding_decoder_ != nullptr;
    if (wants_embedding &&
        (_config.embedding_graph_ == nullptr || _config.embedding_decoder_ == nullptr ||
         _config.cycle_id_ == 0 || _config.job_timeout_ns_ == 0 ||
         _config.job_timeout_ns_ == std::numeric_limits<std::uint64_t>::max())) {
        return {status_code::invalid_argument, "invalid secondary embedding configuration"};
    }
    alignment_capabilities capabilities;
    const auto probed = _config.aligner_->vqec_vision_ai_ports_imaln_probe_capabilities(
        capabilities);
    if (probed.code_ != status_code::ok) {
        return probed;
    }
    const auto checked = _config.aligner_->vqec_vision_ai_ports_imaln_validate_template(
        _config.template_, capabilities);
    if (checked.code_ != status_code::ok) {
        return checked;
    }
    tensor_spec input_spec;
    if (wants_embedding) {
        const auto backend_capabilities =
            _config.embedding_graph_->vqec_vision_ai_ports_infgr_get_capabilities();
        if (backend_capabilities.supports_async_ ||
            backend_capabilities.max_inflight_jobs_ != 1U) {
            return {status_code::unsupported,
                "cascade worker requires a synchronous single-inflight embedding graph"};
        }
        std::vector<tensor_spec> inputs;
        const auto specs =
            _config.embedding_graph_->vqec_vision_ai_ports_infgr_get_input_specs(inputs);
        if (specs.code_ != status_code::ok || inputs.size() != 1U) {
            return {status_code::unsupported,
                "embedding graph must expose exactly one input tensor spec"};
        }
        input_spec = inputs[0];
        if (input_spec.dtype_ != tensor_element_type::uint16 ||
            input_spec.dimensions_.size() != 4U || input_spec.dimensions_[0] != 1U ||
            input_spec.dimensions_[1] != _config.template_.destination_height_ ||
            input_spec.dimensions_[2] != _config.template_.destination_width_ ||
            input_spec.dimensions_[3] != 3U ||
            !input_spec.quantization_.is_quantized_ ||
            !std::isfinite(input_spec.quantization_.scale_) ||
            !(input_spec.quantization_.scale_ > 0.0F)) {
            return {status_code::invalid_argument,
                "embedding graph input spec does not match the alignment template"};
        }
    }
    config_ = _config;
    metrics_ = {};
    embedding_input_spec_ = input_spec;
    has_embedding_input_spec_ = wants_embedding;
    if (wants_embedding) {
        const auto elements = static_cast<std::size_t>(input_spec.dimensions_[1]) *
            input_spec.dimensions_[2] * input_spec.dimensions_[3];
        try {
            embedding_quantized_workspace_.assign(elements, 0U);
            embedding_inputs_.assign(1U, tensor_blob{});
            embedding_inputs_[0].spec_ = input_spec;
            embedding_inputs_[0].bytes_.assign(
                elements * sizeof(std::uint16_t), 0U);
        } catch (const std::bad_alloc&) {
            embedding_quantized_workspace_.clear();
            embedding_inputs_.clear();
            return {status_code::resource_exhausted,
                "cannot allocate embedding input workspace"};
        }
    }
    is_configured_ = true;
    return {};
}

status cascade_execution_worker::vqec_vision_ai_appl_cxwrk_start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_configured_) {
        return {status_code::invalid_state, "cascade worker is not configured"};
    }
    if (is_running_) {
        return {status_code::invalid_state, "cascade worker is already running"};
    }
    is_running_ = true;
    is_stopping_ = false;
    is_exiting_ = false;
    worker_finished_ = false;
    try {
        worker_ = std::thread(
            [this]() { vqec_vision_ai_appl_cxwrk_worker_loop(); });
    } catch (...) {
        is_running_ = false;
        return {status_code::resource_exhausted, "cannot launch cascade worker thread"};
    }
    return {};
}

status cascade_execution_worker::vqec_vision_ai_appl_cxwrk_schedule(
    std::uint64_t _steady_now_ns, const observation_batch& _batch,
    std::uint64_t _policy_revision) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!is_configured_ || !is_running_ || is_stopping_ || is_exiting_) {
        auto* lease = config_.lease_;
        const bool is_drain = is_stopping_ || is_exiting_;
        lock.unlock();
        if (lease != nullptr && _batch.frame_.source_epoch_ != 0 &&
            _batch.frame_.frame_id_ != 0) {
            (void)lease->vqec_vision_ai_ports_cflse_retire(_batch.frame_);
        }
        return is_drain ? status{} :
            status{status_code::invalid_state, "cascade worker is not accepting tasks"};
    }
    if (_batch.frame_.source_epoch_ == 0 || _batch.frame_.frame_id_ == 0) {
        return {status_code::invalid_argument, "cascade batch has no source frame identity"};
    }
    if (pending_queue_.size() + completion_queue_.size() +
            static_cast<std::size_t>(has_inflight_) >=
        cascade_worker_limits::g_max_queue_depth) {
        const auto count = static_cast<std::uint64_t>(_batch.observations_.size());
        metrics_.tasks_skipped_ += count;
        lock.unlock();
        if (config_.lease_ != nullptr) {
            (void)config_.lease_->vqec_vision_ai_ports_cflse_retire(_batch.frame_);
        }
        return {status_code::resource_exhausted, "cascade execution worker queue is full"};
    }
    cascade_worker_task task;
    task.steady_now_ns_ = _steady_now_ns;
    task.policy_revision_ = _policy_revision;
    task.key_ = _batch.frame_;
    task.geometry_ = _batch.geometry_;
    task.observations_ = _batch.observations_;
    pending_queue_.push_back(std::move(task));
    metrics_.queue_depth_ = pending_queue_.size();
    if (metrics_.oldest_job_ns_ == 0) {
        metrics_.oldest_job_ns_ = _steady_now_ns;
    }
    work_cv_.notify_one();
    return {};
}

status cascade_execution_worker::vqec_vision_ai_appl_cxwrk_poll_completion(
    cascade_worker_completion& _completion) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (completion_queue_.empty()) {
        return {status_code::pending, "no cascade completion ready"};
    }
    _completion = std::move(completion_queue_.front());
    completion_queue_.pop_front();
    return {};
}

void cascade_execution_worker::vqec_vision_ai_appl_cxwrk_worker_loop() noexcept {
    while (true) {
        cascade_worker_task current_task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            work_cv_.wait(lock, [this]() {
                return is_exiting_ || !pending_queue_.empty();
            });
            if (is_exiting_ && pending_queue_.empty()) {
                worker_finished_ = true;
                idle_cv_.notify_all();
                return;
            }
            if (!pending_queue_.empty()) {
                current_task = std::move(pending_queue_.front());
                pending_queue_.pop_front();
                metrics_.queue_depth_ = pending_queue_.size();
                has_inflight_ = true;
            }
        }
        if (current_task.key_.source_epoch_ != 0) {
            cascade_worker_completion completion;
            vqec_vision_ai_appl_cxwrk_process_task(current_task, completion);
            completion.tracked_.frame_ = current_task.key_;
            completion.tracked_.geometry_ = current_task.geometry_;
            completion.tracked_.observations_ = std::move(current_task.observations_);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                has_inflight_ = false;
                completion_queue_.push_back(std::move(completion));
                idle_cv_.notify_all();
            }
        }
    }
}

void cascade_execution_worker::vqec_vision_ai_appl_cxwrk_process_task(
    const cascade_worker_task& _task, cascade_worker_completion& _completion) noexcept {
    _completion.key_ = _task.key_;
    _completion.policy_revision_ = _task.policy_revision_;
    _completion.report_ = {};
    _completion.task_status_ = {};

    const auto& key = _task.key_;
    if (config_.lease_ == nullptr || config_.aligner_ == nullptr) {
        _completion.task_status_ = {status_code::invalid_state, "cascade worker missing ports"};
        return;
    }
    const bool embed = config_.embedding_graph_ != nullptr;
    if (embed && armed_source_epoch_ != 0 && armed_source_epoch_ != key.source_epoch_) {
        (void)config_.lease_->vqec_vision_ai_ports_cflse_retire(key);
        _completion.task_status_ = {status_code::invalid_state,
            "secondary graph must restart before source epoch changes"};
        return;
    }

    const auto batch_start_tp = std::chrono::steady_clock::now();
    std::size_t accepted = 0;
    bool task_recovery_required = false;

    for (const auto& observation : _task.observations_) {
        const auto elapsed_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - batch_start_tp).count());
        const bool budget_exhausted = (config_.control_budget_ns_ != 0) &&
            (accepted > 0) && (elapsed_ns >= config_.control_budget_ns_);
        if (accepted >= config_.max_tasks_per_frame_ ||
            observation.landmarks_.points_.empty() ||
            observation.landmarks_.schema_id_.empty() ||
            budget_exhausted) {
            ++_completion.report_.skipped_;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                ++metrics_.tasks_skipped_;
            }
            continue;
        }

        alignment_request request;
        request.frame_ = key;
        request.landmarks_ = observation.landmarks_;
        raw_frame frame;
        std::uint64_t frame_ticket = 0;
        const auto acquired =
            config_.lease_->vqec_vision_ai_ports_cflse_acquire(key, frame, frame_ticket);
        if (acquired.code_ != status_code::ok) {
            if (_completion.task_status_.code_ == status_code::ok) {
                _completion.task_status_ = acquired;
            }
            ++_completion.report_.failed_;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                ++metrics_.tasks_failed_;
            }
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++metrics_.active_tasks_;
        }
        alignment_result result;
        std::uint64_t align_ticket = 0;
        const auto aligned = config_.aligner_->vqec_vision_ai_ports_imaln_align(
            request, frame, config_.template_, result, align_ticket);
        const bool alignment_submitted = aligned.code_ == status_code::ok;
        bool task_ok = alignment_submitted;
        if (!task_ok && _completion.task_status_.code_ == status_code::ok) {
            _completion.task_status_ = aligned;
            std::lock_guard<std::mutex> lock(mutex_);
            metrics_.root_backend_error_code_ = aligned.code_;
        }

        bool align_complete = false;
        if (task_ok) {
            const auto completion = config_.aligner_->
                vqec_vision_ai_ports_imaln_poll_completion(align_ticket, align_complete);
            if (completion.code_ != status_code::ok || !align_complete) {
                task_ok = false;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    metrics_.root_backend_error_code_ = completion.code_ != status_code::ok ?
                        completion.code_ : status_code::protocol_error;
                }
                if (_completion.task_status_.code_ == status_code::ok) {
                    _completion.task_status_ = completion.code_ != status_code::ok ?
                        completion : status{status_code::protocol_error, "alignment incomplete"};
                }
            }
        }

        bool pushed_aligned = false;
        if (task_ok) {
            _completion.aligned_.push_back(std::move(result));
            ++accepted;
            pushed_aligned = true;
        }

        bool pushed_embedding = false;
        if (task_ok && embed) {
            if (armed_source_epoch_ == 0) {
                const auto armed = config_.embedding_graph_->vqec_vision_ai_ports_infgr_arm(
                    config_.cycle_id_, key.source_epoch_, config_.job_timeout_ns_,
                    submission_sequence_policy::repeated_tasks_per_source_frame);
                if (armed.code_ == status_code::ok) {
                    armed_source_epoch_ = key.source_epoch_;
                } else {
                    task_ok = false;
                    std::lock_guard<std::mutex> lock(mutex_);
                    metrics_.root_backend_error_code_ = armed.code_;
                    if (_completion.task_status_.code_ == status_code::ok) {
                        _completion.task_status_ = armed;
                    }
                }
            }
            if (task_ok) {
                const auto quantized = vqec_vision_ai_appl_cxwrk_quantize(
                    _completion.aligned_.back(), embedding_input_spec_,
                    config_.normalize_offset_, config_.normalize_scale_,
                    embedding_quantized_workspace_, embedding_inputs_[0]);
                if (quantized.code_ != status_code::ok) {
                    task_ok = false;
                    if (_completion.task_status_.code_ == status_code::ok) {
                        _completion.task_status_ = quantized;
                    }
                }
            }
            if (task_ok) {
                submission_ticket secondary_ticket;
                const auto submitted =
                    config_.embedding_graph_->vqec_vision_ai_ports_infgr_submit_tensors(
                        key.source_epoch_, key.frame_id_, key.source_pts_ns_, embedding_inputs_,
                        _task.steady_now_ns_, secondary_ticket);
                if (submitted.code_ != status_code::ok) {
                    task_ok = false;
                    std::lock_guard<std::mutex> lock(mutex_);
                    metrics_.root_backend_error_code_ = submitted.code_;
                    if (_completion.task_status_.code_ == status_code::ok) {
                        _completion.task_status_ = submitted;
                    }
                } else {
                    tensor_result tensor;
                    embedding_result embedding;
                    const auto polled =
                        config_.embedding_graph_->vqec_vision_ai_ports_infgr_poll_result(
                            _task.steady_now_ns_, tensor);
                    const auto decoded = polled.code_ == status_code::ok ?
                        config_.embedding_decoder_->vqec_vision_ai_ports_embdec_decode(
                            tensor, key, observation.track_id_, embedding) : polled;
                    if (decoded.code_ == status_code::ok) {
                        _completion.embeddings_.push_back(std::move(embedding));
                        ++_completion.report_.embedded_;
                        {
                            std::lock_guard<std::mutex> lock(mutex_);
                            ++metrics_.tasks_embedded_;
                        }
                        pushed_embedding = true;
                    } else {
                        task_ok = false;
                        std::lock_guard<std::mutex> lock(mutex_);
                        metrics_.root_backend_error_code_ = decoded.code_;
                        if (_completion.task_status_.code_ == status_code::ok) {
                            _completion.task_status_ = decoded;
                        }
                    }
                }
            }
        }

        // Rule 5: Never release or complete lease ticket if hardware did not signal completion.
        const bool must_quarantine = alignment_submitted && !align_complete;
        if (must_quarantine) {
            task_recovery_required = true;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                recovery_required_ = true;
                ++metrics_.quarantine_count_;
            }
            if (_completion.task_status_.code_ == status_code::ok) {
                _completion.task_status_ = {status_code::timeout,
                    "alignment backend timed out; frame lease quarantined"};
            }
        }

        const bool may_complete = !alignment_submitted || align_complete;
        const bool complete_ok = may_complete &&
            config_.lease_->vqec_vision_ai_ports_cflse_complete(frame_ticket).code_ ==
                status_code::ok;
        if (!complete_ok && _completion.task_status_.code_ == status_code::ok) {
            _completion.task_status_ = {status_code::invalid_state,
                "cascade frame lease completion failed"};
        }
        if (complete_ok) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (metrics_.active_tasks_ > 0) {
                --metrics_.active_tasks_;
            }
        }
        if (task_ok && complete_ok) {
            ++_completion.report_.accepted_;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                ++metrics_.tasks_accepted_;
            }
        } else {
            ++_completion.report_.failed_;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                ++metrics_.tasks_failed_;
            }
            if (pushed_aligned) {
                _completion.aligned_.pop_back();
                --accepted;
            }
            if (pushed_embedding) {
                _completion.embeddings_.pop_back();
                --_completion.report_.embedded_;
                std::lock_guard<std::mutex> lock(mutex_);
                --metrics_.tasks_embedded_;
            }
        }
    }

    const auto retired = config_.lease_->vqec_vision_ai_ports_cflse_retire(key);
    if (retired.code_ != status_code::ok && retired.code_ != status_code::invalid_state) {
        if (_completion.task_status_.code_ == status_code::ok) {
            _completion.task_status_ = retired;
        }
        ++_completion.report_.failed_;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++metrics_.tasks_failed_;
        }
    }
    if (task_recovery_required && _completion.task_status_.code_ == status_code::ok) {
        _completion.task_status_ = {status_code::timeout, "retained frame requires recovery"};
    }
}

status cascade_execution_worker::vqec_vision_ai_appl_cxwrk_quiescent_reset(
    std::uint64_t /*_steady_now_ns*/, std::uint64_t _timeout_ns) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!is_running_) {
        return {};
    }
    // Flush pending unstarted tasks and retire their frame leases safely
    is_stopping_ = true;
    std::deque<cascade_worker_task> discarded;
    discarded.swap(pending_queue_);
    for (const auto& task : discarded) {
        metrics_.tasks_skipped_ += task.observations_.size();
    }
    metrics_.queue_depth_ = 0;
    metrics_.oldest_job_ns_ = 0;
    lock.unlock();
    for (const auto& task : discarded) {
        if (config_.lease_ != nullptr) {
            (void)config_.lease_->vqec_vision_ai_ports_cflse_retire(task.key_);
        }
    }
    lock.lock();

    // Wait for inflight task to complete within the bounded timeout
    const auto wait_limit = std::chrono::nanoseconds(_timeout_ns);
    const bool reached_idle = idle_cv_.wait_for(lock, wait_limit, [this]() {
        return !has_inflight_;
    });
    if (!reached_idle) {
        recovery_required_ = true;
        metrics_.root_backend_error_code_ = status_code::timeout;
        return {status_code::timeout,
            "quiescent reset timed out waiting for inflight task; recovery required"};
    }
    if (stop_ns_ == 0 && !is_exiting_) {
        is_stopping_ = false;
    }
    return {};
}

status cascade_execution_worker::vqec_vision_ai_appl_cxwrk_request_stop(
    std::uint64_t _steady_now_ns) {
    std::lock_guard<std::mutex> lock(mutex_);
    is_stopping_ = true;
    if (stop_ns_ == 0) {
        stop_ns_ = _steady_now_ns;
    }
    metrics_.stop_duration_ns_ = _steady_now_ns >= stop_ns_ ? _steady_now_ns - stop_ns_ : 0;
    work_cv_.notify_all();
    return {};
}

status cascade_execution_worker::vqec_vision_ai_appl_cxwrk_drain_and_join(
    std::uint64_t _timeout_ns) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_running_) {
            return {};
        }
        is_exiting_ = true;
        is_stopping_ = true;
    }
    work_cv_.notify_all();

    std::unique_lock<std::mutex> lock(mutex_);
    const auto wait_limit = std::chrono::nanoseconds(_timeout_ns);
    const bool finished = idle_cv_.wait_for(lock, wait_limit, [this]() {
        return worker_finished_;
    });
    if (!finished) {
        recovery_required_ = true;
        return {status_code::timeout, "cascade execution worker join timed out; owner retained"};
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    is_running_ = false;
    return {};
}

bool cascade_execution_worker::vqec_vision_ai_appl_cxwrk_is_running() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return is_running_;
}

bool cascade_execution_worker::vqec_vision_ai_appl_cxwrk_is_stopping() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return is_stopping_;
}

cascade_coordinator_metrics
cascade_execution_worker::vqec_vision_ai_appl_cxwrk_get_metrics() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return metrics_;
}

}  // namespace vqec::vision::ai
