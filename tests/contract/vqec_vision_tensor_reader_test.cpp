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
    return 0;
}
