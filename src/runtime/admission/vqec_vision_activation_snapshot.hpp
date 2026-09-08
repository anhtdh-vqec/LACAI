#ifndef VQEC_VISION_AI_RUNTIME_ADMISSION_ACTIVATION_SNAPSHOT_HPP
#define VQEC_VISION_AI_RUNTIME_ADMISSION_ACTIVATION_SNAPSHOT_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_model_catalog.hpp"

namespace vqec::vision::ai {

struct activation_source_slot {
    std::uint16_t deployment_index_{0};
    std::uint32_t camera_id_{0};
    std::uint32_t channel_id_{0};
    source_profile_config profile_;
    source_memory_config memory_;
    std::uint16_t model_count_{0};
    std::array<std::uint16_t, deployment_limits::g_max_models_per_source>
        catalog_model_indices_{};
};

struct activation_model_slot {
    std::uint16_t catalog_index_{0};
    std::uint16_t assignment_count_{0};
    std::uint16_t context_instance_count_{0};
    std::uint64_t resident_bytes_{0};
};

// Fixed-capacity, allocation-free hot-path lookup. Indices are valid only for the exact
// immutable deployment/catalog revisions recorded here.
struct activation_snapshot {
    std::uint64_t deployment_revision_{0};
    std::uint64_t catalog_revision_{0};
    std::uint64_t total_resident_bytes_{0};
    std::uint64_t required_model_resident_bytes_{0};
    std::uint16_t source_count_{0};
    std::uint16_t active_model_count_{0};
    std::array<activation_source_slot, deployment_limits::g_max_sources> sources_{};
    std::array<activation_model_slot, model_catalog_limits::g_max_models> models_{};
};

// Cold-path build; performs complete deployment/catalog cross-validation first.
// No dynamic allocation is performed by snapshot materialization. Failure preserves output.
[[nodiscard]] status vqec_vision_ai_admis_actsp_build_snapshot(
    const deployment_config& _deployment, const model_catalog& _catalog,
    activation_snapshot& _snapshot);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_RUNTIME_ADMISSION_ACTIVATION_SNAPSHOT_HPP
