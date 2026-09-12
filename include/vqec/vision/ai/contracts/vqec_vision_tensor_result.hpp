#ifndef VQEC_VISION_AI_CONTRACTS_TENSOR_RESULT_HPP
#define VQEC_VISION_AI_CONTRACTS_TENSOR_RESULT_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace vqec::vision::ai {

// Element types the reviewed Qualcomm ML caps/QNN wrapper can carry. This is the neutral
// superset; a backend (e.g. the installed plugin) may still report only a subset, and the
// adapter must reject a model contract it cannot actually satisfy.
enum class tensor_element_type {
    unknown,
    int8,
    uint8,
    int16,
    uint16,
    int32,
    uint32,
    int64,
    uint64,
    float16,
    float32
};

// Explicit affine quantization for integer tensors. The convention is stated, not assumed:
// real = (stored - zero_point) * scale. Floating tensors must not be marked quantized.
struct tensor_quantization {
    bool is_quantized_{false};
    float scale_{0.0F};
    std::int32_t zero_point_{0};
};

// Logical packed tensor identity: name, row-major shape, element type and quantization.
// Shapes are logical dimensions, not strides; the extractor rejects padding reinterprets.
struct tensor_spec {
    std::string name_;
    std::vector<std::uint32_t> dimensions_;
    tensor_element_type dtype_{tensor_element_type::float32};
    tensor_quantization quantization_;
};

// Owned packed little-endian element bytes in row-major order. Consumers must interpret
// bytes using spec_.dtype_/quantization_; no implicit cast to float is performed or assumed.
struct tensor_blob {
    tensor_spec spec_;
    std::vector<std::uint8_t> bytes_;
};

struct tensor_result {
    std::uint64_t pipeline_pts_ns_{UINT64_MAX};
    std::vector<tensor_blob> tensors_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_TENSOR_RESULT_HPP
