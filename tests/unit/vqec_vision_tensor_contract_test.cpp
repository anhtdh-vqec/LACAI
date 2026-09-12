#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto spec = [](const std::string& _name, std::vector<std::uint32_t> _dims,
                         tensor_element_type _dtype = tensor_element_type::float32) {
        tensor_spec value;
        value.name_ = _name;
        value.dimensions_ = std::move(_dims);
        value.dtype_ = _dtype;
        return value;
    };
    unsigned sequence = 0;
    const auto check = [&](const std::vector<tensor_spec>& _outputs,
                           std::uint64_t _budget, status_code _expected,
                           std::uint64_t _bytes = 777) {
        std::uint64_t bytes = 777;
        const auto result = vqec_vision_ai_core_tnctr_validate_outputs(_outputs, _budget, bytes);
        if (result.code_ != _expected || bytes != _bytes) {
            ++failures;
            std::cerr << "case " << sequence << " expected " << static_cast<int>(_expected)
                      << " bytes " << _bytes << " got " << static_cast<int>(result.code_)
                      << " bytes " << bytes << ": " << result.message_ << '\n';
        }
        ++sequence;
    };
    check({spec("boxes", {1, 4}), spec("scores", {1})}, 20, status_code::ok, 20);
    check({spec("boxes", {1, 4}), spec("scores", {1})}, 19, status_code::resource_exhausted);
    check({}, 1024, status_code::invalid_argument);
    check({spec("x", {1})}, 0, status_code::invalid_argument);
    check({spec("x", {1})}, 64ULL * 1024 * 1024 + 1, status_code::invalid_argument);
    check({spec("x", {1}), spec("x", {1})}, 1024, status_code::invalid_argument);
    check({spec("", {1})}, 1024, status_code::invalid_argument);
    check({spec(std::string(128, 'x'), {1})}, 4, status_code::ok, 4);
    check({spec(std::string(129, 'x'), {1})}, 4, status_code::invalid_argument);
    check({spec(std::string("x\0y", 3), {1})}, 4, status_code::invalid_argument);
    check({spec("x", {})}, 1024, status_code::invalid_argument);
    check({spec("x", {0})}, 1024, status_code::invalid_argument);
    check({spec("x", {UINT32_MAX, UINT32_MAX})}, 1024, status_code::invalid_argument);
    check({spec("x", std::vector<std::uint32_t>(8, 1))}, 4, status_code::ok, 4);
    check({spec("x", std::vector<std::uint32_t>(9, 1))}, 4, status_code::invalid_argument);
    check({spec("x", {16777216})}, 64ULL * 1024 * 1024, status_code::ok, 67108864);
    check({spec("x", {16777217})}, 64ULL * 1024 * 1024, status_code::invalid_argument);

    // Every reviewed element type gets its packed byte size.
    check({spec("i8", {4}, tensor_element_type::int8)}, 4, status_code::ok, 4);
    check({spec("u8", {4}, tensor_element_type::uint8)}, 4, status_code::ok, 4);
    check({spec("i16", {4}, tensor_element_type::int16)}, 8, status_code::ok, 8);
    check({spec("u16", {4}, tensor_element_type::uint16)}, 8, status_code::ok, 8);
    check({spec("i32", {4}, tensor_element_type::int32)}, 16, status_code::ok, 16);
    check({spec("u32", {4}, tensor_element_type::uint32)}, 16, status_code::ok, 16);
    check({spec("i64", {4}, tensor_element_type::int64)}, 32, status_code::ok, 32);
    check({spec("u64", {4}, tensor_element_type::uint64)}, 32, status_code::ok, 32);
    check({spec("f16", {4}, tensor_element_type::float16)}, 8, status_code::ok, 8);
    check({spec("f32", {4}, tensor_element_type::float32)}, 16, status_code::ok, 16);

    // Unknown dtype is rejected; quantization rules are explicit.
    check({spec("unknown", {1}, tensor_element_type::unknown)}, 4, status_code::unsupported);
    {
        auto quantized = spec("q", {4}, tensor_element_type::int8);
        quantized.quantization_ = {true, 0.5F, -1};
        check({quantized}, 4, status_code::ok, 4);
    }
    {
        auto quantized_float = spec("qf", {4}, tensor_element_type::float32);
        quantized_float.quantization_ = {true, 0.5F, 0};
        check({quantized_float}, 16, status_code::invalid_argument);
    }
    {
        auto zero_scale = spec("q0", {4}, tensor_element_type::uint8);
        zero_scale.quantization_ = {true, 0.0F, 0};
        check({zero_scale}, 4, status_code::invalid_argument);
    }

    std::vector<tensor_spec> outputs;
    for (unsigned index = 0; index < 16; ++index) {
        outputs.push_back(spec("tensor_" + std::to_string(index), {1}));
    }
    check(outputs, 64, status_code::ok, 64);
    outputs.push_back(spec("extra", {1}));
    check(outputs, 128, status_code::invalid_argument);
    std::cout << "tensor contract failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
