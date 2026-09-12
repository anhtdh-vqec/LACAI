#ifndef VQEC_VISION_AI_CONTRACTS_TENSOR_CONTRACT_HPP
#define VQEC_VISION_AI_CONTRACTS_TENSOR_CONTRACT_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"

namespace vqec::vision::ai {

namespace tensor_contract_limits {
inline constexpr std::size_t g_max_outputs = 16;
inline constexpr std::uint64_t g_max_output_bytes = 64ULL * 1024ULL * 1024ULL;
inline constexpr std::size_t g_max_name_bytes = 128;
inline constexpr std::size_t g_max_rank = 8;
}  // namespace tensor_contract_limits

// Element size in bytes for a known dtype, or 0 for tensor_element_type::unknown.
[[nodiscard]] std::size_t
vqec_vision_ai_core_tnctr_element_size(tensor_element_type _type) noexcept;

// Canonical lowercase contract spelling, e.g. "float32", "int8". Never null.
[[nodiscard]] const char*
vqec_vision_ai_core_tnctr_element_type_name(tensor_element_type _type) noexcept;

// Parses a canonical lowercase spelling; returns tensor_element_type::unknown when the
// name is not one of the reviewed types. One mapping shared by every loader.
[[nodiscard]] tensor_element_type
vqec_vision_ai_core_tnctr_element_type_from_name(const std::string& _name) noexcept;

// Packed row-major byte count for a spec, or 0 for an invalid dtype/rank/shape or
// multiplication overflow. Does not allocate.
[[nodiscard]] std::uint64_t
vqec_vision_ai_core_tnctr_shape_bytes(const tensor_spec& _spec) noexcept;

// Ordered output metadata only. Pure validation, no artifact or SDK inspection.
// Accepts every supported element type; enforces quantization conventions. Success writes
// total packed payload bytes; failure preserves _required_bytes unchanged.
[[nodiscard]] status vqec_vision_ai_core_tnctr_validate_outputs(
    const std::vector<tensor_spec>& _outputs, std::uint64_t _max_output_bytes,
    std::uint64_t& _required_bytes);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_TENSOR_CONTRACT_HPP
