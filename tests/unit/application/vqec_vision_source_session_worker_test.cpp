// Device-free tests for the per-source session worker: the control thread stays
// non-blocking while a session step is stuck in a backend, one step/completion is bounded,
// stop and drain are ordered, and a session exception becomes a completion.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>

#include "vqec_vision_source_session_worker.hpp"

using namespace vqec::vision::ai;

namespace {

class gated_session final : public source_session_port {
public:
    [[nodiscard]] status vqec_vision_ai_appl_srcsn_step(
        std::uint64_t _now_ns, tensor_result& _result,
        source_session_progress& _progress) override {
        {
            std::lock_guard<std::mutex> start_lock(start_mutex_);
            ++entered_;
            start_condition_.notify_all();
        }
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this]() { return is_open_.load(); });
        health_.phase_ = source_session_phase::running;
        ++steps_;
        _progress = {};
        _progress.has_result_ = true;
        _progress.model_slot_ = 0;
        _result.pipeline_pts_ns_ = _now_ns;
        if (fail_) {
            return {status_code::io_error, "injected session failure"};
        }
        return {status_code::pending, "no session work"};
    }
    [[nodiscard]] status vqec_vision_ai_appl_srcsn_request_stop(
        std::uint64_t) override {
        ++stop_calls_;
        health_.phase_ = source_session_phase::stopped;
        return {};
    }
    [[nodiscard]] source_session_health
    vqec_vision_ai_appl_srcsn_get_health() const noexcept override {
        return health_;
    }

    void open() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            is_open_ = true;
        }
        condition_.notify_all();
    }
    bool wait_started(unsigned _count) {
        std::unique_lock<std::mutex> start_lock(start_mutex_);
        return start_condition_.wait_for(start_lock, std::chrono::seconds(3),
            [this, _count]() { return entered_.load() >= _count; });
    }

    bool fail_{false};
    unsigned steps_{0};
    unsigned stop_calls_{0};

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::mutex start_mutex_;
    std::condition_variable start_condition_;
    std::atomic<bool> is_open_{false};
    std::atomic<unsigned> entered_{0};
    source_session_health health_;
};

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    // Control thread never blocks on the session step.
    {
        gated_session session;
        source_session_worker worker;
        check(worker.vqec_vision_ai_appl_sswrk_request_step(1).code_ ==
              status_code::invalid_state);
        check(worker.vqec_vision_ai_appl_sswrk_start(session).code_ == status_code::ok);

        const auto started = std::chrono::steady_clock::now();
        check(worker.vqec_vision_ai_appl_sswrk_request_step(1000).code_ == status_code::ok);
        const auto queued = worker.vqec_vision_ai_appl_sswrk_get_snapshot();
        check(queued.has_pending_ || queued.has_inflight_);
        check(session.wait_started(1));
        check(worker.vqec_vision_ai_appl_sswrk_request_step(2000).code_ ==
              status_code::resource_exhausted);
        const auto request_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        check(request_elapsed < 100);

        status step_status;
        tensor_result result;
        source_session_progress progress;
        source_session_health health;
        check(worker.vqec_vision_ai_appl_sswrk_poll_completion(
                  step_status, result, progress, health).code_ == status_code::pending);

        session.open();
        // Wait for the completion to appear.
        bool completed = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!completed && std::chrono::steady_clock::now() < deadline) {
            if (worker.vqec_vision_ai_appl_sswrk_poll_completion(
                    step_status, result, progress, health).code_ == status_code::ok) {
                completed = true;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        check(completed && step_status.code_ == status_code::pending &&
              health.phase_ == source_session_phase::running &&
              progress.has_result_ && result.pipeline_pts_ns_ == 1000);
        const auto quiescent = worker.vqec_vision_ai_appl_sswrk_get_snapshot();
        check(!quiescent.has_pending_ && !quiescent.has_inflight_ &&
              !quiescent.has_completion_);

        check(worker.vqec_vision_ai_appl_sswrk_request_stop(3000).code_ == status_code::ok);
        completed = false;
        while (!completed && std::chrono::steady_clock::now() < deadline) {
            if (worker.vqec_vision_ai_appl_sswrk_poll_completion(
                    step_status, result, progress, health).code_ == status_code::ok) {
                completed = true;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        check(completed && session.stop_calls_ == 1 &&
              health.phase_ == source_session_phase::stopped);
        check(worker.vqec_vision_ai_appl_sswrk_drain().code_ == status_code::ok);
        const auto snapshot = worker.vqec_vision_ai_appl_sswrk_get_snapshot();
        check(!snapshot.is_running_ && snapshot.steps_requested_ == 1 &&
              snapshot.steps_completed_ == 1 && snapshot.rejected_ == 1);
    }

    // A session exception becomes an io_error completion, not a worker death.
    {
        gated_session session;
        session.open();
        session.fail_ = true;
        source_session_worker worker;
        check(worker.vqec_vision_ai_appl_sswrk_start(session).code_ == status_code::ok);
        check(worker.vqec_vision_ai_appl_sswrk_request_step(1000).code_ == status_code::ok);
        status step_status;
        tensor_result result;
        source_session_progress progress;
        source_session_health health;
        bool completed = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!completed && std::chrono::steady_clock::now() < deadline) {
            if (worker.vqec_vision_ai_appl_sswrk_poll_completion(
                    step_status, result, progress, health).code_ == status_code::ok) {
                completed = true;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        check(completed && step_status.code_ == status_code::io_error);
        check(worker.vqec_vision_ai_appl_sswrk_drain().code_ == status_code::ok);
    }

    std::cout << "source session worker failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
