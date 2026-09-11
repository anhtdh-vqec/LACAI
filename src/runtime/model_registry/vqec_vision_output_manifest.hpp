#ifndef VQEC_VISION_AI_MODEL_REGISTRY_OUTPUT_MANIFEST_HPP
#define VQEC_VISION_AI_MODEL_REGISTRY_OUTPUT_MANIFEST_HPP

#include <istream>

#include "vqec/vision/ai/contracts/vqec_vision_model_outputs.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

// Consumes <=65537 bytes, bounded JSON parsing. Error preserves destination,
// including allocation failure reported as resource_exhausted.
// Caller authorizes/opens stream; parse success is not artifact authentication.
// Stream I/O may block. No C ABI callbacks exposed.
[[nodiscard]] status vqec_vision_ai_mreg_otman_load_manifest(
    std::istream& _stream, model_outputs& _manifest);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_MODEL_REGISTRY_OUTPUT_MANIFEST_HPP
