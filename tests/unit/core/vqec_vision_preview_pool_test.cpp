#include <iostream>
#include <memory>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_preview_pool.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    preview_surface_pool pool;
    writable_preview_surface first;
    writable_preview_surface second;
    check(pool.vqec_vision_ai_core_pvpol_acquire(first).code_ == status_code::invalid_state);
    check(pool.vqec_vision_ai_core_pvpol_configure({2, 2}, 0, 12).code_ ==
        status_code::invalid_argument);
    check(pool.vqec_vision_ai_core_pvpol_configure({2, 2}, 2, 11).code_ ==
        status_code::resource_exhausted);
    check(pool.vqec_vision_ai_core_pvpol_available() == 0);
    check(pool.vqec_vision_ai_core_pvpol_configure({2, 2}, 2, 12).code_ == status_code::ok);
    check(pool.vqec_vision_ai_core_pvpol_allocated_bytes() == 12);
    check(pool.vqec_vision_ai_core_pvpol_configure({4, 4}, 1, 24).code_ ==
        status_code::invalid_state);
    check(pool.vqec_vision_ai_core_pvpol_acquire(first).code_ == status_code::ok);
    check(pool.vqec_vision_ai_core_pvpol_acquire(first).code_ == status_code::invalid_state);
    check(pool.vqec_vision_ai_core_pvpol_available() == 1);
    auto* first_pixels = first.vqec_vision_ai_core_pvsrf_borrow_data();
    if (first_pixels != nullptr) {
        first_pixels[0] = 42;
    }
    check(pool.vqec_vision_ai_core_pvpol_acquire(second).code_ == status_code::ok);
    auto owner = first.vqec_vision_ai_core_pvsrf_seal();
    auto slow_reader = owner;
    std::weak_ptr<const std::vector<std::uint8_t>> old_lease = owner;
    owner.reset();
    check(pool.vqec_vision_ai_core_pvpol_available() == 0);
    check(pool.vqec_vision_ai_core_pvpol_acquire(first).code_ == status_code::resource_exhausted);
    check(slow_reader && (*slow_reader)[0] == 42);
    slow_reader.reset();
    check(old_lease.expired());
    check(pool.vqec_vision_ai_core_pvpol_acquire(first).code_ == status_code::ok);
    check(first.vqec_vision_ai_core_pvsrf_borrow_data() == first_pixels);
    check(!old_lease.lock());  // Reuse does not revive a reader from the previous generation.
    auto reused = first.vqec_vision_ai_core_pvsrf_seal();
    check(reused && (*reused)[0] == 42);  // Reuse deliberately does not clear pixels.
    reused.reset();
    second = writable_preview_surface{};  // Abandon an unsubmitted writable lease.
    check(pool.vqec_vision_ai_core_pvpol_available() == 2);

    std::shared_ptr<const std::vector<std::uint8_t>> survivor;
    {
        preview_surface_pool temporary;
        writable_preview_surface writer;
        check(temporary.vqec_vision_ai_core_pvpol_configure({2, 2}, 1, 6).code_ == status_code::ok);
        check(temporary.vqec_vision_ai_core_pvpol_acquire(writer).code_ == status_code::ok);
        survivor = writer.vqec_vision_ai_core_pvsrf_seal();
    }
    check(survivor && survivor->size() == 6 && (*survivor)[0] == 0);
    std::weak_ptr<const std::vector<std::uint8_t>> observer = survivor;
    survivor.reset();
    check(observer.expired());
    std::cout << "preview pool failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
