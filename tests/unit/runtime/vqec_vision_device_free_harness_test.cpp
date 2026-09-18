// Device-free stress/benchmark harness:
//   - latency and failure injection into the inference worker (section 28),
//   - control thread stays non-blocking and a fast source is not starved by a slow one,
//   - steady-state allocation instrumentation around the tensor pool (section 26),
//   - a coarse throughput print for regression eyes (section 29).

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>

#include "vqec_vision_inference_worker.hpp"
#include "vqec_vision_tensor_pool.hpp"

// Global allocation counters for the instrumentation below.
namespace {
std::atomic<std::uint64_t> g_allocations{0};
std::atomic<std::uint64_t> g_bytes{0};
}  // namespace

void* operator new(std::size_t _size) {
    ++g_allocations;
    g_bytes.fetch_add(_size, std::memory_order_relaxed);
    void* pointer = std::malloc(_size != 0 ? _size : 1U);
    if (pointer == nullptr) {
        throw std::bad_alloc();
    }
    return pointer;
}
void* operator new[](std::size_t _size) { return ::operator new(_size); }
void operator delete(void* _pointer) noexcept { std::free(_pointer); }
void operator delete[](void* _pointer) noexcept { std::free(_pointer); }
void operator delete(void* _pointer, std::size_t) noexcept { std::free(_pointer); }
void operator delete[](void* _pointer, std::size_t) noexcept { std::free(_pointer); }

using namespace vqec::vision::ai;

namespace {

class latency_executor final : public inference_work_executor {
public:
    latency_executor(std::uint64_t _slow_ns, std::uint64_t _fast_ns, std::uint32_t _fail_every)
        : slow_ns_(_slow_ns), fast_ns_(_fast_ns), fail_every_(_fail_every) {}

    [[nodiscard]] status vqec_vision_ai_sched_inwrk_execute(
        const inference_work_item& _item) override {
        const auto delay = _item.source_slot_ == 0 ? slow_ns_ : fast_ns_;
        if (delay != 0) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(delay));
        }
        const auto call = ++calls_;
        if (fail_every_ != 0 && call % fail_every_ == 0) {
            return {status_code::io_error, "injected backend failure"};
        }
        return {};
    }

    std::uint64_t slow_ns_{0};
    std::uint64_t fast_ns_{0};
    std::uint32_t fail_every_{0};
    std::atomic<std::uint64_t> calls_{0};
};

