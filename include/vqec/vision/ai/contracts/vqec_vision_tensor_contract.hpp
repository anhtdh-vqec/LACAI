#ifndef VQEC_VISION_AI_CONTRACTS_TENSOR_CONTRACT_HPP
#define VQEC_VISION_AI_CONTRACTS_TENSOR_CONTRACT_HPP

#include <cstdint>
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

// Ordered packed FLOAT32 outputs only. Pure validation, no artifact or SDK inspection.
// Success writes total payload bytes; failure preserves _required_bytes unchanged.
[[nodiscard]] status vqec_vision_ai_core_tnctr_validate_outputs(
    const std::vector<float_tensor_spec>& _outputs, std::uint64_t _max_output_bytes,
    std::uint64_t& _required_bytes);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_TENSOR_CONTRACT_HPP
