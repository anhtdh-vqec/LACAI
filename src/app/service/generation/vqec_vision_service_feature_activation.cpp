#include "vqec_vision_service_feature_activation.hpp"

#include <algorithm>
#include <cstdio>
#include <new>
#include <utility>
#include <vector>

#include "vqec/vision/ai/contracts/features/vqec_vision_usecase_activation.hpp"
#include "vqec/vision/ai/contracts/output/vqec_vision_output_gate.hpp"
#include "vqec_vision_feature_processor_registry.hpp"
#include "vqec_vision_service_fixture.hpp"

namespace vqec::vision::ai {
namespace {

std::uint16_t vqec_vision_ai_appl_svfac_find_model_slot(
    const source_deployment_config& _source, const std::string& _model_id) noexcept {
    for (std::uint16_t slot = 0; slot < _source.model_ids_.size(); ++slot) {
        if (_source.model_ids_[slot] == _model_id) {
            return slot;
        }
    }
    return g_invalid_model_slot;
}

status vqec_vision_ai_appl_svfac_resolve_runtime_configuration(
    const service_startup_resolution& _startup, const std::string& _source_id,
    const feature_catalog_entry& _feature, feature_configuration& _configuration,
    std::vector<std::string>& _authorized_output_scopes) {
    const app_runtime_association* selected = nullptr;
    for (const auto& usecase : _startup.usecase_control.catalog_.usecases_) {
        if (std::find(usecase.feature_ids_.begin(), usecase.feature_ids_.end(),
                _feature.feature_id_) == usecase.feature_ids_.end()) {
            continue;
        }
        for (const auto& association : _startup.runtime_control.associations_) {
            if (association.app_id_ != usecase.usecase_id_ ||
                association.source_id_ != _source_id || !association.is_effective()) {
                continue;
            }
            if (selected != nullptr &&
                (selected->configuration_schema_id_ !=
                        association.configuration_schema_id_ ||
                    selected->configuration_revision_ !=
                        association.configuration_revision_ ||
                    selected->configuration_sha256_ !=
                        association.configuration_sha256_ ||
                    selected->configuration_payload_ !=
                        association.configuration_payload_ ||
                    selected->output_scopes_ != association.output_scopes_)) {
                return {status_code::invalid_state,
                    "effective applications disagree on shared feature configuration"};
            }
            selected = &association;
        }
    }
    if (selected == nullptr) {
        return {status_code::unauthorized,
            "effective feature has no runtime-control application association"};
    }
    if (selected->configuration_schema_id_ != _feature.configuration_schema_) {
        return {status_code::protocol_error,
            "runtime-control configuration schema differs from feature catalog"};
    }
    try {
        _configuration.schema_id_ = selected->configuration_schema_id_;
        _configuration.revision_ = selected->configuration_revision_;
        _configuration.payload_ = selected->configuration_payload_;
        _authorized_output_scopes = selected->output_scopes_;
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "runtime feature configuration allocation failed"};
    }
}

void vqec_vision_ai_appl_svfac_append_preview_rules(
    output_policy& _policy, const service_startup_resolution& _startup,
    const parsed_arguments& _args, const deployment_config& _deployment) {
    for (const auto& source : _deployment.sources_) {
        if (!vqec_vision_ai_appl_svstr_is_preview_authorized(
                _startup, _args, source.source_id_)) {
            continue;
        }
        output_scope_rule rule;
        rule.source_id_ = source.source_id_;
        rule.feature_id_ = "preview";
        rule.attributes_.push_back("overlay");
        _policy.rules_.push_back(std::move(rule));
    }
}

void vqec_vision_ai_appl_svfac_append_recognition_rules(
    output_policy& _policy, const deployment_config& _deployment,
    const std::string& _feature_id, const std::string& _attribute_id) {
    for (const auto& source : _deployment.sources_) {
        output_scope_rule rule;
        rule.source_id_ = source.source_id_;
        rule.feature_id_ = _feature_id;
        rule.attributes_.push_back(_attribute_id);
        _policy.rules_.push_back(std::move(rule));
    }
}

}  // namespace

