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
    source_profile_config profile_{};
    source_memory_config memory_{};
    source_cascade_config cascade_{};
    std::uint64_t frame_pool_bytes_{0};
    std::uint64_t encoder_pool_bytes_{0};
    std::uint32_t estimated_ddr_mbps_{0};
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

struct admission_resource_breakdown {
    std::uint64_t frame_pool_bytes_{0};
    std::uint64_t tensor_pool_bytes_{0};
    std::uint64_t encoder_pool_bytes_{0};
    std::uint64_t cascade_roi_bytes_{0};
    std::uint64_t total_memory_bytes_{0};
    std::uint32_t estimated_ddr_bandwidth_mbps_{0};
    std::uint16_t fw_concurrency_slots_{0};
    std::uint16_t worker_concurrency_{0};
    std::uint16_t thermal_headroom_pct_{100};
};

struct hardware_admission_profile {
    std::uint64_t max_total_resident_bytes_{4096ULL * 1024ULL * 1024ULL};
    std::uint64_t max_frame_pool_bytes_{1024ULL * 1024ULL * 1024ULL};
    std::uint64_t max_tensor_pool_bytes_{1024ULL * 1024ULL * 1024ULL};
    std::uint64_t max_encoder_pool_bytes_{512ULL * 1024ULL * 1024ULL};
    std::uint64_t max_cascade_roi_bytes_{512ULL * 1024ULL * 1024ULL};
    std::uint32_t max_ddr_bandwidth_mbps_{12000};
    std::uint16_t max_fw_concurrency_slots_{16};
    std::uint16_t max_worker_concurrency_{64};
    std::uint16_t min_thermal_headroom_pct_{10};

    [[nodiscard]] bool is_valid() const noexcept {
        return max_total_resident_bytes_ > 0 &&
               max_frame_pool_bytes_ > 0 &&
               max_tensor_pool_bytes_ > 0 &&
               max_encoder_pool_bytes_ > 0 &&
               max_cascade_roi_bytes_ > 0 &&
               max_ddr_bandwidth_mbps_ > 0 &&
               max_fw_concurrency_slots_ > 0 &&
               max_worker_concurrency_ > 0 &&
               min_thermal_headroom_pct_ <= 100;
    }
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
    admission_resource_breakdown resources_{};
    hardware_admission_profile hardware_profile_{};
    std::array<activation_source_slot, deployment_limits::g_max_sources> sources_{};
    std::array<activation_model_slot, model_catalog_limits::g_max_models> models_{};
};

[[nodiscard]] hardware_admission_profile
vqec_vision_ai_admis_actsp_get_default_hardware_profile() noexcept;

// Cold-path build; performs complete deployment/catalog cross-validation first.
// Evaluates resource envelope against hardware limits (pool, queue, encoder, DDR, thermal, FW load).
[[nodiscard]] status vqec_vision_ai_admis_actsp_build_snapshot(
    const deployment_config& _deployment, const model_catalog& _catalog,
    activation_snapshot& _snapshot);

[[nodiscard]] status vqec_vision_ai_admis_actsp_build_snapshot(
    const deployment_config& _deployment, const model_catalog& _catalog,
    const hardware_admission_profile& _hardware_profile,
    activation_snapshot& _snapshot);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_RUNTIME_ADMISSION_ACTIVATION_SNAPSHOT_HPP
