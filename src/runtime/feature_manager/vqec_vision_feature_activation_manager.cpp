#include "vqec_vision_feature_activation_manager.hpp"

#include <new>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {

status feature_activation_manager::vqec_vision_ai_ftmgr_famgr_configure(
    const feature_catalog& _features, const model_catalog& _models,
    const deployment_config& _deployment) {
    if (is_configured_) {
        return {status_code::invalid_state,
            "feature activation manager is already configured"};
    }
    const auto valid_features =
        vqec_vision_ai_core_ftcat_validate_model_dependencies(_features, _models);
    if (valid_features.code_ != status_code::ok) {
        return valid_features;
    }
    std::uint64_t required_model_bytes = 0;
    const auto valid_deployment =
        vqec_vision_ai_core_mdcat_validate_deployment_models(
            _deployment, _models, required_model_bytes);
    if (valid_deployment.code_ != status_code::ok) {
        return valid_deployment;
    }
    if (_features.model_catalog_ref_ != _models.catalog_id_) {
        return {status_code::invalid_argument,
            "feature activation references another model catalog"};
    }
    features_ = &_features;
    models_ = &_models;
    deployment_ = &_deployment;
    is_configured_ = true;
    return {};
}

const feature_catalog_entry* feature_activation_manager::
vqec_vision_ai_ftmgr_famgr_find_feature(
    const std::string& _feature_id) const noexcept {
    if (features_ == nullptr) {
        return nullptr;
    }
    for (const auto& feature : features_->features_) {
        if (feature.feature_id_ == _feature_id) {
            return &feature;
        }
    }
    return nullptr;
}

const source_deployment_config* feature_activation_manager::
vqec_vision_ai_ftmgr_famgr_find_source(
    const std::string& _source_id) const noexcept {
    if (deployment_ == nullptr) {
        return nullptr;
    }
    for (const auto& source : deployment_->sources_) {
        if (source.source_id_ == _source_id) {
            return &source;
        }
    }
    return nullptr;
}