status service_feature_activation::vqec_vision_ai_appl_svfac_configure(
    const service_startup_resolution& _startup, const parsed_arguments& _args,
    const deployment_config& _deployment, const model_catalog& _catalog,
    const feature_catalog& _features, const feature_processor_registry& _registry,
    const std::string& _fallback_attribute_schema, output_gate& _output_gate,
    std::uint16_t _source_count, bool _apply_output_policy) {
    if (has_wiring_ || manager_.vqec_vision_ai_ftmgr_famgr_is_frozen() ||
        _source_count == 0 || _source_count > _deployment.sources_.size()) {
        return {status_code::invalid_state,
            "service feature activation owner cannot be configured"};
    }
    wiring_.deployment_revision_ = _deployment.revision_;
    wiring_.catalog_revision_ = _catalog.revision_;
    wiring_.source_count_ = _source_count;

    output_policy policy;
    policy.revision_ = _startup.has_usecase_control ?
        _startup.usecase_activation.policy_revision_ : service_harness::g_policy_revision;
    policy.not_before_ns_ = 0;
    policy.expires_ns_ = service_harness::g_policy_expiry_ns;
    std::uint16_t request_count = 0;

    if (!_features.features_.empty()) {
        const auto configured = manager_.vqec_vision_ai_ftmgr_famgr_configure(
            _features, _catalog, _deployment);
        if (configured.code_ != status_code::ok) {
            return configured;
        }
        std::array<feature_activation_request,
            feature_activation_limits::g_max_associations> requests{};
        std::array<std::pair<std::uint16_t, std::uint16_t>,
            feature_activation_limits::g_max_associations> request_slots{};
        std::array<std::vector<std::string>,
            feature_activation_limits::g_max_associations> authorized_output_scopes{};
        for (std::uint16_t source_slot = 0; source_slot < _source_count; ++source_slot) {
            const auto& source = _deployment.sources_[source_slot];
            for (const auto& feature : _features.features_) {
                if (feature.input_mode_ != feature_input_mode::single_model ||
                    feature.model_dependencies_.size() != 1) {
                    continue;
                }
                const auto slot = vqec_vision_ai_appl_svfac_find_model_slot(
                    source, feature.model_dependencies_[0].model_id_);
                if (slot == g_invalid_model_slot) {
                    continue;
                }
                if (request_count == requests.size()) {
                    return {status_code::resource_exhausted,
                        "service feature association limit reached"};
                }
                auto& request = requests[request_count];
                request.source_id_ = source.source_id_;
                request.feature_id_ = feature.feature_id_;
                if (_startup.has_usecase_control) {
                    const auto projected =
                        vqec_vision_ai_core_ucact_project_feature_association(
                            _startup.usecase_control.catalog_,
                            _startup.usecase_activation, _deployment,
                            source.source_id_, feature, request.association_);
                    if (projected.code_ != status_code::ok) {
                        return projected;
                    }
                    request.desired_enabled_ = request.association_.desired_enabled_;
                    request.entitlement_granted_ =
                        request.association_.entitlement_granted_;
                    request.resource_admitted_ = request.association_.resource_admitted_;
                } else {
                    const auto authority =
                        vqec_vision_ai_appl_svstr_resolve_feature_authority(
                            _startup, _args, source.source_id_, feature.feature_id_);
                    request.desired_enabled_ = authority.desired_enabled_;
                    request.entitlement_granted_ = authority.entitlement_granted_;
                    request.resource_admitted_ = authority.resource_admitted_;
                }
                request.configuration_.schema_id_ = feature.configuration_schema_;
                request.configuration_.revision_ = _startup.has_usecase_control ?
                    _startup.usecase_activation.config_revision_ :
                    service_harness::g_config_revision;
                if (_startup.has_runtime_control && request.desired_enabled_ &&
                    request.entitlement_granted_ && request.resource_admitted_) {
                    const auto resolved =
                        vqec_vision_ai_appl_svfac_resolve_runtime_configuration(
                            _startup, source.source_id_, feature,
                            request.configuration_,
                            authorized_output_scopes[request_count]);
                    if (resolved.code_ != status_code::ok) {
                        return resolved;
                    }
                    request.association_.config_revision_ =
                        request.configuration_.revision_;
                }
                request_slots[request_count] = {source_slot, slot};
                record_slots_[request_count] = {source_slot, slot};
                ++request_count;
            }
        }

        if (request_count != 0) {
            feature_activation_snapshot snapshot;
            const auto reconciled = manager_.vqec_vision_ai_ftmgr_famgr_reconcile(
                requests, request_count, _registry, snapshot);
            if (reconciled.code_ != status_code::ok) {
                return reconciled;
            }
            std::array<std::array<std::vector<feature_stage*>,
                deployment_limits::g_max_models_per_source>,
                deployment_limits::g_max_sources> stages_by_slot{};
            for (std::uint16_t index = 0; index < request_count; ++index) {
                const auto* record =
                    manager_.vqec_vision_ai_ftmgr_famgr_get_record(index);
                if (record != nullptr && record->state_ == feature_effective_state::ready) {
                    output_scope_rule rule;
                    rule.source_id_ = record->source_id_;
                    rule.feature_id_ = record->feature_id_;
                    if (_startup.has_usecase_control) {
                        rule.attributes_ = _startup.has_runtime_control ?
                            authorized_output_scopes[index] :
                            record->association_.attribute_scopes_;
                    } else {
                        rule.attributes_.push_back(_fallback_attribute_schema);
                    }
                    policy.rules_.push_back(std::move(rule));
                }
                auto* stage = manager_.vqec_vision_ai_ftmgr_famgr_get_stage(index);
                if (stage != nullptr) {
                    stages_by_slot[request_slots[index].first]
                                  [request_slots[index].second].push_back(stage);
                }
            }
            for (std::uint16_t source_slot = 0; source_slot < _source_count;
                 ++source_slot) {
                for (std::uint16_t model_slot = 0;
                     model_slot < deployment_limits::g_max_models_per_source;
                     ++model_slot) {
                    auto& stages = stages_by_slot[source_slot][model_slot];
                    if (stages.empty() ||
                        stages.size() > feature_fanout_limits::g_max_feature_stages) {
                        continue;
                    }
                    std::array<feature_stage*,
                        feature_fanout_limits::g_max_feature_stages> stage_array{};
                    for (std::size_t index = 0; index < stages.size(); ++index) {
                        stage_array[index] = stages[index];
                    }
                    const std::size_t fanout_index =
                        static_cast<std::size_t>(source_slot) *
                            deployment_limits::g_max_models_per_source + model_slot;
                    fanouts_[fanout_index] = std::make_unique<feature_fanout>();
                    const auto fanout_configured = fanouts_[fanout_index]->
                        vqec_vision_ai_appl_ftfan_configure(
                            stage_array, static_cast<std::uint16_t>(stages.size()));
                    if (fanout_configured.code_ != status_code::ok) {
                        return fanout_configured;
                    }
                    wiring_.sources_[source_slot].fanouts_[model_slot] =
                        fanouts_[fanout_index].get();
                    has_wiring_ = true;
                }
            }
        }
    }

    vqec_vision_ai_appl_svfac_append_preview_rules(
        policy, _startup, _args, _deployment);
    if (_startup.fr_effectively_enabled) {
        vqec_vision_ai_appl_svfac_append_recognition_rules(
            policy, _deployment, _args.fr_feature_id,
            _args.fr_identity_attribute);
    }
    output_policy_ = policy;
    record_count_ = request_count;
    if (_apply_output_policy) {
        const auto applied = _output_gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0);
        if (applied.code_ != status_code::ok) {
            return applied;
        }
    }
    if (has_wiring_) {
        manager_.vqec_vision_ai_ftmgr_famgr_freeze();
    }
    return {};
}

