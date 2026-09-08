#ifndef VQEC_VISION_AI_CONTRACTS_MODEL_CATALOG_HPP
#define VQEC_VISION_AI_CONTRACTS_MODEL_CATALOG_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_deployment_config.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_model_outputs.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace model_catalog_limits {
inline constexpr std::uint32_t g_schema_version = 1;
inline constexpr std::size_t g_max_models = 64;
inline constexpr std::size_t g_max_identifier_bytes = 128;
inline constexpr std::uint64_t g_max_model_resident_bytes = 2ULL * 1024 * 1024 * 1024;
inline constexpr std::uint64_t g_max_catalog_resident_bytes = 8ULL * 1024 * 1024 * 1024;
}  // namespace model_catalog_limits

struct model_source_constraints {
    std::uint32_t min_width_{0};
    std::uint32_t min_height_{0};
    std::uint32_t max_width_{0};
    std::uint32_t max_height_{0};
    std::uint32_t min_fps_numerator_{0};
    std::uint32_t min_fps_denominator_{0};
};

struct model_resource_profile {
    std::uint64_t resident_bytes_{0};
    std::uint64_t max_tensor_bytes_per_source_{0};
    std::uint32_t output_queue_buffers_{0};
    unsigned max_concurrent_sources_{0};
    bool can_share_context_across_sources_{false};
};

struct model_catalog_entry {
    std::string model_id_;
    std::string model_version_;
    std::string target_id_;
    std::string artifact_ref_;
    std::string artifact_sha256_;
    std::string output_manifest_ref_;
    std::string decoder_contract_;
    std::string preprocess_contract_;
    std::string graph_name_;
    std::uint32_t tensor_width_{0};
    std::uint32_t tensor_height_{0};
    tensor_type input_type_{tensor_type::uint8};
    channel_order channel_order_{channel_order::rgb};
    image_placement placement_{image_placement::unspecified};
    std::array<double, 3> mean_{0.0, 0.0, 0.0};
    std::array<double, 3> sigma_{1.0, 1.0, 1.0};
    std::uint32_t inference_fps_numerator_{0};
    std::uint32_t inference_fps_denominator_{0};
    model_source_constraints source_constraints_;
    model_resource_profile resources_;
};

struct model_catalog {
    std::uint32_t schema_version_{0};
    std::uint64_t revision_{0};
    std::string catalog_id_;
    std::vector<model_catalog_entry> models_;
};

// Result of a trusted platform resolver. References prevent accidentally binding paths
// resolved for a different catalog entry. This does not itself authenticate any path.
struct resolved_model_paths {
    std::string model_id_;
    std::string target_id_;
    std::string artifact_ref_;
    std::string model_path_;
    std::string backend_path_;
    std::string system_path_;
};

// Cold-path pure validators. Output byte counts are transactional.
[[nodiscard]] status vqec_vision_ai_core_mdcat_validate_catalog(
    const model_catalog& _catalog, std::uint64_t& _declared_resident_bytes);
[[nodiscard]] status vqec_vision_ai_core_mdcat_validate_deployment_models(
    const deployment_config& _deployment, const model_catalog& _catalog,
    std::uint64_t& _required_model_resident_bytes);
[[nodiscard]] status vqec_vision_ai_core_mdcat_validate_model_outputs(
    const model_catalog_entry& _model, const std::string& _resolved_manifest_ref,
    const model_outputs& _outputs, std::uint64_t& _required_output_bytes);

// Builds the current single-image Qualcomm plan from three distinct authorities:
// source profile/budget, Model-team catalog entry, and trusted platform path resolver.
// Failure preserves _plan.
[[nodiscard]] status vqec_vision_ai_core_mdcat_compose_inference_plan(
    const source_deployment_config& _source, const model_catalog_entry& _model,
    const resolved_model_paths& _paths, inference_plan& _plan);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_MODEL_CATALOG_HPP
