#ifndef VQEC_VISION_AI_CONTRACTS_COLOR_HPP
#define VQEC_VISION_AI_CONTRACTS_COLOR_HPP

#include <array>
#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_image_enums.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preprocess_spec.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

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
// model input tensor (NHWC, tightly packed). Each channel applies value * scale[c] +
// offset[c], then the model quantization stored = round(real / quant_scale) +
// quant_zero_point, clamped to uint16. Deterministic and reviewed; the caller owns both
// buffers.
[[nodiscard]] status vqec_vision_ai_core_color_quantize_rgb8_to_uint16(
    const std::uint8_t* _rgb, std::uint32_t _width, std::uint32_t _height,
    std::uint32_t _rgb_stride,
    const std::array<float, 3>& _offset, const std::array<float, 3>& _scale,
    float _quant_scale, std::int32_t _quant_zero_point,
    std::uint16_t* _output) noexcept;

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_COLOR_HPP
