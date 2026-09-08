#include "vqec_vision_activation_snapshot.hpp"

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

status vqec_vision_ai_admis_actsp_build_snapshot(
    const deployment_config& _deployment, const model_catalog& _catalog,
    activation_snapshot& _snapshot) {
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
        slot.model_count_ = static_cast<std::uint16_t>(source.model_ids_.size());
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
        }
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
    _snapshot = candidate;
    return {};
}

}  // namespace vqec::vision::ai
