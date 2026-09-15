#ifndef VQEC_VISION_AI_CONTRACTS_COLOR_HPP
#define VQEC_VISION_AI_CONTRACTS_COLOR_HPP

#include <array>
#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_image_enums.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preprocess_spec.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"

namespace vqec::vision::ai {

// Reviewed neutral NV12 (semi-planar, Cb then Cr) to packed RGB/BGR 8-bit conversion with an
// explicit matrix/range/channel-order policy. Deterministic, no vendor type and no OpenCV.
// The caller owns both buffers; it does not allocate or import memory. Chroma is read at
// 2x2 subsampling. Returns invalid_argument for null buffers, non-even dimensions or a
// stride smaller than the row, and unsupported for an unspecified matrix/range.
[[nodiscard]] status vqec_vision_ai_core_color_convert_nv12_to_rgb(
    const std::uint8_t* _y_plane, std::uint32_t _y_stride,
    const std::uint8_t* _uv_plane, std::uint32_t _uv_stride,
    std::uint32_t _width, std::uint32_t _height,
    color_matrix _matrix, color_range _range, channel_order _order,
    std::uint8_t* _rgb, std::uint32_t _rgb_stride) noexcept;

// Packed RGB/BGR 8-bit (already in the model's channel order) to the exact quantized uint16
// model input tensor (NHWC, tightly packed). Each channel applies the preprocess_spec
// offset_scale contract: (value - offset[c]) * scale[c], then the model quantization
// stored = round(real / quant_scale) + quant_zero_point, clamped to uint16. Deterministic
// and reviewed; the caller owns both buffers.
[[nodiscard]] status vqec_vision_ai_core_color_quantize_rgb8_to_uint16(
    const std::uint8_t* _rgb, std::uint32_t _width, std::uint32_t _height,
    std::uint32_t _rgb_stride,
    const std::array<float, 3>& _offset, const std::array<float, 3>& _scale,
    float _quant_scale, std::int32_t _quant_zero_point,
    std::uint16_t* _output) noexcept;

// Proves that applying an offset_scale preprocess followed by the target quantization maps
// every possible RGB8 channel value to within one quantized LSB of the direct integer output
// produced by the Qualcomm converter path: identity for uint8 or full-range widening for
// uint16. The one-LSB allowance covers the boundary between the model's affine rounding and
// integer widening. This is a pure, exhaustive 256-value check; success does not claim
// device execution or golden parity.
[[nodiscard]] status vqec_vision_ai_core_color_validate_direct_integer_mapping(
    const preprocess_spec& _preprocess, const tensor_spec& _target) noexcept;

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_COLOR_HPP
