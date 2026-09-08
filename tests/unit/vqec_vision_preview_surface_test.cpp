#include <iostream>
#include <type_traits>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_preview_surface.hpp"

int main() {
    using namespace vqec::vision::ai;
    static_assert(!std::is_copy_constructible<writable_preview_surface>::value);
    static_assert(std::is_nothrow_move_constructible<writable_preview_surface>::value);
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    writable_preview_surface writer;
    check(writer.vqec_vision_ai_core_pvsrf_borrow_data() == nullptr);
    check(!writer.vqec_vision_ai_core_pvsrf_seal());
    check(writer.vqec_vision_ai_core_pvsrf_create({3, 2}, 64).code_ == status_code::invalid_argument);
    check(writer.vqec_vision_ai_core_pvsrf_create({2, 2}, 5).code_ == status_code::resource_exhausted);
    check(writer.vqec_vision_ai_core_pvsrf_size_bytes() == 0);
    check(writer.vqec_vision_ai_core_pvsrf_create({2, 2}, 6).code_ == status_code::ok);
    check(writer.vqec_vision_ai_core_pvsrf_size_bytes() == 6);
    if (writer.vqec_vision_ai_core_pvsrf_borrow_data() != nullptr) {
        writer.vqec_vision_ai_core_pvsrf_borrow_data()[0] = 42;
    }
    check(writer.vqec_vision_ai_core_pvsrf_create({4, 4}, 24).code_ == status_code::invalid_state);
    auto moved = std::move(writer);
    check(writer.vqec_vision_ai_core_pvsrf_size_bytes() == 0);
    auto encoder_owner = moved.vqec_vision_ai_core_pvsrf_seal();
    check(moved.vqec_vision_ai_core_pvsrf_borrow_data() == nullptr);
    check(encoder_owner && encoder_owner->size() == 6 && (*encoder_owner)[0] == 42);
    check(!moved.vqec_vision_ai_core_pvsrf_seal());
    std::weak_ptr<const std::vector<std::uint8_t>> observer = encoder_owner;
    auto input_reader = encoder_owner;
    encoder_owner.reset();  // Simulated result-side completion must not destroy input reader.
    check(!observer.expired());
    check(moved.vqec_vision_ai_core_pvsrf_create({4, 4}, 24).code_ == status_code::ok);
    check(input_reader && (*input_reader)[0] == 42);  // New allocation does not recycle old input.
    input_reader.reset();
    check(observer.expired());
    writable_preview_surface empty;
    check(empty.vqec_vision_ai_core_pvsrf_create({8192, 8192}, 64ULL * 1024 * 1024).code_ ==
        status_code::resource_exhausted);
    check(empty.vqec_vision_ai_core_pvsrf_create({2, 2}, 0).code_ == status_code::invalid_argument);
    std::cout << "preview surface failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
