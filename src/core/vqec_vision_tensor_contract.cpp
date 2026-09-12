#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

#include <cmath>
#include <limits>

namespace vqec::vision::ai {

std::size_t vqec_vision_ai_core_tnctr_element_size(tensor_element_type _type) noexcept {
    switch (_type) {
        case tensor_element_type::int8:
        case tensor_element_type::uint8:
            return 1;
        case tensor_element_type::int16:
        case tensor_element_type::uint16:
        case tensor_element_type::float16:
            return 2;
        case tensor_element_type::int32:
        case tensor_element_type::uint32:
        case tensor_element_type::float32:
            return 4;
        case tensor_element_type::int64:
        case tensor_element_type::uint64:
            return 8;
        case tensor_element_type::unknown:
            break;
    }
    return 0;
}

const char* vqec_vision_ai_core_tnctr_element_type_name(
    tensor_element_type _type) noexcept {
    switch (_type) {
        case tensor_element_type::int8: return "int8";
        case tensor_element_type::uint8: return "uint8";
        case tensor_element_type::int16: return "int16";
        case tensor_element_type::uint16: return "uint16";
        case tensor_element_type::int32: return "int32";
        case tensor_element_type::uint32: return "uint32";
        case tensor_element_type::int64: return "int64";
        case tensor_element_type::uint64: return "uint64";
        case tensor_element_type::float16: return "float16";
        case tensor_element_type::float32: return "float32";
        case tensor_element_type::unknown: break;
    }
    return "unknown";
}

tensor_element_type vqec_vision_ai_core_tnctr_element_type_from_name(
    const std::string& _name) noexcept {
    if (_name == "int8") return tensor_element_type::int8;
    if (_name == "uint8") return tensor_element_type::uint8;
    if (_name == "int16") return tensor_element_type::int16;
    if (_name == "uint16") return tensor_element_type::uint16;
    if (_name == "int32") return tensor_element_type::int32;
    if (_name == "uint32") return tensor_element_type::uint32;
    if (_name == "int64") return tensor_element_type::int64;
    if (_name == "uint64") return tensor_element_type::uint64;
    if (_name == "float16") return tensor_element_type::float16;
    if (_name == "float32") return tensor_element_type::float32;
    return tensor_element_type::unknown;
}

std::uint64_t vqec_vision_ai_core_tnctr_shape_bytes(const tensor_spec& _spec) noexcept {
    const auto element_size = vqec_vision_ai_core_tnctr_element_size(_spec.dtype_);
    if (element_size == 0 || _spec.dimensions_.empty() ||
        _spec.dimensions_.size() > tensor_contract_limits::g_max_rank) {
        return 0;
    }
    std::uint64_t elements = 1;
    for (const auto dimension : _spec.dimensions_) {
        if (dimension == 0 || elements > std::numeric_limits<std::uint64_t>::max() / dimension) {
            return 0;
        }
        elements *= dimension;
    }
    if (elements > std::numeric_limits<std::uint64_t>::max() / element_size) {
        return 0;
    }
    return elements * element_size;
}

namespace {

bool vqec_vision_ai_core_tnctr_is_floating(tensor_element_type _type) noexcept {
    return _type == tensor_element_type::float16 || _type == tensor_element_type::float32;
}

status vqec_vision_ai_core_tnctr_validate_spec(const tensor_spec& _spec) {
    if (_spec.name_.empty() ||
        _spec.name_.size() > tensor_contract_limits::g_max_name_bytes ||
        _spec.name_.find('\0') != std::string::npos ||
        _spec.dimensions_.empty() ||
        _spec.dimensions_.size() > tensor_contract_limits::g_max_rank) {
        return {status_code::invalid_argument, "invalid tensor name or rank"};
    }
    if (_spec.dtype_ == tensor_element_type::unknown) {
        return {status_code::unsupported, "tensor element type is unknown"};
    }
    if (_spec.quantization_.is_quantized_) {
        if (vqec_vision_ai_core_tnctr_is_floating(_spec.dtype_)) {
            return {status_code::invalid_argument, "floating tensor cannot be quantized"};
        }
        if (!std::isfinite(_spec.quantization_.scale_) || _spec.quantization_.scale_ <= 0.0F) {
            return {status_code::invalid_argument, "quantization scale must be finite and positive"};
        }
    }
    if (vqec_vision_ai_core_tnctr_shape_bytes(_spec) == 0) {
        return {status_code::invalid_argument, "invalid tensor shape or excessive size"};
    }
    return {};
}

}  // namespace

status vqec_vision_ai_core_tnctr_validate_outputs(
    const std::vector<tensor_spec>& _outputs, std::uint64_t _max_output_bytes,
    std::uint64_t& _required_bytes) {
    if (_outputs.empty() || _outputs.size() > tensor_contract_limits::g_max_outputs ||
        _max_output_bytes == 0 ||
        _max_output_bytes > tensor_contract_limits::g_max_output_bytes) {
        return {status_code::invalid_argument, "invalid output count or byte budget"};
    }
    std::uint64_t total = 0;
    for (std::size_t index = 0; index < _outputs.size(); ++index) {
        const auto& output = _outputs[index];
        const auto valid = vqec_vision_ai_core_tnctr_validate_spec(output);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (_outputs[previous].name_ == output.name_) {
                return {status_code::invalid_argument, "duplicate output contract name"};
            }
        }
        const auto bytes = vqec_vision_ai_core_tnctr_shape_bytes(output);
        if (bytes > _max_output_bytes) {
            return {status_code::invalid_argument, "invalid output shape or excessive size"};
        }
        if (bytes > _max_output_bytes - total) {
            return {status_code::resource_exhausted, "output contract exceeds byte budget"};
        }
        total += bytes;
    }
    _required_bytes = total;
    return {};
}

}  // namespace vqec::vision::ai
