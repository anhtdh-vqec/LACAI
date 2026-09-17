#ifndef VQEC_VISION_AI_APPL_CASCADE_EXECUTION_WORKER_HPP
#define VQEC_VISION_AI_APPL_CASCADE_EXECUTION_WORKER_HPP

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "vqec_vision_cascade_coordinator.hpp"

namespace vqec::vision::ai {

namespace cascade_worker_limits {
inline constexpr std::size_t g_max_queue_depth = 16;
inline constexpr std::size_t g_max_completions = 16;
inline constexpr std::uint64_t g_default_join_timeout_ns = 500000000ULL;  // 500 ms
}  // namespace cascade_worker_limits

struct cascade_worker_task {
    std::uint64_t steady_now_ns_{0};
    preview_frame_key key_{};
    std::vector<observation> observations_{};
};

struct cascade_worker_completion {
    preview_frame_key key_{};
    std::vector<alignment_result> aligned_{};
    std::vector<embedding_result> embeddings_{};
    cascade_coordinator_report report_{};
    status task_status_{};
};

// Bounded asynchronous cascade execution worker. Runs alignment and secondary embedding
// inference on a dedicated thread without stalling the primary frame progress loop.
// Adheres strictly to Rule 5: when hardware times out or fails to complete, the frame
// lease is quarantined rather than released prematurely.
class cascade_execution_worker final {
public:
    cascade_execution_worker() = default;
    ~cascade_execution_worker() noexcept;
    cascade_execution_worker(const cascade_execution_worker&) = delete;
    cascade_execution_worker& operator=(const cascade_execution_worker&) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_cxwrk_configure(
        const cascade_coordinator_config& _config);
    [[nodiscard]] status vqec_vision_ai_appl_cxwrk_start();
    [[nodiscard]] status vqec_vision_ai_appl_cxwrk_schedule(
        std::uint64_t _steady_now_ns, const observation_batch& _batch);
    [[nodiscard]] status vqec_vision_ai_appl_cxwrk_poll_completion(
        cascade_worker_completion& _completion);
    [[nodiscard]] status vqec_vision_ai_appl_cxwrk_quiescent_reset(
        std::uint64_t _steady_now_ns, std::uint64_t _timeout_ns);
    [[nodiscard]] status vqec_vision_ai_appl_cxwrk_request_stop(
        std::uint64_t _steady_now_ns);
    [[nodiscard]] status vqec_vision_ai_appl_cxwrk_drain_and_join(
        std::uint64_t _timeout_ns);

    [[nodiscard]] bool vqec_vision_ai_appl_cxwrk_is_running() const noexcept;
    [[nodiscard]] bool vqec_vision_ai_appl_cxwrk_is_stopping() const noexcept;
    [[nodiscard]] cascade_coordinator_metrics
    vqec_vision_ai_appl_cxwrk_get_metrics() const noexcept;

private:
    void vqec_vision_ai_appl_cxwrk_worker_loop() noexcept;
    void vqec_vision_ai_appl_cxwrk_process_task(
        const cascade_worker_task& _task, cascade_worker_completion& _completion) noexcept;

    mutable std::mutex mutex_;
    std::condition_variable work_cv_;
    std::condition_variable idle_cv_;
    std::thread worker_;

    cascade_coordinator_config config_{};
    cascade_coordinator_metrics metrics_{};

    std::deque<cascade_worker_task> pending_queue_;
    std::deque<cascade_worker_completion> completion_queue_;

    std::vector<std::uint16_t> embedding_quantized_workspace_;
    std::vector<tensor_blob> embedding_inputs_;
    tensor_spec embedding_input_spec_{};
    bool has_embedding_input_spec_{false};
    std::uint64_t armed_source_epoch_{0};

    std::uint64_t stop_ns_{0};
    bool is_configured_{false};
    bool is_running_{false};
    bool is_stopping_{false};
    bool is_exiting_{false};
    bool has_inflight_{false};
    bool worker_finished_{false};
    bool recovery_required_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_CASCADE_EXECUTION_WORKER_HPP
