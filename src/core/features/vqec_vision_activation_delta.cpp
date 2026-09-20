#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_activation_delta.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace vqec::vision::ai {
namespace {

const usecase_catalog_entry* vqec_vision_ai_core_acdel_find_usecase(
    const usecase_catalog& _catalog, const std::string& _app_id) noexcept {
    const auto found = std::find_if(_catalog.usecases_.begin(), _catalog.usecases_.end(),
        [&_app_id](const auto& _entry) { return _entry.usecase_id_ == _app_id; });
    return found == _catalog.usecases_.end() ? nullptr : &*found;
}

const feature_catalog_entry* vqec_vision_ai_core_acdel_find_feature(
    const feature_catalog& _catalog, const std::string& _feature_id) noexcept {
    const auto found = std::find_if(_catalog.features_.begin(), _catalog.features_.end(),
        [&_feature_id](const auto& _entry) { return _entry.feature_id_ == _feature_id; });
    return found == _catalog.features_.end() ? nullptr : &*found;
}

const model_catalog_entry* vqec_vision_ai_core_acdel_find_model(
    const model_catalog& _catalog, const std::string& _model_id) noexcept {
    const auto found = std::find_if(_catalog.models_.begin(), _catalog.models_.end(),
        [&_model_id](const auto& _entry) { return _entry.model_id_ == _model_id; });
    return found == _catalog.models_.end() ? nullptr : &*found;
}

const app_runtime_component* vqec_vision_ai_core_acdel_find_model_component(
    const app_runtime_association& _association, const std::string& _model_id) noexcept {
    const auto found = std::find_if(
        _association.components_.begin(), _association.components_.end(),
        [&_model_id](const auto& _component) {
            return _component.type_ == app_component_type::model &&
                _component.component_id_ == _model_id &&
                _component.model_role_ == app_model_role::primary;
        });
    return found == _association.components_.end() ? nullptr : &*found;
}

bool vqec_vision_ai_core_acdel_is_prepared(
    const app_runtime_association& _association) noexcept {
    // Entitlement and desired state authorize active work, not neutral owner preparation.
    // Keeping an installed compatible app in the prepared set allows an entitlement revoke
    // followed by renewal to reuse the same immutable capacity without loading hardware early.
    return _association.installed_ && _association.supported_ &&
        _association.compatible_ && _association.admitted_;
}

status vqec_vision_ai_core_acdel_find_source_slot(
    const deployment_config& _deployment, const std::string& _source_id,
    std::uint16_t& _source_slot) noexcept {
    for (std::uint16_t slot = 0; slot < _deployment.sources_.size(); ++slot) {
        if (_deployment.sources_[slot].source_id_ == _source_id) {
            _source_slot = slot;
            return {};
        }
    }
    return {status_code::invalid_argument,
        "runtime application references an unknown source"};
}

status vqec_vision_ai_core_acdel_find_model_slot(
    const source_deployment_config& _source, const std::string& _model_id,
    std::uint16_t& _model_slot) noexcept {
    for (std::uint16_t slot = 0; slot < _source.model_ids_.size(); ++slot) {
        if (_source.model_ids_[slot] == _model_id) {
            _model_slot = slot;
            return {};
        }
    }
    return {status_code::incompatible_plugin,
        "application model is outside prepared source capacity"};
}

model_dependency_reference* vqec_vision_ai_core_acdel_find_reference(
    app_activation_plan& _plan, std::uint16_t _source_slot,
    std::uint16_t _model_slot) noexcept {
    const auto found = std::find_if(
        _plan.model_dependencies_.begin(), _plan.model_dependencies_.end(),
        [_source_slot, _model_slot](const auto& _reference) {
            return _reference.source_slot_ == _source_slot &&
                _reference.model_slot_ == _model_slot;
        });
    return found == _plan.model_dependencies_.end() ? nullptr : &*found;
}

status vqec_vision_ai_core_acdel_add_model_consumer(
    app_activation_plan& _plan, const app_runtime_association& _association,
    const model_catalog_entry& _model, std::uint16_t _source_slot,
    std::uint16_t _model_slot, bool _is_effective) {
    const auto* component = vqec_vision_ai_core_acdel_find_model_component(
        _association, _model.model_id_);
    if (component == nullptr || component->component_version_ != _model.model_version_) {
        return {status_code::incompatible_plugin,
            "application model component differs from immutable model catalog"};
    }
    auto* reference = vqec_vision_ai_core_acdel_find_reference(
        _plan, _source_slot, _model_slot);
    if (reference == nullptr) {
        if (_plan.model_dependencies_.size() ==
            activation_delta_limits::g_max_model_dependencies) {
            return {status_code::resource_exhausted,
                "activation model dependency limit reached"};
        }
        model_dependency_reference created;
        created.source_id_ = _association.source_id_;
        created.model_id_ = _model.model_id_;
        created.model_version_ = _model.model_version_;
        created.target_id_ = _model.target_id_;
        created.artifact_sha256_ = component->artifact_sha256_;
        created.semantic_contract_sha256_ = component->semantic_contract_sha256_;
        created.preprocess_contract_ = _model.preprocess_contract_;
        created.source_slot_ = _source_slot;
        created.model_slot_ = _model_slot;
        _plan.model_dependencies_.push_back(std::move(created));
        reference = &_plan.model_dependencies_.back();
    } else if (reference->model_id_ != _model.model_id_ ||
        reference->model_version_ != _model.model_version_ ||
        reference->target_id_ != _model.target_id_ ||
        reference->artifact_sha256_ != component->artifact_sha256_ ||
        reference->semantic_contract_sha256_ != component->semantic_contract_sha256_ ||
        reference->preprocess_contract_ != _model.preprocess_contract_) {
        return {status_code::incompatible_plugin,
            "shared model consumers disagree on dependency identity"};
    }
    _plan.sources_[_source_slot].prepared_model_mask_ = static_cast<std::uint16_t>(
        _plan.sources_[_source_slot].prepared_model_mask_ | (1U << _model_slot));
    if (!_is_effective) {
        return {};
    }
    if (std::find(reference->consumer_app_ids_.begin(),
            reference->consumer_app_ids_.end(), _association.app_id_) !=
        reference->consumer_app_ids_.end()) {
        return {};
    }
    if (reference->consumer_app_ids_.size() == usecase_activation_limits::g_max_usecases ||
        reference->consumer_app_ids_.size() >=
            std::numeric_limits<std::uint16_t>::max()) {
        return {status_code::resource_exhausted,
            "model consumer reference count limit reached"};
    }
    reference->consumer_app_ids_.push_back(_association.app_id_);
    reference->consumer_count_ =
        static_cast<std::uint16_t>(reference->consumer_app_ids_.size());
    _plan.sources_[_source_slot].active_model_mask_ = static_cast<std::uint16_t>(
        _plan.sources_[_source_slot].active_model_mask_ | (1U << _model_slot));
    return {};
}

status vqec_vision_ai_core_acdel_add_feature_instance(
    app_activation_plan& _plan, const app_runtime_association& _association,
    const usecase_catalog_entry& _usecase, const feature_catalog& _features,
    const source_deployment_config& _source, std::uint16_t _source_slot) {
    for (const auto& feature_id : _usecase.feature_ids_) {
        if (_plan.feature_instances_.size() ==
            activation_delta_limits::g_max_feature_instances) {
            return {status_code::resource_exhausted,
                "activation feature instance limit reached"};
        }
        const auto* feature = vqec_vision_ai_core_acdel_find_feature(
            _features, feature_id);
        if (feature == nullptr || feature->configuration_schema_ !=
                _association.configuration_schema_id_ ||
            feature->model_dependencies_.empty()) {
            return {status_code::incompatible_plugin,
                "application feature differs from feature catalog"};
        }
        app_feature_instance instance;
        instance.source_id_ = _association.source_id_;
        instance.app_id_ = _association.app_id_;
        instance.feature_id_ = feature_id;
        instance.configuration_schema_id_ = _association.configuration_schema_id_;
        instance.configuration_sha256_ = _association.configuration_sha256_;
        instance.configuration_payload_ = _association.configuration_payload_;
        instance.output_scopes_ = _association.output_scopes_;
        instance.configuration_revision_ = _association.configuration_revision_;
        instance.source_slot_ = _source_slot;
        for (const auto& dependency : feature->model_dependencies_) {
            std::uint16_t model_slot = activation_delta_limits::g_invalid_slot;
            const auto resolved = vqec_vision_ai_core_acdel_find_model_slot(
                _source, dependency.model_id_, model_slot);
            if (resolved.code_ != status_code::ok) {
                return resolved;
            }
            instance.model_dependency_mask_ = static_cast<std::uint16_t>(
                instance.model_dependency_mask_ | (1U << model_slot));
        }
        _plan.feature_instances_.push_back(std::move(instance));
    }
    return {};
}

bool vqec_vision_ai_core_acdel_same_feature(
    const app_feature_instance& _left, const app_feature_instance& _right) noexcept {
    return _left.source_id_ == _right.source_id_ &&
        _left.app_id_ == _right.app_id_ &&
        _left.feature_id_ == _right.feature_id_;
}

bool vqec_vision_ai_core_acdel_same_feature_content(
    const app_feature_instance& _left, const app_feature_instance& _right) noexcept {
    return _left.configuration_schema_id_ == _right.configuration_schema_id_ &&
        _left.configuration_sha256_ == _right.configuration_sha256_ &&
        _left.configuration_payload_ == _right.configuration_payload_ &&
        _left.output_scopes_ == _right.output_scopes_ &&
        _left.configuration_revision_ == _right.configuration_revision_ &&
        _left.source_slot_ == _right.source_slot_ &&
        _left.model_dependency_mask_ == _right.model_dependency_mask_;
}

const model_dependency_reference* vqec_vision_ai_core_acdel_find_reference(
    const app_activation_plan& _plan, std::uint16_t _source_slot,
    std::uint16_t _model_slot) noexcept {
    const auto found = std::find_if(
        _plan.model_dependencies_.begin(), _plan.model_dependencies_.end(),
        [_source_slot, _model_slot](const auto& _reference) {
            return _reference.source_slot_ == _source_slot &&
                _reference.model_slot_ == _model_slot;
        });
    return found == _plan.model_dependencies_.end() ? nullptr : &*found;
}

}  // namespace

