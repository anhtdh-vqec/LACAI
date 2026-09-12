#ifndef VQEC_VISION_AI_DETEC_TENSOR_READER_HPP
#define VQEC_VISION_AI_DETEC_TENSOR_READER_HPP

#include <cstddef>
#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace tensor_reader_limits {
inline constexpr std::size_t g_max_rank = 8;
}  // namespace tensor_reader_limits

[[nodiscard]] status vqec_vision_ai_detec_tnrd_find_tensor(
    const tensor_result& _result, const std::string& _name,
    const tensor_blob*& _tensor) noexcept;

// Confirms the blob identity (name/shape/dtype) and packed byte count match the manifest.
// It does not reinterpret or cast element values; consumers use spec_.dtype_/quantization_.
[[nodiscard]] status vqec_vision_ai_detec_tnrd_validate_tensor(
    const tensor_blob& _tensor, const tensor_spec& _expected) noexcept;

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_DETEC_TENSOR_READER_HPP
