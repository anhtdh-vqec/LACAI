#ifndef VQEC_VISION_AI_CONTRACTS_MODEL_IO_MANIFEST_HPP
#define VQEC_VISION_AI_CONTRACTS_MODEL_IO_MANIFEST_HPP

#include <vector>

#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/inference/vqec_vision_tensor_result.hpp"

namespace vqec::vision::ai {

// Symmetric, package-declared model IO identity. The output side already existed; the input
// side makes a package assert the exact input tensor it expects so activation can compare it
// against the actual graph metadata and fail closed on any mismatch.
struct model_io_manifest {
    std::vector<tensor_spec> inputs_;
    std::vector<tensor_spec> outputs_;
};

// Structural validation: non-empty input/output sets, known dtype, nonzero rank and extent,
// positive element/shape bytes, no duplicate names, and a valid quantization pair.
[[nodiscard]] status vqec_vision_ai_core_ioman_validate(
    const model_io_manifest& _manifest) noexcept;

// Exact identity: name, dimensions, dtype, layout and quantization.
[[nodiscard]] bool vqec_vision_ai_core_ioman_same_spec(
    const tensor_spec& _declared, const tensor_spec& _actual) noexcept;

// Fail-closed comparison of a declared manifest against actual graph metadata. Returns
// unsupported with a reason on any count, identity or layout difference.
[[nodiscard]] status vqec_vision_ai_core_ioman_matches(
    const model_io_manifest& _declared, const model_io_manifest& _actual);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_MODEL_IO_MANIFEST_HPP
