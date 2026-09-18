#include "vqec_vision_source_perception_factory.hpp"

#include <new>
#include <utility>

namespace vqec::vision::ai {
namespace {

const model_catalog_entry* vqec_vision_ai_appl_spfac_find_model(
    const model_catalog& _models, const std::string& _model_id) noexcept {
    for (const auto& model : _models.models_) {
        if (model.model_id_ == _model_id) {
            return &model;
        }
    }
    return nullptr;
}

}  // namespace

multi_model_result_router* source_perception_bundle::
vqec_vision_ai_appl_spfac_get_result_router() noexcept {
    return &router_;
}

perception_stage_bundle* source_perception_bundle::
vqec_vision_ai_appl_spfac_get_stage_bundle(std::uint16_t _slot) noexcept {
    return _slot < model_count_ ? stages_[_slot].get() : nullptr;
}

std::uint16_t source_perception_bundle::
vqec_vision_ai_appl_spfac_get_model_count() const noexcept {
    return model_count_;
}

status vqec_vision_ai_appl_spfac_create_source_bundle(
    const model_catalog& _models, const source_deployment_config& _source,
    const std::array<perception_model_activation,
        deployment_limits::g_max_models_per_source>& _activations,
    std::uint16_t _activation_count,
    const model_decoder_registry& _decoder_registry,
    const tracker_registry& _tracker_registry,
    std::unique_ptr<source_perception_bundle>& _bundle) {
    std::uint64_t declared_model_bytes = 0;
    const auto valid_models = vqec_vision_ai_core_mdcat_validate_catalog(
        _models, declared_model_bytes);
    if (valid_models.code_ != status_code::ok) {
        return valid_models;
    }
    if (_source.source_id_.empty() || _source.profile_.width_ == 0 ||
        _source.profile_.height_ == 0 || _activation_count == 0 ||
        _activation_count > _activations.size() ||
        _activation_count != _source.model_ids_.size()) {
        return {status_code::invalid_argument,
            "source perception activation count or source profile is invalid"};
    }
    for (std::uint16_t slot = 0; slot < _activation_count; ++slot) {
        if (_activations[slot].model_id_ != _source.model_ids_[slot] ||
            _activations[slot].tracker_contract_.empty() ||
            _activations[slot].resolved_output_manifest_ref_.empty() ||
            vqec_vision_ai_appl_spfac_find_model(
                _models, _activations[slot].model_id_) == nullptr) {
            return {status_code::invalid_argument,
                "perception activation does not match the deployment model slot"};
        }
    }

    try {
        auto candidate = std::unique_ptr<source_perception_bundle>(
            new source_perception_bundle());
        std::array<perception_result_stage*,
            deployment_limits::g_max_models_per_source> result_stages{};
        for (std::uint16_t slot = 0; slot < _activation_count; ++slot) {
            const auto* model = vqec_vision_ai_appl_spfac_find_model(
                _models, _activations[slot].model_id_);
            const auto created = vqec_vision_ai_appl_prfac_create_bundle(
                *model, _source,
                _activations[slot].resolved_output_manifest_ref_,
                _activations[slot].outputs_,
                _activations[slot].tracker_contract_,
                _decoder_registry, _tracker_registry, candidate->stages_[slot]);
            if (created.code_ != status_code::ok) {
                return created;
            }
            result_stages[slot] = candidate->stages_[slot]->
                vqec_vision_ai_appl_prfac_get_result_stage();
        }
        const perception_result_config source_config{_source.camera_id_,
            _source.channel_id_,
            {_source.profile_.width_, _source.profile_.height_}};
        const auto configured = candidate->router_.
            vqec_vision_ai_appl_mmrrt_configure(
                source_config, result_stages, _activation_count);
        if (configured.code_ != status_code::ok) {
            return configured;
        }
        candidate->model_count_ = _activation_count;
        _bundle = std::move(candidate);
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "source perception bundle allocation failed"};
    } catch (...) {
        return {status_code::io_error,
            "source perception bundle construction raised an exception"};
    }
}

}  // namespace vqec::vision::ai
