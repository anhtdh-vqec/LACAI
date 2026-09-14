#ifndef VQEC_VISION_AI_CONTRACTS_MODEL_PACKAGE_HPP
#define VQEC_VISION_AI_CONTRACTS_MODEL_PACKAGE_HPP

#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_model_catalog.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_model_io_manifest.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preprocess_spec.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

// Inputs to resolve one model package. The catalog entry and the trusted-resolved paths are
// supplied by the caller (the trusted artifact resolver already enforced containment, size
// and digest); the IO manifest and preprocess spec are supplied by the package loaders.
struct model_package_inputs {
    model_catalog_entry model_;
    // Graph name from the package; must agree with the catalog entry when both are set.
    std::string graph_name_;
    // Trusted-resolved artifact/backend/system paths for this model.
    resolved_model_paths paths_;
    model_io_manifest io_;
    preprocess_spec preprocess_;
    std::vector<std::string> class_labels_;
};

// One fully resolved model package handed to the platform/model owner. It carries only
// neutral metadata and resolved paths; it holds no vendor type and loads nothing itself.
struct resolved_model_package {
    std::string model_id_;
    std::string model_version_;
    std::string target_id_;
    std::string graph_name_;
    std::string decoder_contract_;
    resolved_model_paths paths_;
    model_io_manifest io_;
    preprocess_spec preprocess_;
    std::vector<std::string> class_labels_;
};

// Pure composition/validation (no I/O, allocation beyond the output or vendor call):
//  - catalog identity: model_id/version/target_id/artifact_ref/artifact_sha256/decoder/graph;
//  - graph name agreement between the catalog entry and the package;
//  - IO manifest is valid and this base supports exactly one image input;
//  - preprocess spec is valid;
//  - resolved paths identity matches the catalog entry and all three paths are non-empty.
// On failure _package is unchanged.
[[nodiscard]] status vqec_vision_ai_core_mpkg_resolve(
    const model_package_inputs& _inputs, resolved_model_package& _package);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_MODEL_PACKAGE_HPP
