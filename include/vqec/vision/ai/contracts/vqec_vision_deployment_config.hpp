#ifndef VQEC_VISION_AI_CONTRACTS_DEPLOYMENT_CONFIG_HPP
#define VQEC_VISION_AI_CONTRACTS_DEPLOYMENT_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

struct inference_plan;

namespace deployment_limits {
inline constexpr std::uint32_t g_schema_version = 1;
inline constexpr std::size_t g_max_sources = 16;
inline constexpr std::size_t g_max_models_per_source = 16;
inline constexpr std::size_t g_max_identifier_bytes = 128;
inline constexpr std::uint32_t g_max_dimension_pixels = 8192;
inline constexpr std::uint32_t g_max_frames_per_second = 240;
inline constexpr unsigned g_max_inflight_frames_per_source = 4;
inline constexpr unsigned g_max_preview_surfaces_per_source = 4;
inline constexpr std::uint64_t g_max_frame_allocation_bytes = 256ULL * 1024 * 1024;
inline constexpr std::uint64_t g_max_tensor_bytes_per_source = 64ULL * 1024 * 1024;
inline constexpr std::uint64_t g_max_temporal_bytes_per_source = 256ULL * 1024 * 1024;
inline constexpr std::uint64_t g_max_total_resident_bytes = 8ULL * 1024 * 1024 * 1024;
}  // namespace deployment_limits

struct source_profile_config {
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::uint32_t fps_numerator_{0};
    std::uint32_t fps_denominator_{0};
};

struct source_memory_config {
    std::uint64_t max_frame_allocation_bytes_{0};
    unsigned max_inflight_frames_{0};
    unsigned preview_surface_count_{0};
    std::uint64_t max_tensor_bytes_{0};
    std::uint64_t max_temporal_bytes_{0};
};

struct source_deployment_config {
    std::string source_id_;
    // Mandatory FW-owned RAW source registry identity. Camera/Box transport is opaque.
    std::string raw_source_ref_;
    // Logical identity preserved in events/preview on both AI Camera and AI Box.
    std::uint32_t camera_id_{0};
    std::uint32_t channel_id_{0};
    // Required only when preview_surface_count > 0; resolved by the FW output adapter.
    std::string preview_output_ref_;
    source_profile_config profile_;
    source_memory_config memory_;
    std::vector<std::string> model_ids_;
};

struct deployment_config {
    std::uint32_t schema_version_{0};
    std::uint64_t revision_{0};
    // Reference to the separately authenticated AI Model team catalog/configuration.
    std::string model_catalog_ref_;
    std::uint64_t max_total_resident_bytes_{0};
    std::uint64_t max_model_resident_bytes_{0};
    std::vector<source_deployment_config> sources_;
};

// Cold-path validation. Computes conservative declared resident bytes transactionally.
// Does not allocate pools, resolve RAW sources/catalogs, authenticate config or admit a board.
[[nodiscard]] status vqec_vision_ai_core_dpval_validate_deployment(
    const deployment_config& _config, std::uint64_t& _declared_resident_bytes);

// Cold-path composition after deployment validation and model-plan resolution.
// Source geometry/rate has one authority: _source. Failure preserves _plan.
[[nodiscard]] status vqec_vision_ai_core_dpval_bind_source_profile(
    const source_deployment_config& _source, inference_plan& _plan);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_DEPLOYMENT_CONFIG_HPP
