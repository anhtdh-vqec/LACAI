// Device-free invariants and repeated-cycle resource stability:
//   inference: submitted == completed + failed + superseded + cancelled
//   tensor pool: capacity == free + live, acquire == release + live
// and a start/submit/poll/drain loop that must return to an idle, empty state each cycle.

#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>

#include "vqec_vision_inference_worker.hpp"
#include "vqec_vision_tensor_pool.hpp"

using namespace vqec::vision::ai;

namespace {

class immediate_executor final : public inference_work_executor {
public:
    [[nodiscard]] status vqec_vision_ai_sched_inwrk_execute(
        const inference_work_item& _item) override {
        (void)_item;
        ++calls_;
        return {};
    }
    std::atomic<std::uint64_t> calls_{0};
};

inference_work_item make_item(std::uint64_t _job) {
    inference_work_item item;
    item.job_id_ = _job;
    item.source_slot_ = 0;
    item.model_slot_ = 0;
    item.source_epoch_ = 1;
    item.source_frame_id_ = _job;
    return item;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    // Worker: every submitted job yields exactly one completion outcome, and each
    // start/submit/poll/drain cycle returns to an empty idle worker.
    {
        immediate_executor executor;
        inference_worker worker;
        inference_worker_config config;
        config.worker_count_ = 2;
        config.queue_capacity_ = 8;
        constexpr std::uint64_t jobs_per_cycle = 8;
        constexpr unsigned cycles = 200;
        std::uint64_t next_job = 1;
        for (unsigned cycle = 0; cycle < cycles; ++cycle) {
            check(worker.vqec_vision_ai_sched_inwrk_start(config, executor).code_ ==
                  status_code::ok);
            for (std::uint64_t index = 0; index < jobs_per_cycle; ++index) {
                check(worker.vqec_vision_ai_sched_inwrk_submit(
                          make_item(next_job++), model_dispatch_policy::drop_if_busy).code_ ==
                      status_code::ok);
            }
            std::uint64_t polled = 0;
            for (unsigned spin = 0; spin < 100000 && polled < jobs_per_cycle; ++spin) {
                inference_work_result result;
                if (worker.vqec_vision_ai_sched_inwrk_poll(result).code_ == status_code::ok) {
                    ++polled;
                } else {
                    std::this_thread::yield();
                }
            }
            check(polled == jobs_per_cycle);
            check(worker.vqec_vision_ai_sched_inwrk_drain().code_ == status_code::ok);
            const auto snapshot = worker.vqec_vision_ai_sched_inwrk_get_snapshot();
            check(!snapshot.is_started_ && snapshot.queue_depth_ == 0 &&
                  snapshot.completion_depth_ == 0);
            check(snapshot.submitted_total_ ==
                  snapshot.completed_total_ + snapshot.failed_total_ +
                      snapshot.superseded_total_ + snapshot.cancelled_total_);
        }
        check(executor.calls_.load() == jobs_per_cycle * cycles);
    }

    // Tensor pool: allocation is conserved across many acquire/release cycles.
    {
        tensor_pool pool;
        tensor_pool_config config;
        config.capacity_ = 4;
        config.spec_.name_ = "out";
        config.spec_.dimensions_ = {1, 4, 4, 3};
        config.spec_.dtype_ = tensor_element_type::float32;
        check(pool.vqec_vision_ai_core_tnpl_configure(config).code_ == status_code::ok);
        bool ok = true;
        for (unsigned cycle = 0; cycle < 5000; ++cycle) {
            std::size_t slot = 0;
            tensor_blob* blob = nullptr;
            ok = ok && pool.vqec_vision_ai_core_tnpl_acquire(slot, blob).code_ ==
                status_code::ok;
            ok = ok && pool.vqec_vision_ai_core_tnpl_release(slot).code_ == status_code::ok;
        }
        check(ok);
        const auto stats = pool.vqec_vision_ai_core_tnpl_get_stats();
        check(stats.capacity_ == stats.free_ + stats.live_ && stats.live_ == 0);
        check(stats.acquire_total_ == stats.release_total_ + stats.live_ &&
              stats.double_release_total_ == 0 && stats.exhausted_total_ == 0 &&
              stats.high_watermark_ == 1);
    }

    std::cout << "lifecycle invariant failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
