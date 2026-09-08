#include <iostream>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&](const std::vector<float_tensor_spec>& _outputs,
                           std::uint64_t _budget, status_code _expected,
                           std::uint64_t _bytes = 777) {
        std::uint64_t bytes = 777;
        const auto result = vqec_vision_ai_core_tnctr_validate_outputs(_outputs, _budget, bytes);
        if (result.code_ != _expected || bytes != _bytes) {
            ++failures;
            std::cerr << result.message_ << '\n';
        }
    };
    check({{"boxes", {1, 4}}, {"scores", {1}}}, 20, status_code::ok, 20);
    check({{"boxes", {1, 4}}, {"scores", {1}}}, 19, status_code::resource_exhausted);
    check({}, 1024, status_code::invalid_argument);
    check({{"x", {1}}}, 0, status_code::invalid_argument);
    check({{"x", {1}}}, 64ULL * 1024 * 1024 + 1, status_code::invalid_argument);
    check({{"x", {1}}, {"x", {1}}}, 1024, status_code::invalid_argument);
    check({{"", {1}}}, 1024, status_code::invalid_argument);
    check({{std::string(128, 'x'), {1}}}, 4, status_code::ok, 4);
    check({{std::string(129, 'x'), {1}}}, 4, status_code::invalid_argument);
    check({{std::string("x\0y", 3), {1}}}, 4, status_code::invalid_argument);
    check({{"x", {}}}, 1024, status_code::invalid_argument);
    check({{"x", {0}}}, 1024, status_code::invalid_argument);
    check({{"x", {UINT32_MAX, UINT32_MAX}}}, 1024, status_code::invalid_argument);
    check({{"x", std::vector<std::uint32_t>(8, 1)}}, 4, status_code::ok, 4);
    check({{"x", std::vector<std::uint32_t>(9, 1)}}, 4, status_code::invalid_argument);
    check({{"x", {16777216}}}, 64ULL * 1024 * 1024, status_code::ok, 67108864);
    check({{"x", {16777217}}}, 64ULL * 1024 * 1024, status_code::invalid_argument);
    std::vector<float_tensor_spec> outputs;
    for (unsigned index = 0; index < 16; ++index) {
        outputs.push_back({"tensor_" + std::to_string(index), {1}});
    }
    check(outputs, 64, status_code::ok, 64);
    outputs.push_back({"extra", {1}});
    check(outputs, 128, status_code::invalid_argument);
    std::cout << "tensor contract failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
