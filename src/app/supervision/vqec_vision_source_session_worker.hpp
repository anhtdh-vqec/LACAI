#ifndef VQEC_VISION_AI_APP_SOURCE_SESSION_WORKER_HPP
#define VQEC_VISION_AI_APP_SOURCE_SESSION_WORKER_HPP

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include "vqec_vision_source_session.hpp"

namespace vqec::vision::ai {

struct source_session_worker_snapshot {
    std::uint64_t steps_requested_{0};
    std::uint64_t steps_completed_{0};
    std::uint64_t rejected_{0};
    bool is_running_{false};
    bool has_pending_{false};
    bool has_inflight_{false};
    bool has_completion_{false};
};

// Runs one serialized source session on its own thread so a blocking backend call inside a
// session step never stalls the control/source scheduler. The control thread requests one
// step at a time (bounded one in flight, one unread completion) and polls results without
// blocking; the session itself still sees a single serialized caller. This realizes the
// "one serialized executor per source" isolation the supervisor documents.
class source_session_worker {
public:
    source_session_worker() = default;
    ~source_session_worker() noexcept;
    source_session_worker(const source_session_worker& _other) = delete;
    source_session_worker& operator=(const source_session_worker& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_sswrk_start(source_session_port& _session);
    // Non-blocking. resource_exhausted when a step is inflight or a completion is unread.
    [[nodiscard]] status vqec_vision_ai_appl_sswrk_request_step(
        std::uint64_t _steady_now_ns);
    // Non-blocking. Queues a session stop; a queued step may still run first.
    [[nodiscard]] status vqec_vision_ai_appl_sswrk_request_stop(
        std::uint64_t _steady_now_ns);
    [[nodiscard]] status vqec_vision_ai_appl_sswrk_poll_completion(
        status& _step_status, tensor_result& _result, source_session_progress& _progress);
    // Stops accepting, joins the worker and returns. Safe to call more than once.
    [[nodiscard]] status vqec_vision_ai_appl_sswrk_drain();
    [[nodiscard]] source_session_worker_snapshot
    vqec_vision_ai_appl_sswrk_get_snapshot() const;

private:
    enum class worker_command { none, step, stop };

    void vqec_vision_ai_appl_sswrk_worker_main() noexcept;

    source_session_port* session_{nullptr};
    std::thread worker_;
    mutable std::mutex mutex_;
    std::condition_variable work_available_;
    worker_command pending_{worker_command::none};
    std::uint64_t pending_now_ns_{0};
    status completion_status_;
    tensor_result completion_result_;
    source_session_progress completion_progress_;
    std::uint64_t steps_requested_{0};
    std::uint64_t steps_completed_{0};
    std::uint64_t rejected_{0};
    bool has_completion_{false};
    bool has_inflight_{false};
    bool is_running_{false};
    bool is_exiting_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_SOURCE_SESSION_WORKER_HPP
