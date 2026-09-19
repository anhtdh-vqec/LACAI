#include "vqec_vision_tensor_reader.hpp"

#include <cmath>
#include <cstring>
#include <limits>

#include "vqec/vision/ai/contracts/inference/vqec_vision_tensor_contract.hpp"

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

float vqec_vision_ai_detec_tnrd_half_to_float(std::uint16_t _bits) noexcept {
    const std::uint32_t sign = static_cast<std::uint32_t>(_bits & 0x8000U) << 16U;
    std::uint32_t exponent = (_bits >> 10U) & 0x1fU;
    std::uint32_t mantissa = _bits & 0x03ffU;
    std::uint32_t result_bits = 0;
    if (exponent == 0) {
        if (mantissa == 0) {
            result_bits = sign;
        } else {
            exponent = 113U;
            while ((mantissa & 0x0400U) == 0) {
                mantissa <<= 1U;
                --exponent;
            }
            result_bits = sign | (exponent << 23U) | ((mantissa & 0x03ffU) << 13U);
        }
    } else if (exponent == 0x1fU) {
        result_bits = sign | 0x7f800000U | (mantissa << 13U);
    } else {
        result_bits = sign | ((exponent + 112U) << 23U) | (mantissa << 13U);
    }
    float result = 0.0F;
    std::memcpy(&result, &result_bits, sizeof(result));
    return result;
}

template <typename value_type>
float vqec_vision_ai_detec_tnrd_load_value(const std::uint8_t* _bytes) noexcept {
    value_type value{};
    std::memcpy(&value, _bytes, sizeof(value));
    return static_cast<float>(value);
}

status vqec_vision_ai_detec_tnrd_read_scalar(
    const tensor_blob& _tensor, std::size_t _index, float& _value) noexcept {
    const auto element_bytes =
        vqec_vision_ai_core_tnctr_element_size(_tensor.spec_.dtype_);
    if (element_bytes == 0 || _index >= _tensor.bytes_.size() / element_bytes ||
        _tensor.bytes_.size() % element_bytes != 0) {
        return {status_code::protocol_error, "tensor scalar index is out of range"};
    }
    const auto* bytes = _tensor.bytes_.data() + _index * element_bytes;
    float stored = 0.0F;
    switch (_tensor.spec_.dtype_) {
        case tensor_element_type::int8:
            stored = vqec_vision_ai_detec_tnrd_load_value<std::int8_t>(bytes);
            break;
        case tensor_element_type::uint8:
            stored = vqec_vision_ai_detec_tnrd_load_value<std::uint8_t>(bytes);
            break;
        case tensor_element_type::int16:
            stored = vqec_vision_ai_detec_tnrd_load_value<std::int16_t>(bytes);
            break;
        case tensor_element_type::uint16:
            stored = vqec_vision_ai_detec_tnrd_load_value<std::uint16_t>(bytes);
            break;
        case tensor_element_type::int32:
            stored = vqec_vision_ai_detec_tnrd_load_value<std::int32_t>(bytes);
            break;
        case tensor_element_type::uint32:
            stored = vqec_vision_ai_detec_tnrd_load_value<std::uint32_t>(bytes);
            break;
        case tensor_element_type::int64:
            stored = vqec_vision_ai_detec_tnrd_load_value<std::int64_t>(bytes);
            break;
        case tensor_element_type::uint64:
            stored = vqec_vision_ai_detec_tnrd_load_value<std::uint64_t>(bytes);
            break;
        case tensor_element_type::float16: {
            std::uint16_t bits = 0;
            std::memcpy(&bits, bytes, sizeof(bits));
            stored = vqec_vision_ai_detec_tnrd_half_to_float(bits);
            break;
        }
        case tensor_element_type::float32:
            stored = vqec_vision_ai_detec_tnrd_load_value<float>(bytes);
            break;
        default:
            return {status_code::unsupported, "tensor scalar dtype is unsupported"};
    }
    const bool is_float = _tensor.spec_.dtype_ == tensor_element_type::float16 ||
        _tensor.spec_.dtype_ == tensor_element_type::float32;
    float candidate = stored;
    if (is_float) {
        if (_tensor.spec_.quantization_.is_quantized_) {
            return {status_code::invalid_argument, "floating tensor cannot be quantized"};
        }
    } else {
        const auto& quantization = _tensor.spec_.quantization_;
        if (!quantization.is_quantized_ || !std::isfinite(quantization.scale_) ||
            quantization.scale_ <= 0.0F) {
            return {status_code::invalid_argument, "integer tensor quantization is invalid"};
        }
        candidate = (stored - static_cast<float>(quantization.zero_point_)) *
            quantization.scale_;
    }
    if (!is_float && !std::isfinite(candidate)) {
        return {status_code::protocol_error, "tensor scalar is not finite"};
    }
    _value = candidate;
    return {};
}

}  // namespace vqec::vision::ai