inference_work_item make_item(std::uint64_t _job, std::uint16_t _source) {
    inference_work_item item;
    item.job_id_ = _job;
    item.source_slot_ = _source;
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

    // Non-blocking submit under injected latency, then all completions arrive.
    {
        latency_executor executor(20'000'000, 1'000'000, 0);
        inference_worker worker;
        inference_worker_config config;
        config.worker_count_ = 1;
        config.queue_capacity_ = 4;
        check(worker.vqec_vision_ai_sched_inwrk_start(config, executor).code_ ==
              status_code::ok);
        const auto started = std::chrono::steady_clock::now();
        for (std::uint64_t job = 1; job <= 4; ++job) {
            check(worker.vqec_vision_ai_sched_inwrk_submit(
                      make_item(job, 0), model_dispatch_policy::drop_if_busy).code_ ==
                  status_code::ok);
        }
        const auto submit_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        check(submit_elapsed < 40);
        std::uint64_t polled = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (polled < 4 && std::chrono::steady_clock::now() < deadline) {
            inference_work_result result;
            if (worker.vqec_vision_ai_sched_inwrk_poll(result).code_ == status_code::ok) {
                ++polled;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        check(polled == 4);
        check(worker.vqec_vision_ai_sched_inwrk_drain().code_ == status_code::ok);
    }

    // A slow source does not starve a fast source with two workers.
    {
        latency_executor executor(40'000'000, 1'000'000, 0);
        inference_worker worker;
        inference_worker_config config;
        config.worker_count_ = 2;
        config.queue_capacity_ = 4;
        check(worker.vqec_vision_ai_sched_inwrk_start(config, executor).code_ ==
              status_code::ok);
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(1, 0), model_dispatch_policy::drop_if_busy).code_ == status_code::ok);
        check(worker.vqec_vision_ai_sched_inwrk_submit(
                  make_item(2, 1), model_dispatch_policy::drop_if_busy).code_ == status_code::ok);
        inference_work_result first;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        bool got_first = false;
        while (!got_first && std::chrono::steady_clock::now() < deadline) {
            if (worker.vqec_vision_ai_sched_inwrk_poll(first).code_ == status_code::ok) {
                got_first = true;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        check(got_first && first.item_.source_slot_ == 1);
        check(worker.vqec_vision_ai_sched_inwrk_drain().code_ == status_code::ok);
    }

    // Failure injection is reported per completion, not as worker death.
    {
        latency_executor executor(0, 0, 2);
        inference_worker worker;
        inference_worker_config config;
        config.worker_count_ = 1;
        config.queue_capacity_ = 8;
        check(worker.vqec_vision_ai_sched_inwrk_start(config, executor).code_ ==
              status_code::ok);
        for (std::uint64_t job = 1; job <= 4; ++job) {
            check(worker.vqec_vision_ai_sched_inwrk_submit(
                      make_item(job, 1), model_dispatch_policy::drop_if_busy).code_ ==
                  status_code::ok);
        }
        std::uint64_t polled = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (polled < 4 && std::chrono::steady_clock::now() < deadline) {
            inference_work_result result;
            if (worker.vqec_vision_ai_sched_inwrk_poll(result).code_ == status_code::ok) {
                ++polled;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        const auto snapshot = worker.vqec_vision_ai_sched_inwrk_get_snapshot();
        check(polled == 4 && snapshot.failed_total_ == 2 && snapshot.completed_total_ == 2);
        check(worker.vqec_vision_ai_sched_inwrk_drain().code_ == status_code::ok);
    }

    // Allocation instrumentation: the tensor pool steady state allocates nothing.
    {
        tensor_pool pool;
        tensor_pool_config config;
        config.capacity_ = 4;
        config.spec_.name_ = "out";
        config.spec_.dimensions_ = {1, 4, 4, 3};
        config.spec_.dtype_ = tensor_element_type::float32;
        check(pool.vqec_vision_ai_core_tnpl_configure(config).code_ == status_code::ok);
        for (unsigned cycle = 0; cycle < 100; ++cycle) {
            std::size_t slot = 0;
            tensor_blob* blob = nullptr;
            (void)pool.vqec_vision_ai_core_tnpl_acquire(slot, blob);
            (void)pool.vqec_vision_ai_core_tnpl_release(slot);
        }
        const auto before = g_allocations.load();
        std::uint64_t acquire_release_ops = 0;
        const auto bench_start = std::chrono::steady_clock::now();
        for (unsigned cycle = 0; cycle < 100000; ++cycle) {
            std::size_t slot = 0;
            tensor_blob* blob = nullptr;
            if (pool.vqec_vision_ai_core_tnpl_acquire(slot, blob).code_ == status_code::ok &&
                pool.vqec_vision_ai_core_tnpl_release(slot).code_ == status_code::ok) {
                ++acquire_release_ops;
            }
        }
        const auto after = g_allocations.load();
        const auto bench_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - bench_start).count();
        check(after == before);  // steady-state pool path allocates nothing
        check(acquire_release_ops == 100000);
        std::cout << "tensor pool ops=" << acquire_release_ops
                  << " elapsed_ms=" << bench_elapsed << '\n';
    }

    // Worker throughput: submit+poll a bounded batch through two workers.
    {
        latency_executor executor(0, 0, 0);
        inference_worker worker;
        inference_worker_config config;
        config.worker_count_ = 2;
        config.queue_capacity_ = 64;
        check(worker.vqec_vision_ai_sched_inwrk_start(config, executor).code_ ==
              status_code::ok);
        constexpr std::uint64_t total_jobs = 5000;
        std::uint64_t submitted = 0;
        std::uint64_t delivered = 0;
        std::uint64_t next_job = 1;
        const auto worker_start = std::chrono::steady_clock::now();
        // Bounded: submit while capacity allows, then drain completions. A guard prevents
        // an unbounded loop if a completion is dropped under backpressure.
        for (std::uint64_t guard = 0; guard < 50 * total_jobs && submitted < total_jobs;
             ++guard) {
            if (worker.vqec_vision_ai_sched_inwrk_submit(
                    make_item(next_job, 1), model_dispatch_policy::drop_if_busy).code_ ==
                status_code::ok) {
                ++submitted;
                ++next_job;
            }
            inference_work_result result;
            while (worker.vqec_vision_ai_sched_inwrk_poll(result).code_ == status_code::ok) {
                ++delivered;
            }
        }
        // Wait for in-flight jobs, then drain the rest.
        for (std::uint64_t guard = 0; guard < 100000 && delivered < submitted; ++guard) {
            inference_work_result result;
            while (worker.vqec_vision_ai_sched_inwrk_poll(result).code_ == status_code::ok) {
                ++delivered;
            }
            std::this_thread::yield();
        }
        const auto worker_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - worker_start).count();
        check(worker.vqec_vision_ai_sched_inwrk_drain().code_ == status_code::ok);
        check(submitted == total_jobs && delivered > 0);
        std::cout << "worker jobs=" << submitted << " delivered=" << delivered
                  << " elapsed_ms=" << worker_elapsed << '\n';
    }

    std::cout << "device-free harness failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
