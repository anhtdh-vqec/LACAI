// Device-free tests for the bounded tensor pool: preallocation, exhaustion, double-release
// detection, poison-on-release, high-watermark and concurrent acquire/release stability.

#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

#include "vqec_vision_tensor_pool.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

using namespace vqec::vision::ai;

namespace {

tensor_spec make_spec() {
    tensor_spec spec;
    spec.name_ = "output";
    spec.dimensions_ = {1, 8, 8, 3};
    spec.dtype_ = tensor_element_type::float32;
    return spec;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    // Configuration validation and preallocation.
    {
        tensor_pool pool;
        tensor_pool_config bad;
        bad.capacity_ = 0;
        bad.spec_ = make_spec();
        check(pool.vqec_vision_ai_core_tnpl_configure(bad).code_ ==
              status_code::invalid_argument);
        bad.capacity_ = 2;
        bad.spec_.dtype_ = tensor_element_type::unknown;
        check(pool.vqec_vision_ai_core_tnpl_configure(bad).code_ ==
              status_code::invalid_argument);
        bad.spec_ = make_spec();
        bad.spec_.dimensions_ = {
            static_cast<std::uint32_t>(tensor_contract_limits::g_max_output_bytes + 1U)};
        check(pool.vqec_vision_ai_core_tnpl_configure(bad).code_ ==
              status_code::invalid_argument);
        bad.spec_ = make_spec();
        check(pool.vqec_vision_ai_core_tnpl_configure(bad).code_ == status_code::ok);
        check(pool.vqec_vision_ai_core_tnpl_configure(bad).code_ ==
              status_code::invalid_state);
        const auto stats = pool.vqec_vision_ai_core_tnpl_get_stats();
        check(stats.capacity_ == 2 && stats.free_ == 2 && stats.live_ == 0);
    }

    // Acquire/release, exhaustion, double release and read access.
    {
        tensor_pool pool;
        tensor_pool_config config;
        config.capacity_ = 2;
        config.spec_ = make_spec();
        check(pool.vqec_vision_ai_core_tnpl_configure(config).code_ == status_code::ok);
        std::size_t first = 0;
        std::size_t second = 0;
        std::size_t third = 0;
        tensor_blob* blob = nullptr;
        check(pool.vqec_vision_ai_core_tnpl_acquire(first, blob).code_ == status_code::ok);
        check(blob != nullptr && blob->bytes_.size() == 8U * 8U * 3U * 4U);
        check(pool.vqec_vision_ai_core_tnpl_acquire(second, blob).code_ == status_code::ok);
        check(first != second);
        check(pool.vqec_vision_ai_core_tnpl_acquire(third, blob).code_ ==
              status_code::resource_exhausted);
        tensor_blob* reread = nullptr;
        check(pool.vqec_vision_ai_core_tnpl_get(first, reread).code_ == status_code::ok);
        check(reread != nullptr);
        check(pool.vqec_vision_ai_core_tnpl_release(first).code_ == status_code::ok);
        check(pool.vqec_vision_ai_core_tnpl_release(first).code_ ==
              status_code::invalid_state);
        check(pool.vqec_vision_ai_core_tnpl_get(first, reread).code_ ==
              status_code::invalid_argument);
        check(pool.vqec_vision_ai_core_tnpl_release(second).code_ == status_code::ok);
        const auto stats = pool.vqec_vision_ai_core_tnpl_get_stats();
        check(stats.acquire_total_ == 2 && stats.release_total_ == 2 &&
              stats.exhausted_total_ == 1 && stats.double_release_total_ == 1 &&
              stats.high_watermark_ == 2 && stats.live_ == 0);
    }

    // Poison on release exposes use-after-release in debug configurations.
    {
        tensor_pool pool;
        tensor_pool_config config;
        config.capacity_ = 1;
        config.spec_ = make_spec();
        config.poison_on_release_ = true;
        check(pool.vqec_vision_ai_core_tnpl_configure(config).code_ == status_code::ok);
        std::size_t slot = 0;
        tensor_blob* blob = nullptr;
        check(pool.vqec_vision_ai_core_tnpl_acquire(slot, blob).code_ == status_code::ok);
        blob->bytes_[0] = 0x7F;
        check(pool.vqec_vision_ai_core_tnpl_release(slot).code_ == status_code::ok);
        check(pool.vqec_vision_ai_core_tnpl_acquire(slot, blob).code_ == status_code::ok);
        check(blob->bytes_[0] == 0xDD);
        check(pool.vqec_vision_ai_core_tnpl_release(slot).code_ == status_code::ok);
    }

    // Steady-state reuse over many cycles keeps the high-watermark bounded.
    {
        tensor_pool pool;
        tensor_pool_config config;
        config.capacity_ = 4;
        config.spec_ = make_spec();
        check(pool.vqec_vision_ai_core_tnpl_configure(config).code_ == status_code::ok);
        bool ok = true;
        for (int cycle = 0; cycle < 1000; ++cycle) {
            std::size_t slot = 0;
            tensor_blob* blob = nullptr;
            ok = ok && pool.vqec_vision_ai_core_tnpl_acquire(slot, blob).code_ ==
                status_code::ok;
            ok = ok && pool.vqec_vision_ai_core_tnpl_release(slot).code_ == status_code::ok;
        }
        check(ok);
        const auto stats = pool.vqec_vision_ai_core_tnpl_get_stats();
        check(stats.high_watermark_ == 1 && stats.live_ == 0 &&
              stats.exhausted_total_ == 0 && stats.double_release_total_ == 0);
    }

    // Concurrent acquire/release never double-allocates a slot.
    {
        tensor_pool pool;
        tensor_pool_config config;
        config.capacity_ = 4;
        config.spec_ = make_spec();
        check(pool.vqec_vision_ai_core_tnpl_configure(config).code_ == status_code::ok);
        constexpr unsigned thread_count = 4;
        constexpr unsigned iterations = 1000;
        std::atomic<bool> start{false};
        std::atomic<unsigned> errors{0};
        std::vector<std::thread> threads;
        threads.reserve(thread_count);
        for (unsigned thread = 0; thread < thread_count; ++thread) {
            threads.emplace_back([&pool, &start, &errors]() {
                while (!start.load()) {
                }
                for (unsigned cycle = 0; cycle < iterations; ++cycle) {
                    std::size_t slot = 0;
                    tensor_blob* blob = nullptr;
                    if (pool.vqec_vision_ai_core_tnpl_acquire(slot, blob).code_ !=
                            status_code::ok ||
                        pool.vqec_vision_ai_core_tnpl_release(slot).code_ !=
                            status_code::ok) {
                        ++errors;
                        return;
                    }
                }
            });
        }
        start.store(true);
        for (auto& thread : threads) {
            thread.join();
        }
        const auto stats = pool.vqec_vision_ai_core_tnpl_get_stats();
        check(errors.load() == 0 && stats.live_ == 0 &&
              stats.acquire_total_ == stats.release_total_ &&
              stats.acquire_total_ == thread_count * iterations &&
              stats.double_release_total_ == 0 && stats.exhausted_total_ == 0);
    }

    std::cout << "tensor pool failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
