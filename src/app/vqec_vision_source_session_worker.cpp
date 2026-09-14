#include "vqec_vision_source_session_worker.hpp"

#include <new>
#include <utility>

namespace vqec::vision::ai {

source_session_worker::~source_session_worker() noexcept {
    (void)vqec_vision_ai_appl_sswrk_drain();
}

status source_session_worker::vqec_vision_ai_appl_sswrk_start(
    source_session_port& _session) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_running_) {
        return {status_code::invalid_state, "source session worker is already running"};
    }
    session_ = &_session;
    is_running_ = true;
    is_exiting_ = false;
    try {
        worker_ = std::thread(
            [this]() { vqec_vision_ai_appl_sswrk_worker_main(); });
    } catch (...) {
        session_ = nullptr;
        is_running_ = false;
        return {status_code::resource_exhausted, "cannot launch source session worker"};
    }
    return {};
}

void source_session_worker::vqec_vision_ai_appl_sswrk_worker_main() noexcept {
    while (true) {
        worker_command command = worker_command::none;
        std::uint64_t now_ns = 0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            work_available_.wait(lock, [this]() {
                return is_exiting_ || pending_ != worker_command::none;
            });
            if (is_exiting_) {
                pending_ = worker_command::none;
                return;
            }
            command = pending_;
            now_ns = pending_now_ns_;
            pending_ = worker_command::none;
            has_inflight_ = true;
        }
        status step_status;
        tensor_result result;
        source_session_progress progress;
        try {
            if (command == worker_command::step) {
                step_status = session_->vqec_vision_ai_appl_srcsn_step(now_ns, result, progress);
            } else {
                step_status = session_->vqec_vision_ai_appl_srcsn_request_stop(now_ns);
            }
        } catch (const std::bad_alloc&) {
            step_status = {status_code::resource_exhausted, "session step allocation failed"};
        } catch (...) {
            step_status = {status_code::io_error, "session step raised an exception"};
        }
        std::lock_guard<std::mutex> lock(mutex_);
        completion_status_ = step_status;
        completion_result_ = std::move(result);
        completion_progress_ = progress;
        has_completion_ = true;
        has_inflight_ = false;
        if (command == worker_command::step) {
            ++steps_completed_;
        }
        work_available_.notify_all();
    }
}

status source_session_worker::vqec_vision_ai_appl_sswrk_request_step(
    std::uint64_t _steady_now_ns) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_running_) {
        return {status_code::invalid_state, "source session worker is not running"};
    }
    if (has_completion_) {
        ++rejected_;
        return {status_code::resource_exhausted, "poll the unread completion first"};
    }
    if (has_inflight_ || pending_ != worker_command::none) {
        ++rejected_;
        return {status_code::resource_exhausted, "a session step is already pending"};
    }
    pending_ = worker_command::step;
    pending_now_ns_ = _steady_now_ns;
    ++steps_requested_;
    work_available_.notify_all();
    return {};
}

status source_session_worker::vqec_vision_ai_appl_sswrk_request_stop(
    std::uint64_t _steady_now_ns) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_running_) {
        return {status_code::invalid_state, "source session worker is not running"};
    }
    pending_ = worker_command::stop;
    pending_now_ns_ = _steady_now_ns;
    work_available_.notify_all();
    return {};
}

status source_session_worker::vqec_vision_ai_appl_sswrk_poll_completion(
    status& _step_status, tensor_result& _result, source_session_progress& _progress) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!has_completion_) {
        return {status_code::pending, "no session completion is ready"};
    }
    _step_status = std::move(completion_status_);
    _result = std::move(completion_result_);
    _progress = completion_progress_;
    completion_status_ = {};
    completion_result_ = {};
    completion_progress_ = {};
    has_completion_ = false;
    return {};
}

status source_session_worker::vqec_vision_ai_appl_sswrk_drain() {
    std::thread joining;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_running_) {
            return {};
        }
        is_exiting_ = true;
        pending_ = worker_command::none;
    }
    work_available_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    session_ = nullptr;
    is_running_ = false;
    has_completion_ = false;
    has_inflight_ = false;
    completion_status_ = {};
    completion_result_ = {};
    completion_progress_ = {};
    return {};
}

source_session_worker_snapshot
source_session_worker::vqec_vision_ai_appl_sswrk_get_snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    source_session_worker_snapshot snapshot;
    snapshot.steps_requested_ = steps_requested_;
    snapshot.steps_completed_ = steps_completed_;
    snapshot.rejected_ = rejected_;
    snapshot.is_running_ = is_running_;
    snapshot.has_inflight_ = has_inflight_;
    snapshot.has_completion_ = has_completion_;
    return snapshot;
}

}  // namespace vqec::vision::ai
