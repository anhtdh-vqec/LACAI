#include "vqec/vision/ai/contracts/vqec_vision_usecase_activation.hpp"

#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_ucact_is_identifier(const std::string& _value) noexcept {
    return vqec_vision_ai_cntr_ident_is_valid(
        _value, usecase_activation_limits::g_max_identifier_bytes);
}

const model_catalog_entry* vqec_vision_ai_core_ucact_find_model(
    const model_catalog& _models, const std::string& _model_id) noexcept {
    for (const auto& model : _models.models_) {
        if (model.model_id_ == _model_id) {
            return &model;
        }
    }
    return nullptr;
}

const usecase_catalog_entry* vqec_vision_ai_core_ucact_find_usecase(
    const usecase_catalog& _usecases, const std::string& _usecase_id) noexcept {
    for (const auto& usecase : _usecases.usecases_) {
        if (usecase.usecase_id_ == _usecase_id) {
            return &usecase;
        }
    }
    return nullptr;
}

const source_deployment_config* vqec_vision_ai_core_ucact_find_source(
    const deployment_config& _deployment, const std::string& _source_id) noexcept {
    for (const auto& source : _deployment.sources_) {
        if (source.source_id_ == _source_id) {
            return &source;
        }
    }
    return nullptr;
}

bool vqec_vision_ai_core_ucact_source_contains_root(
    const source_deployment_config& _source, const std::string& _model_id) noexcept {
    for (const auto& source_model_id : _source.model_ids_) {
        if (source_model_id == _model_id) {
            return true;
        }
    }
    return false;
}

bool vqec_vision_ai_core_ucact_usecase_contains_root(
    const usecase_catalog_entry& _usecase, const std::string& _model_id) noexcept {
    for (const auto& root_model_id : _usecase.root_model_ids_) {
        if (root_model_id == _model_id) {
            return true;
        }
    }
    return false;
}

usecase_effective_state vqec_vision_ai_core_ucact_resolve_state(
    const usecase_activation_request& _request, status_code& _reason_code) noexcept {
    _reason_code = status_code::ok;
    if (!_request.desired_enabled_) {
        return usecase_effective_state::disabled;
    }
    if (!_request.installed_) {
        _reason_code = status_code::missing_plugin;
        return usecase_effective_state::not_installed;
    }
    if (!_request.entitlement_granted_) {
        _reason_code = status_code::unauthorized;
        return usecase_effective_state::denied;
    }
    if (!_request.supported_) {
        _reason_code = status_code::unsupported;
        return usecase_effective_state::unsupported;
    }
    if (!_request.compatible_) {
        _reason_code = status_code::incompatible_plugin;
        return usecase_effective_state::incompatible;
    }
    if (!_request.resource_admitted_) {
        _reason_code = status_code::resource_exhausted;
        return usecase_effective_state::resource_limited;
    }
    return usecase_effective_state::ready;
}

}  // namespace

status vqec_vision_ai_core_ucact_validate_catalog(
    const usecase_catalog& _usecases, const model_catalog& _models) {
    std::uint64_t declared_model_bytes = 0;
    const auto valid_models =
        vqec_vision_ai_core_mdcat_validate_catalog(_models, declared_model_bytes);
    if (valid_models.code_ != status_code::ok) {
        return valid_models;
    }
    if (_usecases.schema_version_ != usecase_activation_limits::g_schema_version) {
        return {status_code::unsupported, "unsupported usecase catalog schema"};
    }
    if (_usecases.revision_ == 0 ||
        !vqec_vision_ai_core_ucact_is_identifier(_usecases.catalog_id_) ||
        _usecases.model_catalog_ref_ != _models.catalog_id_ ||
        _usecases.usecases_.empty() ||
        _usecases.usecases_.size() > usecase_activation_limits::g_max_usecases) {
        return {status_code::invalid_argument, "invalid usecase catalog identity"};
    }
    for (std::size_t index = 0; index < _usecases.usecases_.size(); ++index) {
        const auto& usecase = _usecases.usecases_[index];
        if (!vqec_vision_ai_core_ucact_is_identifier(usecase.usecase_id_) ||
            !vqec_vision_ai_core_ucact_is_identifier(usecase.usecase_version_) ||
            usecase.root_model_ids_.empty() ||
            usecase.root_model_ids_.size() > usecase_activation_limits::g_max_root_models ||
            usecase.feature_ids_.size() > usecase_activation_limits::g_max_feature_ids) {
            return {status_code::invalid_argument, "invalid usecase catalog entry"};
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (_usecases.usecases_[prior].usecase_id_ == usecase.usecase_id_) {
                return {status_code::invalid_argument, "duplicate usecase identifier"};
            }
        }
        for (std::size_t root = 0; root < usecase.root_model_ids_.size(); ++root) {
            const auto& model_id = usecase.root_model_ids_[root];
            const auto* model = vqec_vision_ai_core_ucact_find_model(_models, model_id);
            if (!vqec_vision_ai_core_ucact_is_identifier(model_id) || model == nullptr ||
                model->role_ != model_role::primary) {
                return {status_code::invalid_argument, "usecase root is not a primary model"};
            }
            for (std::size_t prior = 0; prior < root; ++prior) {
                if (usecase.root_model_ids_[prior] == model_id) {
                    return {status_code::invalid_argument, "duplicate usecase root model"};
                }
            }
        }
        for (std::size_t feature = 0; feature < usecase.feature_ids_.size(); ++feature) {
            if (!vqec_vision_ai_core_ucact_is_identifier(usecase.feature_ids_[feature])) {
                return {status_code::invalid_argument, "invalid usecase feature identifier"};
            }
            for (std::size_t prior = 0; prior < feature; ++prior) {
                if (usecase.feature_ids_[prior] == usecase.feature_ids_[feature]) {
                    return {status_code::invalid_argument, "duplicate usecase feature"};
                }
            }
        }
    }
    return {};
}

