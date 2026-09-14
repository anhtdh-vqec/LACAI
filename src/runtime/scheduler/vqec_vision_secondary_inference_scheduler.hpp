#ifndef VQEC_VISION_AI_RUNTIME_SCHEDULER_SECONDARY_INFERENCE_SCHEDULER_HPP
#define VQEC_VISION_AI_RUNTIME_SCHEDULER_SECONDARY_INFERENCE_SCHEDULER_HPP

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_secondary_inference.hpp"

namespace vqec::vision::ai {

struct secondary_inference_snapshot {
    std::size_t queue_depth_{0};
    std::size_t queue_capacity_{0};
    std::uint64_t submitted_total_{0};
    std::uint64_t completed_total_{0};
    std::uint64_t failed_total_{0};
    std::uint64_t expired_total_{0};
    std::uint64_t rejected_total_{0};
    std::uint64_t cancelled_total_{0};
    std::uint64_t stale_total_{0};
};

// Bounded, serialized scheduler for cascade/ROI tasks. Feature code submits a neutral
// request instead of calling a backend directly; the scheduler maps source/model slots to
// the registered backend, correlates the source epoch and returns a neutral result. It owns
// no tensor and proves no model accuracy.
class secondary_inference_scheduler {
public:
    secondary_inference_scheduler() = default;
    secondary_inference_scheduler(const secondary_inference_scheduler& _other) = delete;
    secondary_inference_scheduler& operator=(const secondary_inference_scheduler& _other) =
        delete;

    [[nodiscard]] status vqec_vision_ai_sched_secsd_configure(
        std::size_t _capacity, std::uint16_t _source_count,
        secondary_inference_backend& _backend);
    [[nodiscard]] bool vqec_vision_ai_sched_secsd_is_configured() const noexcept;
    [[nodiscard]] status vqec_vision_ai_sched_secsd_submit(
        const secondary_inference_request& _request);
    // Returns one result: a cancelled task first, otherwise the next queued task executed
    // on the calling thread. No blocking backend call happens anywhere else.
    [[nodiscard]] status vqec_vision_ai_sched_secsd_step(
        std::uint64_t _now_ns, secondary_inference_result& _result);
    [[nodiscard]] status vqec_vision_ai_sched_secsd_set_source_epoch(
        std::uint16_t _source_slot, std::uint64_t _source_epoch);
    // Cancels queued tasks into cancelled results; the caller drains them via step().
    void vqec_vision_ai_sched_secsd_cancel_all() noexcept;
    [[nodiscard]] secondary_inference_snapshot
    vqec_vision_ai_sched_secsd_get_snapshot() const;

private:
    std::deque<secondary_inference_request> queue_;
    std::deque<secondary_inference_result> cancelled_;
    std::vector<std::uint64_t> source_epochs_;
    secondary_inference_backend* backend_{nullptr};
    std::size_t capacity_{0};
    std::uint16_t source_count_{0};
    std::uint64_t submitted_total_{0};
    std::uint64_t completed_total_{0};
    std::uint64_t failed_total_{0};
    std::uint64_t expired_total_{0};
    std::uint64_t rejected_total_{0};
    std::uint64_t cancelled_total_{0};
    std::uint64_t stale_total_{0};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_RUNTIME_SCHEDULER_SECONDARY_INFERENCE_SCHEDULER_HPP
