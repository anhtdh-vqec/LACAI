#include "vqec_vision_tensor_reader.hpp"

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {

status vqec_vision_ai_detec_tnrd_find_tensor(
    const tensor_result& _result, const std::string& _name,
    const tensor_blob*& _tensor) noexcept {
    _tensor = nullptr;
    if (_name.empty()) {
        return {status_code::invalid_argument, "tensor name is empty"};
    }
    for (const auto& tensor : _result.tensors_) {
        if (tensor.spec_.name_ == _name) {
            _tensor = &tensor;
            return {};
        }
    }
    return {status_code::unsupported, "tensor name is not present in result"};
}

status vqec_vision_ai_detec_tnrd_validate_tensor(
    const tensor_blob& _tensor, const tensor_spec& _expected) noexcept {
    if (_tensor.spec_.name_ != _expected.name_ || _expected.name_.empty() ||
        _tensor.spec_.dimensions_ != _expected.dimensions_ ||
        _tensor.spec_.dtype_ != _expected.dtype_) {
        return {status_code::invalid_argument,
            "tensor name, shape or dtype differs from manifest"};
    }
    if (_expected.dimensions_.empty() ||
        _expected.dimensions_.size() > tensor_reader_limits::g_max_rank) {
        return {status_code::invalid_argument, "tensor rank is empty or exceeds limit"};
    }
    const auto bytes = vqec_vision_ai_core_tnctr_shape_bytes(_expected);
    if (bytes == 0) {
        return {status_code::invalid_argument, "tensor dimensions are invalid"};
    }
    if (_tensor.bytes_.size() != bytes) {
        return {status_code::invalid_argument, "tensor byte count differs from shape/dtype"};
    }
    return {};
}

}  // namespace vqec::vision::ai
