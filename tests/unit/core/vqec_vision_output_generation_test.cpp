#include <iostream>
#include <limits>
#include <type_traits>

#include "vqec/vision/ai/contracts/vqec_vision_output_generation.hpp"

int main() {
    using namespace vqec::vision::ai;
    static_assert(!std::is_copy_constructible_v<output_generation>);
    static_assert(!std::is_move_constructible_v<output_generation>);
    static_assert(!std::is_copy_assignable_v<output_generation>);
    static_assert(!std::is_move_assignable_v<output_generation>);
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    output_generation issuer;
    std::uint64_t generation = 99;
    check(issuer.vqec_vision_ai_core_otgen_issue(generation).code_ == status_code::ok);
    check(generation == 1);
    // Simulate a failed setup: discarding this ID must not make it available again.
    generation = 0;
    check(issuer.vqec_vision_ai_core_otgen_issue(generation).code_ == status_code::ok);
    check(generation == 2);
    check(issuer.vqec_vision_ai_core_otgen_issue(generation).code_ == status_code::ok);
    check(generation == 3);

    const auto max_generation = std::numeric_limits<std::uint64_t>::max();
    output_generation ending(max_generation - 1);
    check(ending.vqec_vision_ai_core_otgen_issue(generation).code_ == status_code::ok);
    check(generation == max_generation);
    generation = 42;
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        check(ending.vqec_vision_ai_core_otgen_issue(generation).code_ ==
              status_code::resource_exhausted);
        check(generation == 42);
    }
    output_generation exhausted(max_generation);
    check(exhausted.vqec_vision_ai_core_otgen_issue(generation).code_ ==
          status_code::resource_exhausted);
    check(generation == 42);
    std::cout << "output generation failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
