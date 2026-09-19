#ifndef VQEC_VISION_AI_MODEL_REGISTRY_DECODER_PACKAGE_HPP
#define VQEC_VISION_AI_MODEL_REGISTRY_DECODER_PACKAGE_HPP

#include <cstddef>
#include <istream>

#include "vqec/vision/ai/contracts/inference/vqec_vision_decoder_package.hpp"
#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace decoder_package_document_limits {
inline constexpr std::size_t g_max_document_bytes = 64U * 1024U;
inline constexpr int g_max_json_depth = 8;
}  // namespace decoder_package_document_limits

// Startup-only strict JSON load of a model package decoder.json. Every policy field this
// loader consumes is required and range-checked; unknown keys and implicit defaults are
// rejected. The caller authenticates and opens the document; this is metadata validation
// only. Failure preserves _package.
[[nodiscard]] status vqec_vision_ai_mreg_dcpkg_load(
    std::istream& _stream, decoder_package& _package);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_MODEL_REGISTRY_DECODER_PACKAGE_HPP
