// Device-free tests for the bounded inference worker: control thread never runs the
// backend, queue/backpressure is bounded, latest_wins supersedes, stop cancels pending and
// completions from an older source epoch are flagged stale.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "vqec_vision_inference_worker.hpp"

using namespace vqec::vision::ai;

namespace {

class gate_executor final : public inference_work_executor {
public:
    void open() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            is_open_ = true;
        }
        condition_.notify_all();
    }
    void set_fail_source(std::uint16_t _slot) { fail_source_ = _slot; }

    [[nodiscard]] status vqec_vision_ai_sched_inwrk_execute(
        const inference_work_item& _item) override {
        {
            std::lock_guard<std::mutex> start_lock(start_mutex_);
            ++entered_;
            start_condition_.notify_all();
        }
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this]() { return is_open_.load(); });
        ++calls_;
        last_job_id_ = _item.job_id_;
        if (_item.source_slot_ == fail_source_) {
            return {status_code::io_error, "injected executor failure"};
        }
        return {};
    }

    // Blocks until at least _count worker calls have entered the executor. Deterministic
    // replacement for a sleep when a test needs an item popped off the queue.
    bool wait_started(unsigned _count) {
        std::unique_lock<std::mutex> start_lock(start_mutex_);
        return start_condition_.wait_for(start_lock, std::chrono::seconds(3),
            [this, _count]() { return entered_.load() >= _count; });
    }

    unsigned calls() const { return calls_.load(); }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::mutex start_mutex_;
    std::condition_variable start_condition_;
    std::atomic<bool> is_open_{false};
    std::atomic<unsigned> calls_{0};
    std::atomic<unsigned> entered_{0};
    std::uint64_t last_job_id_{0};
    std::uint16_t fail_source_{UINT16_MAX};
};

inference_work_item make_item(std::uint64_t _job, std::uint16_t _source,
    std::uint16_t _model, std::uint64_t _epoch) {
    inference_work_item item;
    item.job_id_ = _job;
    item.source_slot_ = _source;
    item.model_slot_ = _model;
    item.source_epoch_ = _epoch;
    item.source_frame_id_ = _job;
    return item;
}

