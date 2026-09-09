#include <cassert>

#include "vqec_vision_tensor_reader.hpp"

using namespace vqec::vision::ai;

int main() {
    tensor_result result;
    result.tensors_.push_back({{"scores", {1, 2}}, {0.1F, 0.9F}});
    const float_tensor_result* found = nullptr;
    assert(vqec_vision_ai_detec_tnrd_find_tensor(result, "scores", found).code_ ==
           status_code::ok);
    assert(found != nullptr);
    assert(vqec_vision_ai_detec_tnrd_validate_tensor(
               *found, {"scores", {1, 2}}).code_ == status_code::ok);
    assert(vqec_vision_ai_detec_tnrd_find_tensor(result, "missing", found).code_ ==
           status_code::unsupported);
    assert(found == nullptr);
    assert(vqec_vision_ai_detec_tnrd_validate_tensor(
               result.tensors_[0], {"scores", {2, 2}}).code_ == status_code::invalid_argument);
    result.tensors_[0].values_.pop_back();
    assert(vqec_vision_ai_detec_tnrd_validate_tensor(
               result.tensors_[0], {"scores", {1, 2}}).code_ == status_code::invalid_argument);
    float_tensor_result invalid;
    invalid.spec_.name_ = "invalid";
    invalid.spec_.dimensions_ = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    assert(vqec_vision_ai_detec_tnrd_validate_tensor(
               invalid, invalid.spec_).code_ == status_code::invalid_argument);
    return 0;
}
