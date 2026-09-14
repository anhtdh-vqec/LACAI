#include "vqec_vision_inference_worker.hpp"

#include <new>
#include <utility>

namespace vqec::vision::ai {

inference_worker::~inference_worker() noexcept {
    vqec_vision_ai_sched_inwrk_request_stop();
    (void)vqec_vision_ai_sched_inwrk_drain();
}

status inference_worker::vqec_vision_ai_sched_inwrk_start(
    const inference_worker_config& _config, inference_work_executor& _executor) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_started_) {
        return {status_code::invalid_state, "inference worker is already started"};
    }
    if (_config.worker_count_ == 0 ||
        _config.worker_count_ > inference_worker_limits::g_max_workers ||
        _config.queue_capacity_ == 0 ||
        _config.queue_capacity_ > inference_worker_limits::g_max_queue ||
        _config.completion_capacity_ == 0 ||
        _config.completion_capacity_ > inference_worker_limits::g_max_completions) {
        return {status_code::invalid_argument, "invalid inference worker configuration"};
    }
    worker_count_ = _config.worker_count_;
    queue_capacity_ = _config.queue_capacity_;
    completion_capacity_ = _config.completion_capacity_;
    executor_ = &_executor;
    source_epochs_.assign(inference_worker_limits::g_max_sources, 0);
    is_started_ = true;
    is_stopping_ = false;
    try {
        workers_.reserve(worker_count_);
        for (std::uint16_t index = 0; index < worker_count_; ++index) {
            workers_.emplace_back(
                [this]() { (void)vqec_vision_ai_sched_inwrk_worker_main(); });
        }
    } catch (...) {
        is_stopping_ = true;
        work_available_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
        executor_ = nullptr;
        is_started_ = false;
        return {status_code::resource_exhausted, "cannot launch inference worker threads"};
    }
    return {};
}

status inference_worker::vqec_vision_ai_sched_inwrk_worker_main() noexcept {
    while (true) {
        inference_work_item item;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            work_available_.wait(lock, [this]() {
                return is_stopping_ || !pending_.empty();
            });
            if (pending_.empty()) {
                return {};  // stopping and drained
            }
            item = pending_.front();
            pending_.pop_front();
        }
        status executed;
        try {
            executed = executor_->vqec_vision_ai_sched_inwrk_execute(item);
        } catch (const std::bad_alloc&) {
            executed = {status_code::resource_exhausted, "executor allocation failed"};
        } catch (...) {
            executed = {status_code::io_error, "executor raised an exception"};
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (executed.code_ == status_code::ok) {
            ++completed_total_;
        } else {
            ++failed_total_;
        }
        inference_work_result result;
        result.item_ = item;
        result.result_ = std::move(executed);
        vqec_vision_ai_sched_inwrk_push_completion_locked(std::move(result));
    }
}

void inference_worker::vqec_vision_ai_sched_inwrk_push_completion_locked(
    inference_work_result _result) noexcept {
    if (completions_.size() >= completion_capacity_) {
        completions_.pop_front();
        ++dropped_total_;
    }
    completions_.push_back(std::move(_result));
}

void inference_worker::vqec_vision_ai_sched_inwrk_cancel_pending_locked() noexcept {
    while (!pending_.empty()) {
        inference_work_result result;
        result.item_ = pending_.front();
        pending_.pop_front();
        result.cancelled_ = true;
        result.result_ = {status_code::pending, "cancelled before execution"};
        vqec_vision_ai_sched_inwrk_push_completion_locked(std::move(result));
        ++cancelled_total_;
    }
}