status vqec_vision_ai_core_acdel_build_plan(
    const deployment_config& _deployment, const model_catalog& _models,
    const feature_catalog& _features, const usecase_catalog& _usecases,
    const runtime_control_snapshot& _runtime, app_activation_plan& _plan) {
    try {
        const auto valid_runtime =
            vqec_vision_ai_core_applc_validate_runtime_snapshot(_runtime);
        if (valid_runtime.code_ != status_code::ok) {
            return valid_runtime;
        }
        const auto valid_usecases =
            vqec_vision_ai_core_ucact_validate_catalog(_usecases, _models);
        if (valid_usecases.code_ != status_code::ok) {
            return valid_usecases;
        }
        const auto valid_features =
            vqec_vision_ai_core_ftcat_validate_model_dependencies(_features, _models);
        if (valid_features.code_ != status_code::ok) {
            return valid_features;
        }
        std::uint64_t required_bytes = 0;
        const auto valid_deployment =
            vqec_vision_ai_core_mdcat_validate_deployment_models(
                _deployment, _models, required_bytes);
        if (valid_deployment.code_ != status_code::ok) {
            return valid_deployment;
        }
        if (_deployment.sources_.empty() ||
            _deployment.sources_.size() > deployment_limits::g_max_sources ||
            _features.model_catalog_ref_ != _models.catalog_id_ ||
            _usecases.model_catalog_ref_ != _models.catalog_id_) {
            return {status_code::invalid_argument,
                "activation catalogs and deployment do not share one model authority"};
        }

        app_activation_plan candidate;
        candidate.snapshot_revision_ = _runtime.snapshot_revision_;
        candidate.deployment_revision_ = _deployment.revision_;
        candidate.model_catalog_revision_ = _models.revision_;
        candidate.feature_catalog_revision_ = _features.revision_;
        candidate.usecase_catalog_revision_ = _usecases.revision_;
        candidate.source_count_ =
            static_cast<std::uint16_t>(_deployment.sources_.size());
        for (std::uint16_t slot = 0; slot < candidate.source_count_; ++slot) {
            candidate.sources_[slot].source_id_ =
                _deployment.sources_[slot].source_id_;
        }

        for (const auto& association : _runtime.associations_) {
            const auto* usecase = vqec_vision_ai_core_acdel_find_usecase(
                _usecases, association.app_id_);
            std::uint16_t source_slot = activation_delta_limits::g_invalid_slot;
            const auto source_found = vqec_vision_ai_core_acdel_find_source_slot(
                _deployment, association.source_id_, source_slot);
            if (usecase == nullptr || source_found.code_ != status_code::ok) {
                return {status_code::invalid_argument,
                    "runtime application is absent from usecase or source catalog"};
            }
            if (!vqec_vision_ai_core_acdel_is_prepared(association)) {
                continue;
            }
            const auto& source = _deployment.sources_[source_slot];
            for (const auto& model_id : usecase->root_model_ids_) {
                const auto* model = vqec_vision_ai_core_acdel_find_model(
                    _models, model_id);
                std::uint16_t model_slot = activation_delta_limits::g_invalid_slot;
                const auto model_found = vqec_vision_ai_core_acdel_find_model_slot(
                    source, model_id, model_slot);
                if (model == nullptr || model_found.code_ != status_code::ok) {
                    return {status_code::incompatible_plugin,
                        "usecase root model is outside source capacity"};
                }
                const auto added = vqec_vision_ai_core_acdel_add_model_consumer(
                    candidate, association, *model, source_slot, model_slot,
                    association.is_effective());
                if (added.code_ != status_code::ok) {
                    return added;
                }
            }
            if (association.is_effective()) {
                const auto added = vqec_vision_ai_core_acdel_add_feature_instance(
                    candidate, association, *usecase, _features, source, source_slot);
                if (added.code_ != status_code::ok) {
                    return added;
                }
            }
        }
        _plan = std::move(candidate);
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "activation dependency planning allocation failed"};
    }
}

