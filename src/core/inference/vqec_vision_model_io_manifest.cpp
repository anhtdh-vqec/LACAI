#include "vqec/vision/ai/contracts/vqec_vision_model_io_manifest.hpp"

#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_ioman_is_integral(tensor_element_type _type) noexcept {
    switch (_type) {
        case tensor_element_type::int8:
        case tensor_element_type::uint8:
        case tensor_element_type::int16:
        case tensor_element_type::uint16:
        case tensor_element_type::int32:
        case tensor_element_type::uint32:
        case tensor_element_type::int64:
        case tensor_element_type::uint64:
            return true;
        default:
            return false;
    }
}

status vqec_vision_ai_core_ioman_validate_specs(
    const std::vector<tensor_spec>& _specs, const char* _label) {
    // The label ("input"/"output") keeps the two spec lists distinguishable in diagnostics.
    const std::string prefix =
        _label != nullptr ? std::string(_label) + " " : std::string();
    for (std::size_t index = 0; index < _specs.size(); ++index) {
        const auto& spec = _specs[index];
        if (spec.name_.empty()) {
            return {status_code::invalid_argument, prefix + "model IO tensor name is empty"};
        }
        if (spec.dtype_ == tensor_element_type::unknown) {
            return {status_code::invalid_argument, prefix + "model IO tensor dtype is unknown"};
        }
        if (spec.dimensions_.empty()) {
            return {status_code::invalid_argument, prefix + "model IO tensor has no rank"};
        }
        for (const auto dimension : spec.dimensions_) {
            if (dimension == 0) {
                return {status_code::invalid_argument,
                    prefix + "model IO tensor has a zero dimension"};
            }
        }
        if (vqec_vision_ai_core_tnctr_shape_bytes(spec) == 0) {
            return {status_code::invalid_argument, prefix + "model IO tensor has zero bytes"};
        }
        if (spec.quantization_.is_quantized_ &&
            (spec.quantization_.scale_ <= 0.0F || !vqec_vision_ai_core_ioman_is_integral(spec.dtype_))) {
            return {status_code::invalid_argument,
                prefix + "model IO tensor quantization is invalid"};
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (_specs[prior].name_ == spec.name_) {
                return {status_code::invalid_argument,
                    prefix + "model IO tensor name is duplicated"};
            }
        }
    }
    return {};
}

}  // namespace

status vqec_vision_ai_core_ioman_validate(const model_io_manifest& _manifest) noexcept {
    if (_manifest.inputs_.empty()) {
        return {status_code::invalid_argument, "model IO manifest has no inputs"};
    }
    if (_manifest.outputs_.empty()) {
        return {status_code::invalid_argument, "model IO manifest has no outputs"};
    }
    const auto inputs = vqec_vision_ai_core_ioman_validate_specs(_manifest.inputs_, "input");
    if (inputs.code_ != status_code::ok) {
        return inputs;
    }
    return vqec_vision_ai_core_ioman_validate_specs(_manifest.outputs_, "output");
}

bool vqec_vision_ai_core_ioman_same_spec(
    const tensor_spec& _declared, const tensor_spec& _actual) noexcept {
    return _declared.name_ == _actual.name_ &&
        _declared.dimensions_ == _actual.dimensions_ &&
        _declared.dtype_ == _actual.dtype_ &&
        _declared.layout_ == _actual.layout_ &&
        _declared.quantization_.is_quantized_ == _actual.quantization_.is_quantized_ &&
        _declared.quantization_.scale_ == _actual.quantization_.scale_ &&
        _declared.quantization_.zero_point_ == _actual.quantization_.zero_point_;
}

status vqec_vision_ai_core_ioman_matches(
    const model_io_manifest& _declared, const model_io_manifest& _actual) {
    const auto valid_declared = vqec_vision_ai_core_ioman_validate(_declared);
    if (valid_declared.code_ != status_code::ok) {
        return valid_declared;
    }
    if (_declared.inputs_.size() != _actual.inputs_.size() ||
        _declared.outputs_.size() != _actual.outputs_.size()) {
        return {status_code::unsupported,
            "declared model IO count differs from the graph"};
    }
    for (std::size_t index = 0; index < _declared.inputs_.size(); ++index) {
        if (!vqec_vision_ai_core_ioman_same_spec(_declared.inputs_[index], _actual.inputs_[index])) {
            return {status_code::unsupported,
                "declared model input identity differs from the graph"};
        }
    }
    for (std::size_t index = 0; index < _declared.outputs_.size(); ++index) {
        if (!vqec_vision_ai_core_ioman_same_spec(
                _declared.outputs_[index], _actual.outputs_[index])) {
            return {status_code::unsupported,
                "declared model output identity differs from the graph"};
        }
    }
    return {};
}

}  // namespace vqec::vision::ai