status service_feature_activation::vqec_vision_ai_appl_svfac_adopt_compatible_owners(
    service_feature_activation& _live) {
    if (manager_.vqec_vision_ai_ftmgr_famgr_get_count() != record_count_ ||
        _live.manager_.vqec_vision_ai_ftmgr_famgr_get_count() !=
            _live.record_count_) {
        return {status_code::invalid_state,
            "feature activation candidate or live owner is incomplete"};
    }
    (void)manager_.vqec_vision_ai_ftmgr_famgr_adopt_compatible_owners(
        _live.manager_);
    for (std::uint16_t source_slot = 0; source_slot < wiring_.source_count_;
         ++source_slot) {
        for (std::uint16_t model_slot = 0;
             model_slot < deployment_limits::g_max_models_per_source; ++model_slot) {
            auto* fanout = wiring_.sources_[source_slot].fanouts_[model_slot];
            if (fanout == nullptr) {
                continue;
            }
            std::array<feature_stage*, feature_fanout_limits::g_max_feature_stages>
                stages{};
            std::uint16_t stage_count = 0;
            for (std::uint16_t record = 0; record < record_count_; ++record) {
                if (record_slots_[record].first != source_slot ||
                    record_slots_[record].second != model_slot) {
                    continue;
                }
                auto* stage = manager_.vqec_vision_ai_ftmgr_famgr_get_stage(record);
                if (stage != nullptr) {
                    stages[stage_count++] = stage;
                }
            }
            const auto replaced = fanout->vqec_vision_ai_appl_ftfan_replace_stages(
                stages, stage_count);
            if (replaced.code_ != status_code::ok) {
                return replaced;
            }
        }
    }
    return {};
}

const runtime_feature_activation*
service_feature_activation::vqec_vision_ai_appl_svfac_get_wiring() const noexcept {
    return has_wiring_ ? &wiring_ : nullptr;
}

const output_policy&
service_feature_activation::vqec_vision_ai_appl_svfac_get_output_policy() const noexcept {
    return output_policy_;
}

}  // namespace vqec::vision::ai
