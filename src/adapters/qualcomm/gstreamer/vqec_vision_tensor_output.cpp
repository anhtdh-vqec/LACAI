#include "vqec_vision_tensor_output.hpp"

#include <array>
#include <cstring>
#include <new>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

struct memory_mapping {
    GstMemory* memory_{nullptr};
    GstMapInfo map_ = GST_MAP_INFO_INIT;
    bool mapped_{false};
    ~memory_mapping() noexcept {
        if (mapped_) {
            gst_memory_unmap(memory_, &map_);
        }
    }
};

}  // namespace

tensor_element_type vqec_vision_ai_qcom_tnout_element_type_from_ml(
    const char* _name) noexcept {
    if (_name == nullptr) {
        return tensor_element_type::unknown;
    }
    if (std::strcmp(_name, "INT8") == 0) return tensor_element_type::int8;
    if (std::strcmp(_name, "UINT8") == 0) return tensor_element_type::uint8;
    if (std::strcmp(_name, "INT16") == 0) return tensor_element_type::int16;
    if (std::strcmp(_name, "UINT16") == 0) return tensor_element_type::uint16;
    if (std::strcmp(_name, "INT32") == 0) return tensor_element_type::int32;
    if (std::strcmp(_name, "UINT32") == 0) return tensor_element_type::uint32;
    if (std::strcmp(_name, "INT64") == 0) return tensor_element_type::int64;
    if (std::strcmp(_name, "UINT64") == 0) return tensor_element_type::uint64;
    if (std::strcmp(_name, "FLOAT16") == 0) return tensor_element_type::float16;
    if (std::strcmp(_name, "FLOAT32") == 0) return tensor_element_type::float32;
    return tensor_element_type::unknown;
}

const char* vqec_vision_ai_qcom_tnout_ml_type_name(
    tensor_element_type _type) noexcept {
    switch (_type) {
        case tensor_element_type::int8: return "INT8";
        case tensor_element_type::uint8: return "UINT8";
        case tensor_element_type::int16: return "INT16";
        case tensor_element_type::uint16: return "UINT16";
        case tensor_element_type::int32: return "INT32";
        case tensor_element_type::uint32: return "UINT32";
        case tensor_element_type::int64: return "INT64";
        case tensor_element_type::uint64: return "UINT64";
        case tensor_element_type::float16: return "FLOAT16";
        case tensor_element_type::float32: return "FLOAT32";
        case tensor_element_type::unknown: break;
    }
    return nullptr;
}

status vqec_vision_ai_qcom_tnout_copy_sample(
    GstSample* _sample, const std::vector<tensor_spec>& _expected,
    std::uint64_t _max_bytes, tensor_result& _result) {
    if (_sample == nullptr || _expected.empty() ||
        _expected.size() > tensor_contract_limits::g_max_outputs ||
        _max_bytes == 0 || _max_bytes > tensor_contract_limits::g_max_output_bytes) {
        return {status_code::invalid_argument, "invalid sample, tensor count or byte budget"};
    }
    auto* caps = gst_sample_get_caps(_sample);
    auto* buffer = gst_sample_get_buffer(_sample);
    if (caps == nullptr || buffer == nullptr || gst_caps_get_size(caps) != 1 ||
        !gst_caps_is_fixed(caps) || GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_GAP) ||
        GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_CORRUPTED)) {
        return {status_code::protocol_error, "missing/flexible caps or invalid tensor buffer"};
    }
    const auto* structure = gst_caps_get_structure(caps, 0);
    const auto* type = gst_structure_get_string(structure, "type");
    const auto* dimensions = gst_structure_get_value(structure, "dimensions");
    const auto caps_type = vqec_vision_ai_qcom_tnout_element_type_from_ml(type);
    if (!gst_structure_has_name(structure, "neural-network/tensors") ||
        caps_type == tensor_element_type::unknown || dimensions == nullptr ||
        !GST_VALUE_HOLDS_ARRAY(dimensions) ||
        gst_value_array_get_size(dimensions) != _expected.size() ||
        gst_buffer_n_memory(buffer) != _expected.size()) {
        return {status_code::unsupported,
            "caps type/count/layout is not a supported packed tensor result"};
    }
    // The reviewed caps carry one element type for all tensors. Reject a model contract
    // that requires a different or mixed type instead of silently misreading bytes.
    std::array<std::size_t, tensor_contract_limits::g_max_outputs> sizes{};
    std::uint64_t total = 0;
    for (std::size_t index = 0; index < _expected.size(); ++index) {
        const auto& spec = _expected[index];
        if (spec.dtype_ != caps_type) {
            return {status_code::unsupported,
                "model output dtype differs from the negotiated caps type"};
        }
        const auto bytes = vqec_vision_ai_core_tnctr_shape_bytes(spec);
        if (bytes == 0) {
            return {status_code::invalid_argument, "invalid model output spec"};
        }
        const auto* shape = gst_value_array_get_value(dimensions, static_cast<guint>(index));
        if (shape == nullptr || !GST_VALUE_HOLDS_ARRAY(shape) ||
            gst_value_array_get_size(shape) != spec.dimensions_.size()) {
            return {status_code::protocol_error, "tensor rank does not match model contract"};
        }
        for (std::size_t axis = 0; axis < spec.dimensions_.size(); ++axis) {
            const auto* value = gst_value_array_get_value(shape, static_cast<guint>(axis));
            if (value == nullptr || !G_VALUE_HOLDS_INT(value) ||
                g_value_get_int(value) <= 0 ||
                static_cast<std::uint32_t>(g_value_get_int(value)) != spec.dimensions_[axis]) {
                return {status_code::protocol_error, "tensor dimension mismatch"};
            }
        }
        if (bytes > _max_bytes - total) {
            return {status_code::resource_exhausted, "combined tensors exceed output byte budget"};
        }
        auto* memory = gst_buffer_peek_memory(buffer, static_cast<guint>(index));
        if (memory == nullptr || gst_memory_get_sizes(memory, nullptr, nullptr) != bytes) {
            return {status_code::protocol_error, "tensor memory size differs from packed shape"};
        }
        total += bytes;
        sizes[index] = static_cast<std::size_t>(bytes);
    }
    tensor_result result;
    result.pipeline_pts_ns_ = GST_BUFFER_PTS(buffer);
    result.tensors_.reserve(_expected.size());
    for (std::size_t index = 0; index < _expected.size(); ++index) {
        memory_mapping mapping;
        mapping.memory_ = gst_buffer_peek_memory(buffer, static_cast<guint>(index));
        mapping.mapped_ = gst_memory_map(mapping.memory_, &mapping.map_, GST_MAP_READ);
        if (!mapping.mapped_ || mapping.map_.data == nullptr ||
            mapping.map_.size != sizes[index]) {
            return {status_code::io_error, "cannot map exact tensor view for CPU read"};
        }
        tensor_blob tensor;
        tensor.spec_ = _expected[index];
        try {
            tensor.bytes_.resize(sizes[index]);
        } catch (const std::bad_alloc&) {
            return {status_code::resource_exhausted, "tensor output allocation failed"};
        }
        std::memcpy(tensor.bytes_.data(), mapping.map_.data, sizes[index]);
        result.tensors_.push_back(std::move(tensor));
    }
    _result = std::move(result);
    return {};
}

}  // namespace vqec::vision::ai
