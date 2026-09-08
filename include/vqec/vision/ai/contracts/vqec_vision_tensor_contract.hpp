#ifndef VQEC_VISION_AI_CONTRACTS_TENSOR_CONTRACT_HPP
#define VQEC_VISION_AI_CONTRACTS_TENSOR_CONTRACT_HPP

#include <cstdint>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"

namespace vqec::vision::ai {

// Ordered packed FLOAT32 outputs only. Pure validation, no artifact or SDK inspection.
// Success writes total payload bytes; failure preserves _required_bytes unchanged.
[[nodiscard]] status vqec_vision_ai_core_tnctr_validate_outputs(
    const std::vector<float_tensor_spec>& _outputs, std::uint64_t _max_output_bytes,
    std::uint64_t& _required_bytes);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_TENSOR_CONTRACT_HPP
