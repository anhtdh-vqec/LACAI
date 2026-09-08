#include "vqec_vision_tensor_output.hpp"

#include <array>
#include <cstring>
#include <limits>
#include <utility>

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

status vqec_vision_ai_qcom_tnout_copy_sample(
    GstSample* _sample, const std::vector<float_tensor_spec>& _expected,
    std::uint64_t _max_bytes, tensor_result& _result) {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
                  "QNN FLOAT32 extraction requires IEEE binary32");
    if (_sample == nullptr || _expected.empty() || _expected.size() > 16 ||
        _max_bytes == 0 || _max_bytes > 64ULL * 1024 * 1024) {
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
    if (!gst_structure_has_name(structure, "neural-network/tensors") || type == nullptr ||
        std::strcmp(type, "FLOAT32") != 0 || dimensions == nullptr ||
        !GST_VALUE_HOLDS_ARRAY(dimensions) ||
        gst_value_array_get_size(dimensions) != _expected.size() ||
        gst_buffer_n_memory(buffer) != _expected.size()) {
        return {status_code::unsupported, "requires FLOAT32 tensors, one memory per tensor"};
    }
    std::array<std::size_t, 16> sizes{};
    std::uint64_t total = 0;
    for (std::size_t index = 0; index < _expected.size(); ++index) {
        const auto& spec = _expected[index];
        if (spec.name_.empty() || spec.name_.size() > 128 ||
            spec.name_.find('\0') != std::string::npos || spec.dimensions_.empty() ||
            spec.dimensions_.size() > 8) {
            return {status_code::invalid_argument, "invalid tensor name or rank in model contract"};
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (_expected[previous].name_ == spec.name_) {
                return {status_code::invalid_argument, "duplicate tensor name in model contract"};
            }
        }
        const auto* shape = gst_value_array_get_value(dimensions, static_cast<guint>(index));
        if (shape == nullptr || !GST_VALUE_HOLDS_ARRAY(shape) ||
            gst_value_array_get_size(shape) != spec.dimensions_.size()) {
            return {status_code::protocol_error, "tensor rank does not match model contract"};
        }
        std::uint64_t bytes = sizeof(float);
        for (std::size_t axis = 0; axis < spec.dimensions_.size(); ++axis) {
            const auto size = spec.dimensions_[axis];
            const auto* value = gst_value_array_get_value(shape, static_cast<guint>(axis));
            if (size == 0 || value == nullptr || !G_VALUE_HOLDS_INT(value) ||
                g_value_get_int(value) <= 0 ||
                static_cast<std::uint32_t>(g_value_get_int(value)) != size) {
                return {status_code::protocol_error, "tensor dimension mismatch"};
            }
            if (bytes > _max_bytes / size) {
                return {status_code::resource_exhausted, "tensor exceeds output byte budget"};
            }
            bytes *= size;
        }
        if (bytes > _max_bytes - total) {
            return {status_code::resource_exhausted, "combined tensors exceed output byte budget"};
        }
        total += bytes;
        auto* memory = gst_buffer_peek_memory(buffer, static_cast<guint>(index));
        if (memory == nullptr || gst_memory_get_sizes(memory, nullptr, nullptr) != bytes) {
            return {status_code::protocol_error, "tensor memory size differs from packed shape"};
        }
        sizes[index] = static_cast<std::size_t>(bytes);
    }
    tensor_result result;
    result.pipeline_pts_ns_ = GST_BUFFER_PTS(buffer);
    result.tensors_.reserve(_expected.size());
    for (std::size_t index = 0; index < _expected.size(); ++index) {
        memory_mapping mapping;
        mapping.memory_ = gst_buffer_peek_memory(buffer, static_cast<guint>(index));
        mapping.mapped_ = gst_memory_map(mapping.memory_, &mapping.map_, GST_MAP_READ);
        if (!mapping.mapped_ || mapping.map_.data == nullptr || mapping.map_.size != sizes[index]) {
            return {status_code::io_error, "cannot map exact tensor view for CPU read"};
        }
        float_tensor_result tensor;
        tensor.spec_ = _expected[index];
        tensor.values_.resize(sizes[index] / sizeof(float));
        std::memcpy(tensor.values_.data(), mapping.map_.data, sizes[index]);
        result.tensors_.push_back(std::move(tensor));
    }
    _result = std::move(result);
    return {};
}

}  // namespace vqec::vision::ai