status inference_worker::vqec_vision_ai_sched_inwrk_submit(
    const inference_work_item& _item, model_dispatch_policy _qos) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_started_) {
        return {status_code::invalid_state, "inference worker is not started"};
    }
    if (is_stopping_) {
        return {status_code::invalid_state, "inference worker is stopping"};
    }
    if (_item.job_id_ == 0 ||
        _item.source_slot_ >= inference_worker_limits::g_max_sources ||
        _item.model_slot_ >= inference_worker_limits::g_max_models ||
        _item.source_epoch_ == 0) {
        return {status_code::invalid_argument, "invalid inference work item identity"};
    }
    if (_qos == model_dispatch_policy::must_process_once ||
        _qos == model_dispatch_policy::event_triggered) {
        return {status_code::unsupported, "dispatch policy needs a durable queue"};
    }
    if (_qos == model_dispatch_policy::latest_wins ||
        _qos == model_dispatch_policy::replace_pending) {
        for (auto it = pending_.begin(); it != pending_.end(); ++it) {
            if (it->source_slot_ == _item.source_slot_ &&
                it->model_slot_ == _item.model_slot_) {
                inference_work_result superseded;
                superseded.item_ = *it;
                superseded.superseded_ = true;
                superseded.result_ = {status_code::pending, "superseded by a newer item"};
                pending_.erase(it);
                vqec_vision_ai_sched_inwrk_push_completion_locked(std::move(superseded));
                ++superseded_total_;
                break;
            }
        }
    }
    if (pending_.size() >= queue_capacity_) {
        ++rejected_total_;
        return {status_code::resource_exhausted, "inference worker queue is full"};
    }
    pending_.push_back(_item);
    ++submitted_total_;
    work_available_.notify_one();
    return {};
}

status inference_worker::vqec_vision_ai_sched_inwrk_poll(
    inference_work_result& _result) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (completions_.empty()) {
        return {status_code::pending, "no inference completion is ready"};
    }
    inference_work_result candidate = std::move(completions_.front());
    completions_.pop_front();
    const auto slot = candidate.item_.source_slot_;
    if (!candidate.cancelled_ && !candidate.superseded_ &&
        slot < source_epochs_.size() && source_epochs_[slot] != 0 &&
        candidate.item_.source_epoch_ != source_epochs_[slot]) {
        candidate.stale_epoch_ = true;
        ++stale_total_;
    }
    _result = std::move(candidate);
    return {};
}

status inference_worker::vqec_vision_ai_sched_inwrk_set_source_epoch(
    std::uint16_t _source_slot, std::uint64_t _source_epoch) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (_source_slot >= source_epochs_.size()) {
        return {status_code::invalid_argument, "source slot is outside the worker range"};
    }
    if (_source_epoch == 0 || _source_epoch < source_epochs_[_source_slot]) {
        return {status_code::invalid_argument, "source epoch must be nonzero and monotonic"};
    }
    source_epochs_[_source_slot] = _source_epoch;
    return {};
}

void inference_worker::vqec_vision_ai_sched_inwrk_request_stop() noexcept {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_started_ || is_stopping_) {
            return;
        }
        is_stopping_ = true;
        vqec_vision_ai_sched_inwrk_cancel_pending_locked();
    }
    work_available_.notify_all();
}

status inference_worker::vqec_vision_ai_sched_inwrk_drain() {
    std::vector<std::thread> joining;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (is_started_) {
            is_stopping_ = true;
        }
        joining.swap(workers_);
    }
    work_available_.notify_all();
    for (auto& worker : joining) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pending_.empty()) {
        vqec_vision_ai_sched_inwrk_cancel_pending_locked();
    }
    is_started_ = false;
    is_stopping_ = false;
    executor_ = nullptr;
    return {};
}

inference_worker_snapshot inference_worker::vqec_vision_ai_sched_inwrk_get_snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    inference_worker_snapshot snapshot;
    snapshot.worker_count_ = worker_count_;
    snapshot.queue_capacity_ = queue_capacity_;
    snapshot.queue_depth_ = pending_.size();
    snapshot.completion_depth_ = completions_.size();
    snapshot.submitted_total_ = submitted_total_;
    snapshot.completed_total_ = completed_total_;
    snapshot.failed_total_ = failed_total_;
    snapshot.rejected_total_ = rejected_total_;
    snapshot.superseded_total_ = superseded_total_;
    snapshot.cancelled_total_ = cancelled_total_;
    snapshot.dropped_total_ = dropped_total_;
    snapshot.stale_total_ = stale_total_;
    snapshot.is_started_ = is_started_;
    snapshot.is_stopping_ = is_stopping_;
    return snapshot;
}

}  // namespace vqec::vision::ai