status vqec_vision_ai_core_ucact_compose_effective_deployment(
    const deployment_config& _base_deployment, const model_catalog& _models,
    const usecase_catalog& _usecases,
    const std::vector<usecase_activation_request>& _requests,
    usecase_activation_snapshot& _snapshot, deployment_config& _effective_deployment) {
    std::uint64_t declared_deployment_bytes = 0;
    const auto valid_deployment = vqec_vision_ai_core_dpval_validate_deployment(
        _base_deployment, declared_deployment_bytes);
    if (valid_deployment.code_ != status_code::ok) {
        return valid_deployment;
    }
    const auto valid_catalog = vqec_vision_ai_core_ucact_validate_catalog(_usecases, _models);
    if (valid_catalog.code_ != status_code::ok) {
        return valid_catalog;
    }
    if (_base_deployment.model_catalog_ref_ != _models.catalog_id_) {
        return {status_code::invalid_argument, "deployment references another model catalog"};
    }
    const auto expected_associations =
        _base_deployment.sources_.size() * _usecases.usecases_.size();
    if (_requests.size() != expected_associations ||
        _requests.size() > usecase_activation_limits::g_max_associations) {
        return {status_code::invalid_argument, "usecase plan is not a complete snapshot"};
    }

    usecase_activation_snapshot candidate_snapshot;
    candidate_snapshot.usecase_catalog_revision_ = _usecases.revision_;
    candidate_snapshot.model_catalog_revision_ = _models.revision_;
    candidate_snapshot.deployment_revision_ = _base_deployment.revision_;
    candidate_snapshot.records_.reserve(_requests.size());
    for (std::size_t index = 0; index < _requests.size(); ++index) {
        const auto& request = _requests[index];
        const auto* source =
            vqec_vision_ai_core_ucact_find_source(_base_deployment, request.source_id_);
        const auto* usecase =
            vqec_vision_ai_core_ucact_find_usecase(_usecases, request.usecase_id_);
        if (source == nullptr || usecase == nullptr) {
            return {status_code::invalid_argument, "usecase plan references an unknown entry"};
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (_requests[prior].source_id_ == request.source_id_ &&
                _requests[prior].usecase_id_ == request.usecase_id_) {
                return {status_code::invalid_argument, "duplicate usecase association"};
            }
        }
        usecase_activation_record record;
        record.source_id_ = request.source_id_;
        record.usecase_id_ = request.usecase_id_;
        record.desired_enabled_ = request.desired_enabled_;
        record.installed_ = request.installed_;
        record.entitlement_granted_ = request.entitlement_granted_;
        record.supported_ = request.supported_;
        record.compatible_ = request.compatible_;
        record.resource_admitted_ = request.resource_admitted_;
        record.state_ = vqec_vision_ai_core_ucact_resolve_state(
            request, record.reason_code_);
        if (record.state_ == usecase_effective_state::ready) {
            for (const auto& root_model_id : usecase->root_model_ids_) {
                if (!vqec_vision_ai_core_ucact_source_contains_root(*source, root_model_id)) {
                    record.state_ = usecase_effective_state::incompatible;
                    record.reason_code_ = status_code::incompatible_plugin;
                    break;
                }
            }
        }
        candidate_snapshot.records_.push_back(std::move(record));
    }

    deployment_config candidate_deployment = _base_deployment;
    candidate_deployment.sources_.clear();
    for (const auto& base_source : _base_deployment.sources_) {
        source_deployment_config effective_source = base_source;
        effective_source.model_ids_.clear();
        for (const auto& base_model_id : base_source.model_ids_) {
            bool is_required = false;
            for (std::size_t index = 0; index < _requests.size(); ++index) {
                const auto& request = _requests[index];
                const auto& record = candidate_snapshot.records_[index];
                if (request.source_id_ != base_source.source_id_ ||
                    record.state_ != usecase_effective_state::ready) {
                    continue;
                }
                const auto* usecase =
                    vqec_vision_ai_core_ucact_find_usecase(_usecases, request.usecase_id_);
                if (usecase != nullptr &&
                    vqec_vision_ai_core_ucact_usecase_contains_root(*usecase, base_model_id)) {
                    is_required = true;
                    break;
                }
            }
            if (is_required) {
                effective_source.model_ids_.push_back(base_model_id);
            }
        }
        if (effective_source.model_ids_.empty()) {
            continue;
        }
        bool has_secondary_dependency = false;
        for (const auto& model : _models.models_) {
            if (model.role_ == model_role::secondary &&
                vqec_vision_ai_core_mdcat_source_activates_model(effective_source, model)) {
                has_secondary_dependency = true;
                break;
            }
        }
        if (!has_secondary_dependency) {
            effective_source.cascade_ = {};
        }
        candidate_deployment.sources_.push_back(std::move(effective_source));
    }

    if (!candidate_deployment.sources_.empty()) {
        std::uint64_t effective_bytes = 0;
        const auto valid_effective = vqec_vision_ai_core_dpval_validate_deployment(
            candidate_deployment, effective_bytes);
        if (valid_effective.code_ != status_code::ok) {
            return valid_effective;
        }
        std::uint64_t required_model_bytes = 0;
        const auto valid_models = vqec_vision_ai_core_mdcat_validate_deployment_models(
            candidate_deployment, _models, required_model_bytes);
        if (valid_models.code_ != status_code::ok) {
            return valid_models;
        }
    }

    _snapshot = std::move(candidate_snapshot);
    _effective_deployment = std::move(candidate_deployment);
    return {};
}

}  // namespace vqec::vision::ai
