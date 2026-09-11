#ifndef VQEC_VISION_AI_MREG_ARTIFACT_RESOLVER_HPP
#define VQEC_VISION_AI_MREG_ARTIFACT_RESOLVER_HPP

#include <cstdint>
#include <string>

#include "vqec_vision_artifact_digest.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_model_catalog.hpp"

namespace vqec::vision::ai {

// Trusted platform policy: absolute roots and library paths, plus a byte ceiling. These
// come from deployment/platform configuration, never from a remote or model-supplied
// document. The resolver itself does not prove authenticity; it enforces containment,
// immutability assumptions and the catalog digest before any SDK load.
struct artifact_resolver_config {
    std::string model_root_;
    std::string backend_library_;
    std::string system_library_;
    std::uint64_t max_artifact_bytes_{0};
};

// Resolves a trusted relative artifact path (the mapping from model.artifact_ref_) under
// model_root_, rejecting traversal, symlink escape, non-regular files, out-of-bound sizes
// and digest mismatch. On success _paths is filled from the catalog entry and the trusted
// libraries; on any failure _paths is unchanged. The returned model_path_ is the canonical
// path of the opened inode; no lock is held after return.
[[nodiscard]] status vqec_vision_ai_mreg_artsr_resolve_model(
    const model_catalog_entry& _model, const std::string& _artifact_relative_path,
    const artifact_resolver_config& _config, resolved_model_paths& _paths);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_MREG_ARTIFACT_RESOLVER_HPP
