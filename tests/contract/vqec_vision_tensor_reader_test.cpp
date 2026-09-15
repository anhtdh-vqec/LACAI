#include <cassert>
#include <cstdint>
#include <utility>

#include "vqec_vision_tensor_reader.hpp"

using namespace vqec::vision::ai;

namespace {

tensor_spec vqec_vision_ai_ctest_trct_spec(
    const char* _name, std::vector<std::uint32_t> _dims,
    tensor_element_type _dtype = tensor_element_type::float32) {
    tensor_spec spec;
    spec.name_ = _name;
    spec.dimensions_ = std::move(_dims);
    spec.dtype_ = _dtype;
    return spec;
}

}  // namespace

int main() {
    tensor_result result;
    tensor_blob blob;
    blob.spec_ = vqec_vision_ai_ctest_trct_spec("scores", {1, 2});
    blob.bytes_.assign(8, 0U);  // two float32 elements
    result.tensors_.push_back(std::move(blob));

    const tensor_blob* found = nullptr;
    assert(vqec_vision_ai_detec_tnrd_find_tensor(result, "scores", found).code_ ==
           status_code::ok);
    assert(found != nullptr);
    assert(vqec_vision_ai_detec_tnrd_validate_tensor(
               *found, vqec_vision_ai_ctest_trct_spec("scores", {1, 2})).code_ ==
           status_code::ok);
    assert(vqec_vision_ai_detec_tnrd_find_tensor(result, "missing", found).code_ ==
           status_code::unsupported);
    assert(found == nullptr);
    assert(vqec_vision_ai_detec_tnrd_validate_tensor(
               result.tensors_[0], vqec_vision_ai_ctest_trct_spec("scores", {2, 2})).code_ ==
           status_code::invalid_argument);
    // dtype must match, not only shape.
    assert(vqec_vision_ai_detec_tnrd_validate_tensor(
               result.tensors_[0],
               vqec_vision_ai_ctest_trct_spec("scores", {1, 2}, tensor_element_type::int8)).code_ ==
           status_code::invalid_argument);
    result.tensors_[0].bytes_.pop_back();
    assert(vqec_vision_ai_detec_tnrd_validate_tensor(
               result.tensors_[0], vqec_vision_ai_ctest_trct_spec("scores", {1, 2})).code_ ==
           status_code::invalid_argument);

    tensor_blob invalid;
    invalid.spec_.name_ = "invalid";
    invalid.spec_.dimensions_ = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    assert(vqec_vision_ai_detec_tnrd_validate_tensor(
               invalid, invalid.spec_).code_ == status_code::invalid_argument);

    tensor_blob quantized;
    quantized.spec_ = vqec_vision_ai_ctest_trct_spec(
        "quantized", {2}, tensor_element_type::uint16);
    quantized.spec_.quantization_ = {true, 0.5F, 10};
    quantized.bytes_ = {10U, 0U, 14U, 0U};
    float scalar = -1.0F;
    assert(vqec_vision_ai_detec_tnrd_read_scalar(quantized, 1, scalar).code_ ==
        status_code::ok);
    assert(scalar == 2.0F);
    const float preserved = scalar;
    assert(vqec_vision_ai_detec_tnrd_read_scalar(quantized, 2, scalar).code_ ==
        status_code::protocol_error);
    assert(scalar == preserved);
    quantized.spec_.quantization_.is_quantized_ = false;
    assert(vqec_vision_ai_detec_tnrd_read_scalar(quantized, 0, scalar).code_ ==
        status_code::invalid_argument);

    tensor_blob half;
    half.spec_ = vqec_vision_ai_ctest_trct_spec(
        "half", {1}, tensor_element_type::float16);
    half.bytes_ = {0x00U, 0x3cU};
    assert(vqec_vision_ai_detec_tnrd_read_scalar(half, 0, scalar).code_ ==
        status_code::ok);
    assert(scalar == 1.0F);
    return 0;
}