status vqec_vision_ai_core_acdel_build_delta(
    const app_activation_plan& _previous, const app_activation_plan& _candidate,
    app_activation_delta& _delta) {
    try {
        if (_previous.snapshot_revision_ == 0 ||
            _candidate.snapshot_revision_ <= _previous.snapshot_revision_ ||
            _candidate.deployment_revision_ != _previous.deployment_revision_ ||
            _candidate.model_catalog_revision_ != _previous.model_catalog_revision_ ||
            _candidate.feature_catalog_revision_ != _previous.feature_catalog_revision_ ||
            _candidate.usecase_catalog_revision_ != _previous.usecase_catalog_revision_ ||
            _candidate.source_count_ != _previous.source_count_) {
            return {status_code::invalid_state,
                "activation delta revisions are stale or incompatible"};
        }
        app_activation_delta candidate_delta;
        candidate_delta.previous_snapshot_revision_ = _previous.snapshot_revision_;
        candidate_delta.candidate_snapshot_revision_ = _candidate.snapshot_revision_;
        for (std::uint16_t source_slot = 0;
             source_slot < _candidate.source_count_; ++source_slot) {
            if (_candidate.sources_[source_slot].source_id_ !=
                _previous.sources_[source_slot].source_id_) {
                return {status_code::invalid_state,
                    "activation source slot identity changed"};
            }
            candidate_delta.previous_active_model_masks_[source_slot] =
                _previous.sources_[source_slot].active_model_mask_;
            candidate_delta.candidate_active_model_masks_[source_slot] =
                _candidate.sources_[source_slot].active_model_mask_;
            if ((_candidate.sources_[source_slot].prepared_model_mask_ &
                    static_cast<std::uint16_t>(
                        ~_previous.sources_[source_slot].prepared_model_mask_)) != 0) {
                candidate_delta.requires_capacity_replacement_ = true;
            }
            const auto capacity_mask = static_cast<std::uint16_t>(
                _previous.sources_[source_slot].prepared_model_mask_ |
                _candidate.sources_[source_slot].prepared_model_mask_);
            for (std::uint16_t model_slot = 0;
                 model_slot < deployment_limits::g_max_models_per_source; ++model_slot) {
                if ((capacity_mask & (1U << model_slot)) == 0) {
                    continue;
                }
                const auto* previous = vqec_vision_ai_core_acdel_find_reference(
                    _previous, source_slot, model_slot);
                const auto* next = vqec_vision_ai_core_acdel_find_reference(
                    _candidate, source_slot, model_slot);
                const auto previous_count = previous == nullptr ? 0 : previous->consumer_count_;
                const auto candidate_count = next == nullptr ? 0 : next->consumer_count_;
                if (previous != nullptr && next != nullptr &&
                    (previous->model_id_ != next->model_id_ ||
                     previous->model_version_ != next->model_version_ ||
                     previous->target_id_ != next->target_id_ ||
                     previous->artifact_sha256_ != next->artifact_sha256_ ||
                     previous->semantic_contract_sha256_ !=
                        next->semantic_contract_sha256_ ||
                     previous->preprocess_contract_ != next->preprocess_contract_)) {
                    candidate_delta.requires_capacity_replacement_ = true;
                }
                if (previous_count == candidate_count) {
                    continue;
                }
                model_dependency_delta change;
                change.source_id_ = _candidate.sources_[source_slot].source_id_;
                change.model_id_ = next != nullptr ? next->model_id_ : previous->model_id_;
                change.source_slot_ = source_slot;
                change.model_slot_ = model_slot;
                change.previous_consumer_count_ = previous_count;
                change.candidate_consumer_count_ = candidate_count;
                change.kind_ = previous_count == 0 ? model_dependency_delta_kind::acquire :
                    (candidate_count == 0 ? model_dependency_delta_kind::release :
                        model_dependency_delta_kind::retain);
                candidate_delta.model_dependencies_.push_back(std::move(change));
            }
        }

        for (const auto& previous : _previous.feature_instances_) {
            const auto found = std::find_if(
                _candidate.feature_instances_.begin(), _candidate.feature_instances_.end(),
                [&previous](const auto& _value) {
                    return vqec_vision_ai_core_acdel_same_feature(previous, _value);
                });
            feature_instance_delta change;
            if (found == _candidate.feature_instances_.end()) {
                change = {previous.source_id_, previous.app_id_, previous.feature_id_,
                    feature_instance_delta_kind::remove};
            } else if (!vqec_vision_ai_core_acdel_same_feature_content(previous, *found)) {
                change = {previous.source_id_, previous.app_id_, previous.feature_id_,
                    feature_instance_delta_kind::replace};
            } else {
                continue;
            }
            candidate_delta.feature_instances_.push_back(std::move(change));
        }
        for (const auto& next : _candidate.feature_instances_) {
            const auto found = std::find_if(
                _previous.feature_instances_.begin(), _previous.feature_instances_.end(),
                [&next](const auto& _value) {
                    return vqec_vision_ai_core_acdel_same_feature(next, _value);
                });
            if (found == _previous.feature_instances_.end()) {
                candidate_delta.feature_instances_.push_back(
                    {next.source_id_, next.app_id_, next.feature_id_,
                        feature_instance_delta_kind::add});
            }
        }
        _delta = std::move(candidate_delta);
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "activation delta allocation failed"};
    }
}

}  // namespace vqec::vision::ai
