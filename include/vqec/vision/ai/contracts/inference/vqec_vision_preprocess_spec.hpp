#ifndef VQEC_VISION_AI_CONTRACTS_PREPROCESS_SPEC_HPP
#define VQEC_VISION_AI_CONTRACTS_PREPROCESS_SPEC_HPP

#include <array>
#include <cstdint>

#include "vqec/vision/ai/contracts/media/vqec_vision_image_enums.hpp"
#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"

namespace vqec::vision::ai {

// Source pixel format the preprocess stage consumes.
enum class source_pixel_format { unknown, nv12, rgb888, bgr888, rgba8888 };

// Colour matrix and quantization range. These must be data: a BT.709 source converted with
// BT.601 coefficients (or vice versa) silently degrades accuracy without any execution error.
enum class color_matrix { unspecified, bt601, bt709, bt2020 };
enum class color_range { unspecified, limited, full };

enum class resize_mode { stretch, letterbox, crop };
enum class interpolation_mode { nearest, bilinear, area };

// How the per-channel affine is defined. `offset_scale` is real = (pixel - offset) * scale.
// `mean_std` is real = (pixel - mean) / std, carried in the same arrays with an explicit
// formula so `scale` is never read as a standard deviation by mistake.
enum class normalization_formula { offset_scale, mean_std, none };

// Coordinate meaning of a decoded box relative to the input tensor.
enum class coordinate_convention {
    tensor_pixels_xywh,
    tensor_pixels_xyxy,
    normalized_xywh,
    normalized_xyxy
};

// Authoritative, fully explicit preprocess contract. Every assumption an accelerator
// implementation would otherwise hardcode lives here. It is startup metadata; validation is
// pure and performs no allocation, I/O or vendor call.
struct preprocess_spec {
    source_pixel_format source_format_{source_pixel_format::unknown};
    color_matrix matrix_{color_matrix::unspecified};
    color_range range_{color_range::unspecified};
    resize_mode resize_{resize_mode::letterbox};
    interpolation_mode interpolation_{interpolation_mode::bilinear};
    image_placement placement_{image_placement::centre};
    // Letterbox/pad fill in source 0..255 units.
    std::array<float, 3> pad_value_{0.0F, 0.0F, 0.0F};
    channel_order channels_{channel_order::rgb};
    normalization_formula normalization_{normalization_formula::offset_scale};
    // Used by offset_scale and mean_std; see normalization_formula.
    std::array<float, 3> offset_{0.0F, 0.0F, 0.0F};
    std::array<float, 3> scale_{1.0F, 1.0F, 1.0F};
    coordinate_convention coordinates_{coordinate_convention::tensor_pixels_xywh};
};

// Structural validation only: enum membership, finite pad/offset/scale, nonzero scale.
[[nodiscard]] status vqec_vision_ai_core_ppspc_validate(
    const preprocess_spec& _spec) noexcept;

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_PREPROCESS_SPEC_HPP
