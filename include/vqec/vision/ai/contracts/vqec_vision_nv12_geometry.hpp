#ifndef VQEC_VISION_AI_CONTRACTS_NV12_GEOMETRY_HPP
#define VQEC_VISION_AI_CONTRACTS_NV12_GEOMETRY_HPP

#include <cstdint>

namespace vqec::vision::ai {

// Shared packed-NV12 geometry rules. Color conversion, preview surfaces, encoder input,
// deployment budgets and inference plans must agree on the same even-dimension rule and
// byte formula, so the definition lives here instead of being re-derived per module.
//
// A valid packed NV12 frame has non-zero even width and height. The packed size is
// width*height*3/2 bytes: a full-size Y plane plus one interleaved half-size UV plane.
[[nodiscard]] constexpr bool vqec_vision_ai_cntr_nvgeo_is_even_nonzero(
    std::uint32_t _width, std::uint32_t _height) noexcept {
    return _width != 0 && _height != 0 && _width % 2U == 0 && _height % 2U == 0;
}

// Returns 0 for invalid geometry; callers must reject that before allocating or indexing.
// The product fits std::uint64_t for any std::uint32_t width/height.
[[nodiscard]] constexpr std::uint64_t vqec_vision_ai_cntr_nvgeo_packed_bytes(
    std::uint32_t _width, std::uint32_t _height) noexcept {
    if (!vqec_vision_ai_cntr_nvgeo_is_even_nonzero(_width, _height)) {
        return 0;
    }
    const std::uint64_t pixels = static_cast<std::uint64_t>(_width) * _height;
    return pixels + pixels / 2U;
}

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_NV12_GEOMETRY_HPP