bool poll_until(inference_worker& _worker, std::size_t _count,
    std::vector<inference_work_result>& _out) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (_out.size() < _count && std::chrono::steady_clock::now() < deadline) {
        inference_work_result result;
        if (_worker.vqec_vision_ai_sched_inwrk_poll(result).code_ == status_code::ok) {
            _out.push_back(std::move(result));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    return _out.size() == _count;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    // Configuration and identity validation.
    {
        gate_executor executor;
        inference_worker worker;
        inference_worker_config bad;
        bad.worker_count_ = 0;
        check(worker.vqec_vision_ai_sched_inwrk_start(bad, executor).code_ ==
              status_code::invalid_argument);
        inference_worker_config config;
        config.worker_count_ = 1;
        config.queue_capacity_ = 4;
        check(worker.vqec_vision_ai_sched_inwrk_start(config, executor).code_ ==
              status_code::ok);
        check(worker.vqec_vision_ai_sched_inwrk_start(config, executor).code_ ==
              status_code::invalid_state);
        inference_work_item invalid;
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  invalid, model_dispatch_policy::drop_if_busy).code_ ==
              status_code::invalid_argument);
        executor.open();
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(1, 0, 0, 1), model_dispatch_policy::drop_if_busy).code_ ==
              status_code::ok);
        std::vector<inference_work_result> results;
        check(poll_until(worker, 1, results));
        check(results.size() == 1 && results[0].result_.code_ == status_code::ok &&
              !results[0].stale_epoch_ && !results[0].cancelled_);
        check(worker.vqec_vision_ai_sched_inwrk_drain().code_ == status_code::ok);
        const auto snapshot = worker.vqec_vision_ai_sched_inwrk_get_snapshot();
        check(!snapshot.is_started_ && snapshot.queue_depth_ == 0 &&
              snapshot.submitted_total_ == 1 && snapshot.completed_total_ == 1);
    }

    // Queue full under drop_if_busy is rejected, not blocked and not silently dropped.
    {
        gate_executor executor;
        inference_worker worker;
        inference_worker_config config;
        config.worker_count_ = 1;
        config.queue_capacity_ = 1;
        check(worker.vqec_vision_ai_sched_inwrk_start(config, executor).code_ ==
              status_code::ok);
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(1, 0, 0, 1), model_dispatch_policy::drop_if_busy).code_ ==
              status_code::ok);
        // Wait until item 1 left the queue and is blocked in the executor, so item 2 is
        // guaranteed to be the one that fills the queue.
        check(executor.wait_started(1));
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(2, 0, 0, 1), model_dispatch_policy::drop_if_busy).code_ ==
              status_code::ok);
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(3, 0, 0, 1), model_dispatch_policy::drop_if_busy).code_ ==
              status_code::resource_exhausted);
        const auto snapshot = worker.vqec_vision_ai_sched_inwrk_get_snapshot();
        check(snapshot.rejected_total_ == 1);
        executor.open();
        std::vector<inference_work_result> results;
        check(poll_until(worker, 2, results));
        check(worker.vqec_vision_ai_sched_inwrk_drain().code_ == status_code::ok);
    }

    // latest_wins supersedes a queued item for the same source/model slot.
    {
        gate_executor executor;
        inference_worker worker;
        inference_worker_config config;
        config.worker_count_ = 1;
        config.queue_capacity_ = 2;
        check(worker.vqec_vision_ai_sched_inwrk_start(config, executor).code_ ==
              status_code::ok);
        // Different slot occupies the single worker and blocks on the gate, so the
        // same-slot item stays queued and can be superseded deterministically.
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(10, 0, 1, 1), model_dispatch_policy::drop_if_busy).code_ ==
              status_code::ok);
        check(executor.wait_started(1));
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(11, 0, 0, 1), model_dispatch_policy::latest_wins).code_ ==
              status_code::ok);
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(12, 0, 0, 1), model_dispatch_policy::latest_wins).code_ ==
              status_code::ok);
        executor.open();
        std::vector<inference_work_result> results;
        check(poll_until(worker, 3, results));
        check(worker.vqec_vision_ai_sched_inwrk_drain().code_ == status_code::ok);
        unsigned ok = 0;
        unsigned superseded = 0;
        bool saw_eleven = false;
        bool saw_twelve = false;
        for (const auto& result : results) {
            if (result.superseded_) {
                ++superseded;
                saw_eleven = saw_eleven || result.item_.job_id_ == 11;
            } else if (result.result_.code_ == status_code::ok) {
                ++ok;
                saw_twelve = saw_twelve || result.item_.job_id_ == 12;
            }
        }
        check(superseded == 1 && saw_eleven && ok == 2 && saw_twelve);
    }

    // An older source epoch is flagged stale on poll.
    {
        gate_executor executor;
        executor.open();
        inference_worker worker;
        inference_worker_config config;
        config.worker_count_ = 1;
        check(worker.vqec_vision_ai_sched_inwrk_start(config, executor).code_ ==
              status_code::ok);
        check(worker.vqec_vision_ai_sched_inwrk_set_source_epoch(0, 5).code_ ==
              status_code::ok);
        check(worker.vqec_vision_ai_sched_inwrk_set_source_epoch(0, 6).code_ ==
              status_code::ok);
        check(worker.vqec_vision_ai_sched_inwrk_set_source_epoch(0, 4).code_ ==
              status_code::invalid_argument);
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(20, 0, 0, 5), model_dispatch_policy::drop_if_busy).code_ ==
              status_code::ok);
        std::vector<inference_work_result> results;
        check(poll_until(worker, 1, results));
        check(results.size() == 1 && results[0].stale_epoch_);
        check(worker.vqec_vision_ai_sched_inwrk_get_snapshot().stale_total_ == 1);
        check(worker.vqec_vision_ai_sched_inwrk_drain().code_ == status_code::ok);
    }

    // Executor failure is reported per completion, not as a worker death.
    {
        gate_executor executor;
        executor.open();
        executor.set_fail_source(2);
        inference_worker worker;
        inference_worker_config config;
        config.worker_count_ = 1;
        check(worker.vqec_vision_ai_sched_inwrk_start(config, executor).code_ ==
              status_code::ok);
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(30, 2, 0, 1), model_dispatch_policy::drop_if_busy).code_ ==
              status_code::ok);
        std::vector<inference_work_result> results;
        check(poll_until(worker, 1, results));
        check(results.size() == 1 && results[0].result_.code_ == status_code::io_error);
        check(worker.vqec_vision_ai_sched_inwrk_get_snapshot().failed_total_ == 1);
        check(worker.vqec_vision_ai_sched_inwrk_drain().code_ == status_code::ok);
    }

    // A slow backend does not block the control thread, and a healthy source still
    // completes while another source's executor call is in flight.
    {
        gate_executor executor;
        inference_worker worker;
        inference_worker_config config;
        config.worker_count_ = 2;
        config.queue_capacity_ = 4;
        check(worker.vqec_vision_ai_sched_inwrk_start(config, executor).code_ ==
              status_code::ok);
        const auto started = std::chrono::steady_clock::now();
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(40, 0, 0, 1), model_dispatch_policy::drop_if_busy).code_ ==
              status_code::ok);
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(41, 1, 0, 1), model_dispatch_policy::drop_if_busy).code_ ==
              status_code::ok);
        const auto submit_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        // Executor is still gated; control returned without waiting for inference.
        check(submit_elapsed < 100 && executor.calls() == 0);
        executor.open();
        std::vector<inference_work_result> results;
        check(poll_until(worker, 2, results));
        check(worker.vqec_vision_ai_sched_inwrk_drain().code_ == status_code::ok);
    }

    // stop cancels queued work and drain joins workers; a completed in-flight job survives.
    {
        gate_executor executor;
        inference_worker worker;
        inference_worker_config config;
        config.worker_count_ = 1;
        config.queue_capacity_ = 4;
        check(worker.vqec_vision_ai_sched_inwrk_start(config, executor).code_ ==
              status_code::ok);
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(50, 0, 0, 1), model_dispatch_policy::drop_if_busy).code_ ==
              status_code::ok);
        // Wait until job 50 has left the queue and is blocked in the executor, so the next
        // item deterministically stays pending and is the one cancelled by stop.
        check(executor.wait_started(1));
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(51, 0, 1, 1), model_dispatch_policy::drop_if_busy).code_ ==
              status_code::ok);
        worker.vqec_vision_ai_sched_inwrk_request_stop();
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(52, 0, 1, 1), model_dispatch_policy::drop_if_busy).code_ ==
              status_code::invalid_state);
        executor.open();
        check(worker.vqec_vision_ai_sched_inwrk_drain().code_ == status_code::ok);
        std::vector<inference_work_result> results;
        check(poll_until(worker, 2, results));
        bool cancelled = false;
        bool completed = false;
        for (const auto& result : results) {
            cancelled = cancelled || (result.item_.job_id_ == 51 && result.cancelled_);
            completed = completed || (result.item_.job_id_ == 50 && !result.cancelled_);
        }
        check(cancelled && completed);
        const auto snapshot = worker.vqec_vision_ai_sched_inwrk_get_snapshot();
        check(!snapshot.is_started_ && snapshot.queue_depth_ == 0);
    }

    std::cout << "inference worker failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