bool feature_activation_manager::vqec_vision_ai_ftmgr_famgr_source_has_models(
    const source_deployment_config& _source,
    const feature_catalog_entry& _feature) const noexcept {
    for (const auto& dependency : _feature.model_dependencies_) {
        bool found = false;
        for (const auto& model_id : _source.model_ids_) {
            if (model_id == dependency.model_id_) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

status feature_activation_manager::vqec_vision_ai_ftmgr_famgr_reconcile(
    const std::array<feature_activation_request,
        feature_activation_limits::g_max_associations>& _requests,
    std::uint16_t _request_count, const feature_processor_registry& _registry,
    feature_activation_snapshot& _snapshot) {
    if (!is_configured_) {
        return {status_code::invalid_state,
            "feature activation manager is not configured"};
    }
    if (is_frozen_) {
        return {status_code::invalid_state,
            "feature activation manager is frozen while its stages are borrowed"};
    }
    if (_request_count == 0 || _request_count > _requests.size()) {
        return {status_code::invalid_argument,
            "feature activation request count is invalid"};
    }
    for (std::uint16_t index = 0; index < _request_count; ++index) {
        if (!vqec_vision_ai_cntr_ident_is_valid(
                _requests[index].source_id_,
                feature_catalog_limits::g_max_identifier_bytes) ||
            !vqec_vision_ai_cntr_ident_is_valid(
                _requests[index].feature_id_,
                feature_catalog_limits::g_max_identifier_bytes) ||
            vqec_vision_ai_ftmgr_famgr_find_source(_requests[index].source_id_) == nullptr ||
            vqec_vision_ai_ftmgr_famgr_find_feature(_requests[index].feature_id_) == nullptr) {
            return {status_code::invalid_argument,
                "feature activation references unknown source or feature"};
        }
        for (std::uint16_t prior = 0; prior < index; ++prior) {
            if (_requests[prior].source_id_ == _requests[index].source_id_ &&
                _requests[prior].feature_id_ == _requests[index].feature_id_) {
                return {status_code::invalid_argument,
                    "feature activation association is duplicated"};
            }
        }
    }

    std::array<feature_activation_record,
        feature_activation_limits::g_max_associations> candidate{};
    feature_activation_snapshot candidate_snapshot;
    candidate_snapshot.feature_catalog_revision_ = features_->revision_;
    candidate_snapshot.model_catalog_revision_ = models_->revision_;
    candidate_snapshot.deployment_revision_ = deployment_->revision_;
    status first_error;
    bool has_error = false;
    for (std::uint16_t index = 0; index < _request_count; ++index) {
        const auto& request = _requests[index];
        const auto* feature = vqec_vision_ai_ftmgr_famgr_find_feature(
            request.feature_id_);
        const auto* source = vqec_vision_ai_ftmgr_famgr_find_source(
            request.source_id_);
        auto& record = candidate[index];
        record.source_id_ = request.source_id_;
        record.feature_id_ = request.feature_id_;
        record.desired_enabled_ = request.desired_enabled_;
        record.entitlement_granted_ = request.entitlement_granted_;
        record.resource_admitted_ = request.resource_admitted_;
        ++candidate_snapshot.association_count_;

        if (!request.desired_enabled_) {
            record.state_ = feature_effective_state::disabled;
            continue;
        }
        if (!request.entitlement_granted_) {
            record.state_ = feature_effective_state::denied;
            record.reason_code_ = status_code::unauthorized;
            ++candidate_snapshot.denied_count_;
            continue;
        }
        if (!request.resource_admitted_) {
            record.state_ = feature_effective_state::resource_limited;
            record.reason_code_ = status_code::resource_exhausted;
            ++candidate_snapshot.resource_limited_count_;
            continue;
        }
        if (!vqec_vision_ai_ftmgr_famgr_source_has_models(*source, *feature)) {
            record.state_ = feature_effective_state::unsupported;
            record.reason_code_ = status_code::unsupported;
            ++candidate_snapshot.unsupported_count_;
            continue;
        }

        std::unique_ptr<feature_processor_port> processor;
        const auto created = _registry.vqec_vision_ai_ftmgr_ftreg_create_processor(
            *feature, request.source_id_, request.configuration_, processor);
        if (created.code_ != status_code::ok) {
            record.state_ = created.code_ == status_code::unsupported ?
                feature_effective_state::unsupported : feature_effective_state::faulted;
            record.reason_code_ = created.code_;
            if (record.state_ == feature_effective_state::unsupported) {
                ++candidate_snapshot.unsupported_count_;
            } else {
                ++candidate_snapshot.faulted_count_;
            }
            if (record.state_ == feature_effective_state::faulted && !has_error) {
                first_error = created;
                has_error = true;
            }
            continue;
        }
        try {
            feature_processor_config processor_config;
            const auto composed = vqec_vision_ai_core_ftcat_compose_processor_config(
                *feature, request.source_id_, request.configuration_.revision_,
                processor_config);
            if (composed.code_ != status_code::ok) {
                record.state_ = feature_effective_state::faulted;
                record.reason_code_ = composed.code_;
                ++candidate_snapshot.faulted_count_;
                if (!has_error) {
                    first_error = composed;
                    has_error = true;
                }
                continue;
            }
            auto stage = std::make_unique<feature_stage>(*processor, processor_config);
            const auto activated = stage->vqec_vision_ai_ftmgr_ftstg_activate();
            if (activated.code_ != status_code::ok) {
                record.state_ = feature_effective_state::faulted;
                record.reason_code_ = activated.code_;
                ++candidate_snapshot.faulted_count_;
                if (!has_error) {
                    first_error = activated;
                    has_error = true;
                }
                continue;
            }
            record.processor_ = std::move(processor);
            record.stage_ = std::move(stage);
            record.state_ = feature_effective_state::ready;
            ++candidate_snapshot.ready_count_;
        } catch (const std::bad_alloc&) {
            record.state_ = feature_effective_state::faulted;
            record.reason_code_ = status_code::resource_exhausted;
            ++candidate_snapshot.faulted_count_;
            if (!has_error) {
                first_error = {status_code::resource_exhausted,
                    "feature stage allocation failed"};
                has_error = true;
            }
        } catch (...) {
            record.state_ = feature_effective_state::faulted;
            record.reason_code_ = status_code::io_error;
            ++candidate_snapshot.faulted_count_;
            if (!has_error) {
                first_error = {status_code::io_error,
                    "feature stage construction raised an exception"};
                has_error = true;
            }
        }
    }

    records_ = std::move(candidate);
    record_count_ = _request_count;
    _snapshot = candidate_snapshot;
    return has_error ? first_error : status{};
}

const feature_activation_record* feature_activation_manager::
vqec_vision_ai_ftmgr_famgr_get_record(std::uint16_t _slot) const noexcept {
    return _slot < record_count_ ? &records_[_slot] : nullptr;
}

const feature_catalog_entry* feature_activation_manager::
vqec_vision_ai_ftmgr_famgr_get_feature(std::uint16_t _slot) const noexcept {
    const auto* record = vqec_vision_ai_ftmgr_famgr_get_record(_slot);
    return record == nullptr ? nullptr :
        vqec_vision_ai_ftmgr_famgr_find_feature(record->feature_id_);
}

feature_stage* feature_activation_manager::
vqec_vision_ai_ftmgr_famgr_get_stage(std::uint16_t _slot) const noexcept {
    return _slot < record_count_ ? records_[_slot].stage_.get() : nullptr;
}

std::uint16_t feature_activation_manager::
vqec_vision_ai_ftmgr_famgr_get_count() const noexcept {
    return record_count_;
}

void feature_activation_manager::vqec_vision_ai_ftmgr_famgr_freeze() noexcept {
    is_frozen_ = true;
}

bool feature_activation_manager::
vqec_vision_ai_ftmgr_famgr_is_frozen() const noexcept {
    return is_frozen_;
}

}  // namespace vqec::vision::ai
