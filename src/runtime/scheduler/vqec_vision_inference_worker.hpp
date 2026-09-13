#ifndef VQEC_VISION_AI_RUNTIME_SCHEDULER_INFERENCE_WORKER_HPP
#define VQEC_VISION_AI_RUNTIME_SCHEDULER_INFERENCE_WORKER_HPP

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "vqec_vision_model_cadence.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace inference_worker_limits {
inline constexpr std::uint16_t g_max_workers = 8;
inline constexpr std::uint16_t g_max_sources = 16;
inline constexpr std::uint16_t g_max_models = 16;
inline constexpr std::size_t g_max_queue = 64;
// A completion buffer deeper than the queue lets short bursts drain without dropping.
inline constexpr std::size_t g_max_completions = 128;
}  // namespace inference_worker_limits

// One unit of backend work handed to a worker. Vendor-neutral: the worker never sees a
// tensor or vendor type, only identity, ordering and deadline.
struct inference_work_item {
    std::uint64_t job_id_{0};
    std::uint16_t source_slot_{0};
    std::uint16_t model_slot_{0};
    std::uint64_t source_epoch_{0};
    std::uint64_t source_frame_id_{0};
    std::uint64_t enqueued_ns_{0};
    // 0 means "no deadline"; otherwise an absolute monotonic deadline shared with the caller.
    std::uint64_t deadline_ns_{0};
    std::uint16_t priority_{0};
};

struct inference_work_result {
    inference_work_item item_;
    status result_;
    bool cancelled_{false};
    bool superseded_{false};
    // True when the source epoch changed before the consumer observed this completion.
    bool stale_epoch_{false};
};

struct inference_worker_config {
    std::uint16_t worker_count_{1};
    std::size_t queue_capacity_{inference_worker_limits::g_max_queue};
    std::size_t completion_capacity_{inference_worker_limits::g_max_completions};
};

// Executes one item. Called on a worker thread; with several workers it must be safe for
// concurrent calls. The backend owns any blocking work; the control thread never calls it.
class inference_work_executor {
public:
    virtual ~inference_work_executor() = default;
    [[nodiscard]] virtual status vqec_vision_ai_sched_inwrk_execute(
        const inference_work_item& _item) = 0;
};

struct inference_worker_snapshot {
    std::uint16_t worker_count_{0};
    std::size_t queue_capacity_{0};
    std::size_t queue_depth_{0};
    std::size_t completion_depth_{0};
    std::uint64_t submitted_total_{0};
    std::uint64_t completed_total_{0};
    std::uint64_t failed_total_{0};
    std::uint64_t rejected_total_{0};
    std::uint64_t superseded_total_{0};
    std::uint64_t dropped_total_{0};
    std::uint64_t stale_total_{0};
    bool is_started_{false};
    bool is_stopping_{false};
};

// Bounded worker pool that moves a blocking backend call off the control/source executor.
// submit() returns without running the backend; poll() returns completions non-blocking.
// Lifecycle: start once, request_stop() to stop accepting and cancel pending, drain() to
// join workers. The destructor calls both and never throws.
class inference_worker {
public:
    inference_worker() = default;
    ~inference_worker() noexcept;
    inference_worker(const inference_worker& _other) = delete;
    inference_worker& operator=(const inference_worker& _other) = delete;
    inference_worker(inference_worker&& _other) = delete;
    inference_worker& operator=(inference_worker&& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_sched_inwrk_start(
        const inference_worker_config& _config, inference_work_executor& _executor);
    // Non-blocking enqueue. drop_if_busy rejects when the queue is full; latest_wins
    // replaces a pending item from the same source/model slot. Other policies are rejected.
    [[nodiscard]] status vqec_vision_ai_sched_inwrk_submit(
        const inference_work_item& _item, model_dispatch_policy _qos);
    [[nodiscard]] status vqec_vision_ai_sched_inwrk_poll(
        inference_work_result& _result);
    // Records the current epoch of a source slot; newer epochs flag older completions stale.
    [[nodiscard]] status vqec_vision_ai_sched_inwrk_set_source_epoch(
        std::uint16_t _source_slot, std::uint64_t _source_epoch);
    void vqec_vision_ai_sched_inwrk_request_stop() noexcept;
    // Joins all workers. Safe to call without start and more than once.
    [[nodiscard]] status vqec_vision_ai_sched_inwrk_drain();
    [[nodiscard]] inference_worker_snapshot
    vqec_vision_ai_sched_inwrk_get_snapshot() const;

private:
    [[nodiscard]] status vqec_vision_ai_sched_inwrk_worker_main() noexcept;
    // Caller must hold mutex_.
    void vqec_vision_ai_sched_inwrk_push_completion_locked(
        inference_work_result _result) noexcept;
    // Caller must hold mutex_.
    void vqec_vision_ai_sched_inwrk_cancel_pending_locked() noexcept;

    inference_work_executor* executor_{nullptr};
    std::vector<std::thread> workers_;
    mutable std::mutex mutex_;
    std::condition_variable work_available_;
    std::deque<inference_work_item> pending_;
    std::deque<inference_work_result> completions_;
    std::vector<std::uint64_t> source_epochs_;
    std::size_t queue_capacity_{0};
    std::size_t completion_capacity_{0};
    std::uint16_t worker_count_{0};
    std::uint64_t submitted_total_{0};
    std::uint64_t completed_total_{0};
    std::uint64_t failed_total_{0};
    std::uint64_t rejected_total_{0};
    std::uint64_t superseded_total_{0};
    std::uint64_t dropped_total_{0};
    std::uint64_t stale_total_{0};
    bool is_started_{false};
    bool is_stopping_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_RUNTIME_SCHEDULER_INFERENCE_WORKER_HPP
