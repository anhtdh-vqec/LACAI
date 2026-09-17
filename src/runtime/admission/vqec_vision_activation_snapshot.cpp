#include "vqec_vision_activation_snapshot.hpp"

#include <algorithm>
#include <limits>

namespace vqec::vision::ai {
namespace {

std::size_t vqec_vision_ai_admis_actsp_find_model_index(
    const model_catalog& _catalog, const std::string& _model_id) noexcept {
    for (std::size_t index = 0; index < _catalog.models_.size(); ++index) {
        if (_catalog.models_[index].model_id_ == _model_id) {
            return index;
        }
    }
    return _catalog.models_.size();
}

}  // namespace

hardware_admission_profile
vqec_vision_ai_admis_actsp_get_default_hardware_profile() noexcept {
    hardware_admission_profile profile;
    profile.max_total_resident_bytes_ = 4096ULL * 1024ULL * 1024ULL;
    profile.max_frame_pool_bytes_ = 1024ULL * 1024ULL * 1024ULL;
    profile.max_tensor_pool_bytes_ = 1024ULL * 1024ULL * 1024ULL;
    profile.max_encoder_pool_bytes_ = 512ULL * 1024ULL * 1024ULL;
    profile.max_cascade_roi_bytes_ = 512ULL * 1024ULL * 1024ULL;
    profile.max_ddr_bandwidth_mbps_ = 12000;
    profile.max_fw_concurrency_slots_ = 16;
    profile.max_worker_concurrency_ = 64;
    profile.min_thermal_headroom_pct_ = 10;
    return profile;
}

status vqec_vision_ai_admis_actsp_build_snapshot(
    const deployment_config& _deployment, const model_catalog& _catalog,
    activation_snapshot& _snapshot) {
    return vqec_vision_ai_admis_actsp_build_snapshot(
        _deployment, _catalog,
        vqec_vision_ai_admis_actsp_get_default_hardware_profile(), _snapshot);
}

status vqec_vision_ai_admis_actsp_build_snapshot(
    const deployment_config& _deployment, const model_catalog& _catalog,
    const hardware_admission_profile& _hardware_profile,
    activation_snapshot& _snapshot) {
    if (!_hardware_profile.is_valid()) {
        return {status_code::unsupported, "measured hardware profile is missing or invalid"};
    }
    std::uint64_t declared_deployment_bytes = 0;
    const auto valid_deployment = vqec_vision_ai_core_dpval_validate_deployment(
        _deployment, declared_deployment_bytes);
    if (valid_deployment.code_ != status_code::ok) {
        return valid_deployment;
    }
    std::uint64_t required_model_bytes = 0;
    const auto valid_models = vqec_vision_ai_core_mdcat_validate_deployment_models(
        _deployment, _catalog, required_model_bytes);
    if (valid_models.code_ != status_code::ok) {
        return valid_models;
    }
    if (declared_deployment_bytes < _deployment.max_model_resident_bytes_ ||
        required_model_bytes > std::numeric_limits<std::uint64_t>::max() -
            (declared_deployment_bytes - _deployment.max_model_resident_bytes_)) {
        return {status_code::resource_exhausted, "activation memory arithmetic overflow"};
    }
    activation_snapshot candidate;
    candidate.deployment_revision_ = _deployment.revision_;
    candidate.catalog_revision_ = _catalog.revision_;
    candidate.required_model_resident_bytes_ = required_model_bytes;
    candidate.total_resident_bytes_ = declared_deployment_bytes -
        _deployment.max_model_resident_bytes_ + required_model_bytes;
    candidate.source_count_ = static_cast<std::uint16_t>(_deployment.sources_.size());
    candidate.hardware_profile_ = _hardware_profile;

    std::array<unsigned, model_catalog_limits::g_max_models> assignment_counts{};
    for (std::size_t source_index = 0;
         source_index < _deployment.sources_.size(); ++source_index) {
        const auto& source = _deployment.sources_[source_index];
        auto& slot = candidate.sources_[source_index];
        slot.deployment_index_ = static_cast<std::uint16_t>(source_index);
        slot.camera_id_ = source.camera_id_;
        slot.channel_id_ = source.channel_id_;
        slot.profile_ = source.profile_;
        slot.memory_ = source.memory_;
        slot.cascade_ = source.cascade_;
        slot.model_count_ = static_cast<std::uint16_t>(source.model_ids_.size());

        slot.frame_pool_bytes_ =
            source.memory_.max_frame_allocation_bytes_ * source.memory_.max_inflight_frames_;
        if (source.memory_.preview_surface_count_ > 0) {
            const std::uint64_t nv12_frame =
                (static_cast<std::uint64_t>(source.profile_.width_) *
                 source.profile_.height_ * 3ULL) / 2ULL;
            slot.encoder_pool_bytes_ =
                nv12_frame * source.memory_.preview_surface_count_;
        }
        const std::uint64_t fps = (source.profile_.fps_denominator_ > 0)
            ? (source.profile_.fps_numerator_ / source.profile_.fps_denominator_)
            : 25ULL;
        const std::uint64_t frame_bytes =
            (static_cast<std::uint64_t>(source.profile_.width_) *
             source.profile_.height_ * 3ULL) / 2ULL;
        const std::uint64_t ingress_bps =
            frame_bytes * fps * (1ULL + source.memory_.max_inflight_frames_);
        const std::uint64_t preview_bps =
            (source.memory_.preview_surface_count_ > 0) ? (frame_bytes * fps) : 0ULL;
        std::uint64_t total_bps = ingress_bps + preview_bps;

        for (std::size_t model_index = 0;
             model_index < source.model_ids_.size(); ++model_index) {
            const auto catalog_index = vqec_vision_ai_admis_actsp_find_model_index(
                _catalog, source.model_ids_[model_index]);
            if (catalog_index >= _catalog.models_.size()) {
                return {status_code::invalid_argument, "validated model index disappeared"};
            }
            slot.catalog_model_indices_[model_index] =
                static_cast<std::uint16_t>(catalog_index);
            ++assignment_counts[catalog_index];

            const auto& model = _catalog.models_[catalog_index];
            const std::uint64_t model_fps = (model.inference_fps_denominator_ > 0)
                ? (model.inference_fps_numerator_ / model.inference_fps_denominator_)
                : fps;
            const std::uint64_t tensor_bytes =
                static_cast<std::uint64_t>(model.tensor_width_) * model.tensor_height_ * 3ULL;
            total_bps += tensor_bytes * model_fps;
        }
        slot.estimated_ddr_mbps_ =
            static_cast<std::uint32_t>(total_bps / (1024ULL * 1024ULL));

        candidate.resources_.frame_pool_bytes_ += slot.frame_pool_bytes_;
        candidate.resources_.encoder_pool_bytes_ += slot.encoder_pool_bytes_;
        candidate.resources_.cascade_roi_bytes_ += source.cascade_.max_bytes_;
        candidate.resources_.estimated_ddr_bandwidth_mbps_ += slot.estimated_ddr_mbps_;
        candidate.resources_.worker_concurrency_ +=
            1U + static_cast<std::uint16_t>(source.model_ids_.size()) +
            (source.cascade_.frames_ > 0 ? 1U : 0U);
    }

    for (std::size_t catalog_index = 0;
         catalog_index < _catalog.models_.size(); ++catalog_index) {
        if (assignment_counts[catalog_index] == 0) {
            continue;
        }
        const auto& model = _catalog.models_[catalog_index];
        auto& slot = candidate.models_[candidate.active_model_count_];
        slot.catalog_index_ = static_cast<std::uint16_t>(catalog_index);
        slot.assignment_count_ =
            static_cast<std::uint16_t>(assignment_counts[catalog_index]);
        slot.context_instance_count_ = static_cast<std::uint16_t>(
            model.resources_.can_share_context_across_sources_
                ? 1U
                : assignment_counts[catalog_index]);
        slot.resident_bytes_ = model.resources_.resident_bytes_ *
            slot.context_instance_count_;
        ++candidate.active_model_count_;
    }

    candidate.resources_.tensor_pool_bytes_ = required_model_bytes;
    candidate.resources_.fw_concurrency_slots_ = candidate.source_count_;
    candidate.resources_.total_memory_bytes_ = candidate.total_resident_bytes_ +
        candidate.resources_.encoder_pool_bytes_ +
        candidate.resources_.cascade_roi_bytes_;

    const std::uint32_t ddr_load_pct =
        (candidate.resources_.estimated_ddr_bandwidth_mbps_ * 50U) /
        _hardware_profile.max_ddr_bandwidth_mbps_;
    const std::uint32_t model_load_pct = candidate.active_model_count_ * 5U;
    const std::uint32_t total_load_pct =
        std::min<std::uint32_t>(90U, ddr_load_pct + model_load_pct);
    candidate.resources_.thermal_headroom_pct_ =
        static_cast<std::uint16_t>(100U - total_load_pct);

    if (candidate.source_count_ > _hardware_profile.max_fw_concurrency_slots_) {
        return {status_code::unsupported, "FW stream concurrency exceeds hardware capacity"};
    }
    if (candidate.resources_.total_memory_bytes_ > _hardware_profile.max_total_resident_bytes_) {
        return {status_code::resource_exhausted,
            "total memory requirement exceeds hardware admission envelope"};
    }
    if (candidate.resources_.frame_pool_bytes_ > _hardware_profile.max_frame_pool_bytes_) {
        return {status_code::resource_exhausted,
            "frame pool requirement exceeds hardware admission envelope"};
    }
    if (candidate.resources_.tensor_pool_bytes_ > _hardware_profile.max_tensor_pool_bytes_) {
        return {status_code::resource_exhausted,
            "tensor pool requirement exceeds hardware admission envelope"};
    }
    if (candidate.resources_.encoder_pool_bytes_ > _hardware_profile.max_encoder_pool_bytes_) {
        return {status_code::resource_exhausted,
            "encoder pool requirement exceeds hardware admission envelope"};
    }
    if (candidate.resources_.cascade_roi_bytes_ > _hardware_profile.max_cascade_roi_bytes_) {
        return {status_code::resource_exhausted,
            "cascade ROI memory exceeds hardware admission envelope"};
    }
    if (candidate.resources_.estimated_ddr_bandwidth_mbps_ >
        _hardware_profile.max_ddr_bandwidth_mbps_) {
        return {status_code::resource_exhausted,
            "estimated DDR bandwidth exceeds hardware admission envelope"};
    }
    if (candidate.resources_.worker_concurrency_ >
        _hardware_profile.max_worker_concurrency_) {
        return {status_code::resource_exhausted,
            "worker concurrency exceeds hardware capacity"};
    }
    if (candidate.resources_.thermal_headroom_pct_ <
        _hardware_profile.min_thermal_headroom_pct_) {
        return {status_code::resource_exhausted,
            "insufficient thermal headroom for workload"};
    }

    _snapshot = candidate;
    return {};
}

}  // namespace vqec::vision::ai
