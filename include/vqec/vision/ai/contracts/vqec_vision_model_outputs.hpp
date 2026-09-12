#ifndef VQEC_VISION_AI_CONTRACTS_MODEL_OUTPUTS_HPP
#define VQEC_VISION_AI_CONTRACTS_MODEL_OUTPUTS_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"

namespace vqec::vision::ai {

// Parsed metadata, NOT authenticated. A claimed digest is not verification evidence.
struct model_outputs {
    std::string model_id_;
    std::string model_version_;
    std::string artifact_sha256_;
    std::string decoder_contract_;
    std::uint64_t max_output_bytes_{0};
    std::vector<tensor_spec> outputs_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_MODEL_OUTPUTS_HPP
