#include "vqec_vision_tensor_reader.hpp"

#include <limits>

namespace vqec::vision::ai {
namespace {

status vqec_vision_ai_detec_tnrd_count_elements(
    const std::vector<std::uint32_t>& _dimensions, std::size_t& _count) noexcept {
    if (_dimensions.empty() || _dimensions.size() > tensor_reader_limits::g_max_rank) {
        return {status_code::invalid_argument, "tensor rank is empty or exceeds limit"};
    }
    std::uint64_t count = 1;
    for (const auto dimension : _dimensions) {
        if (dimension == 0 || count > std::numeric_limits<std::uint64_t>::max() / dimension) {
            return {status_code::invalid_argument, "tensor dimensions are invalid"};
        }
        count *= dimension;
    }
    if (count > std::numeric_limits<std::size_t>::max()) {
        return {status_code::resource_exhausted, "tensor element count exceeds host size"};
    }
    _count = static_cast<std::size_t>(count);
    return {};
}

}  // namespace

status vqec_vision_ai_detec_tnrd_find_tensor(
    const tensor_result& _result, const std::string& _name,
    const float_tensor_result*& _tensor) noexcept {
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
    const float_tensor_result& _tensor, const float_tensor_spec& _expected) noexcept {
    if (_tensor.spec_.name_ != _expected.name_ || _expected.name_.empty() ||
        _tensor.spec_.dimensions_ != _expected.dimensions_) {
        return {status_code::invalid_argument, "tensor name or shape differs from manifest"};
    }
    std::size_t element_count = 0;
    const auto counted = vqec_vision_ai_detec_tnrd_count_elements(
        _expected.dimensions_, element_count);
    if (counted.code_ != status_code::ok) {
        return counted;
    }
    if (_tensor.values_.size() != element_count) {
        return {status_code::invalid_argument, "tensor value count differs from shape"};
    }
    return {};
}

}  // namespace vqec::vision::ai
