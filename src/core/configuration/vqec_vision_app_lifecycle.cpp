#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_app_lifecycle.hpp"

#include <array>
#include <cmath>

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_applc_is_identifier(const std::string& _value) noexcept {
    return vqec_vision_ai_cntr_ident_is_valid(
        _value, app_lifecycle_limits::g_max_identifier_bytes);
}

bool vqec_vision_ai_core_applc_is_text(
    const std::string& _value, std::size_t _max_bytes) noexcept {
    return !_value.empty() && _value.size() <= _max_bytes;
}

bool vqec_vision_ai_core_applc_is_unique_identifier(
    const std::vector<std::string>& _values, std::size_t _index) noexcept {
    if (!vqec_vision_ai_core_applc_is_identifier(_values[_index])) {
        return false;
    }
    for (std::size_t previous = 0; previous < _index; ++previous) {
        if (_values[previous] == _values[_index]) {
            return false;
        }
    }
    return true;
}

}  // namespace

status vqec_vision_ai_core_applc_validate_manifest(
    const usecase_app_manifest& _manifest) {
    if (_manifest.schema_version_ != app_lifecycle_limits::g_schema_version ||
        !vqec_vision_ai_core_applc_is_identifier(_manifest.app_id_) ||
        _manifest.app_id_ != _manifest.usecase_id_ ||
        !vqec_vision_ai_core_applc_is_text(
            _manifest.app_version_, app_lifecycle_limits::g_max_version_bytes) ||
        !vqec_vision_ai_core_applc_is_text(
            _manifest.usecase_version_, app_lifecycle_limits::g_max_version_bytes) ||
        _manifest.release_sequence_ == 0 ||
        _manifest.runtime_abi_major_ != app_lifecycle_limits::g_runtime_abi_major ||
        _manifest.runtime_abi_minor_ != app_lifecycle_limits::g_runtime_abi_minor ||
        _manifest.target_ids_.empty() ||
        _manifest.target_ids_.size() > app_lifecycle_limits::g_max_targets ||
        _manifest.components_.empty() ||
        _manifest.components_.size() > app_lifecycle_limits::g_max_components ||
        _manifest.features_.empty() ||
        _manifest.features_.size() > app_lifecycle_limits::g_max_features ||
        !vqec_vision_ai_core_applc_is_identifier(
            _manifest.configuration_schema_id_) ||
        _manifest.configuration_schema_version_ !=
            app_lifecycle_limits::g_schema_version ||
        !vqec_vision_ai_cntr_ident_is_sha256_hex(
            _manifest.configuration_defaults_sha256_) ||
        _manifest.resources_.max_resident_bytes_ == 0 ||
        _manifest.resources_.max_tensor_bytes_ == 0 ||
        _manifest.resources_.max_tensor_bytes_ >
            _manifest.resources_.max_resident_bytes_ ||
        _manifest.resources_.max_active_incidents_ == 0 ||
        _manifest.resources_.max_active_incidents_ > 64U ||
        !std::isfinite(_manifest.resources_.max_events_per_second_) ||
        _manifest.resources_.max_events_per_second_ <= 0.0 ||
        _manifest.resources_.max_events_per_second_ > 64.0 ||
        !vqec_vision_ai_core_applc_is_identifier(_manifest.metadata_profile_ref_) ||
        _manifest.uninstall_purges_data_ ||
        !vqec_vision_ai_core_applc_is_text(
            _manifest.sbom_ref_, app_lifecycle_limits::g_max_reference_bytes) ||
        !vqec_vision_ai_core_applc_is_text(
            _manifest.provenance_ref_, app_lifecycle_limits::g_max_reference_bytes) ||
        !vqec_vision_ai_core_applc_is_text(
            _manifest.known_limits_ref_, app_lifecycle_limits::g_max_reference_bytes)) {
        return {status_code::invalid_argument, "invalid usecase app manifest identity"};
    }
    for (std::size_t index = 0; index < _manifest.target_ids_.size(); ++index) {
        if (!vqec_vision_ai_core_applc_is_unique_identifier(
                _manifest.target_ids_, index)) {
            return {status_code::invalid_argument, "invalid or duplicate app target"};
        }
    }
    for (std::size_t index = 0; index < _manifest.components_.size(); ++index) {
        const auto& component = _manifest.components_[index];
        if (!vqec_vision_ai_core_applc_is_identifier(component.component_id_) ||
            !vqec_vision_ai_core_applc_is_text(
                component.component_version_, app_lifecycle_limits::g_max_version_bytes) ||
            !vqec_vision_ai_core_applc_is_identifier(component.target_id_) ||
            !vqec_vision_ai_cntr_ident_is_sha256_hex(component.artifact_sha256_) ||
            component.artifact_bytes_ == 0 ||
            component.artifact_bytes_ > app_lifecycle_limits::g_max_component_bytes ||
            !vqec_vision_ai_cntr_ident_is_sha256_hex(
                component.semantic_contract_sha256_)) {
            return {status_code::invalid_argument, "invalid app component"};
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            const auto& other = _manifest.components_[previous];
            if (other.component_id_ == component.component_id_ &&
                other.component_version_ == component.component_version_ &&
                other.target_id_ == component.target_id_) {
                return {status_code::invalid_argument, "duplicate app component identity"};
            }
        }
    }
    for (std::size_t index = 0; index < _manifest.features_.size(); ++index) {
        const auto& feature = _manifest.features_[index];
        if (!vqec_vision_ai_core_applc_is_identifier(feature.feature_id_) ||
            !vqec_vision_ai_core_applc_is_identifier(feature.processor_contract_) ||
            !vqec_vision_ai_core_applc_is_identifier(
                feature.configuration_schema_id_) ||
            feature.configuration_schema_id_ != _manifest.configuration_schema_id_) {
            return {status_code::invalid_argument, "invalid app feature binding"};
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (_manifest.features_[previous].feature_id_ == feature.feature_id_) {
                return {status_code::invalid_argument, "duplicate app feature identity"};
            }
        }
    }
    const std::array<const std::vector<std::string>*, 4> scope_lists{{
        &_manifest.requested_scopes_.sources_, &_manifest.requested_scopes_.outputs_,
        &_manifest.requested_scopes_.queries_, &_manifest.requested_scopes_.evidence_}};
    for (const auto* scopes : scope_lists) {
        if (scopes->size() > app_lifecycle_limits::g_max_scopes) {
            return {status_code::resource_exhausted, "app scope count exceeds limit"};
        }
        for (std::size_t index = 0; index < scopes->size(); ++index) {
            if (!vqec_vision_ai_core_applc_is_unique_identifier(*scopes, index)) {
                return {status_code::invalid_argument, "invalid or duplicate app scope"};
            }
        }
    }
    return {};
}

status vqec_vision_ai_core_applc_validate_entitlement(
    const app_entitlement_grant& _grant) {
    if (_grant.schema_version_ != app_lifecycle_limits::g_schema_version ||
        !vqec_vision_ai_core_applc_is_identifier(_grant.grant_id_) ||
        _grant.grant_revision_ == 0 || _grant.expected_entitlement_revision_ == 0 ||
        !vqec_vision_ai_core_applc_is_identifier(_grant.issuer_id_) ||
        !vqec_vision_ai_core_applc_is_identifier(_grant.key_id_) ||
        !vqec_vision_ai_core_applc_is_identifier(_grant.customer_id_) ||
        !vqec_vision_ai_core_applc_is_identifier(_grant.device_id_) ||
        !vqec_vision_ai_core_applc_is_identifier(_grant.target_id_) ||
        !vqec_vision_ai_core_applc_is_identifier(_grant.app_id_) ||
        !vqec_vision_ai_core_applc_is_identifier(_grant.source_id_) ||
        _grant.not_before_utc_ns_ == 0 ||
        _grant.expires_utc_ns_ <= _grant.not_before_utc_ns_ ||
        _grant.output_scopes_.size() > app_lifecycle_limits::g_max_scopes ||
        (!_grant.granted_ && !_grant.output_scopes_.empty())) {
        return {status_code::invalid_argument, "invalid app entitlement grant"};
    }
    for (std::size_t index = 0; index < _grant.output_scopes_.size(); ++index) {
        if (!vqec_vision_ai_core_applc_is_unique_identifier(
                _grant.output_scopes_, index)) {
            return {status_code::invalid_argument,
                "invalid or duplicate entitlement output scope"};
        }
    }
    return {};
}

status vqec_vision_ai_core_applc_validate_runtime_snapshot(
    const runtime_control_snapshot& _snapshot) {
    if (_snapshot.schema_version_ != app_lifecycle_limits::g_schema_version ||
        _snapshot.snapshot_revision_ == 0 || _snapshot.inventory_revision_ == 0 ||
        _snapshot.entitlement_revision_ == 0 || _snapshot.desired_revision_ == 0 ||
        _snapshot.associations_.size() > app_lifecycle_limits::g_max_associations) {
        return {status_code::invalid_argument, "invalid runtime control snapshot identity"};
    }
    for (std::size_t index = 0; index < _snapshot.associations_.size(); ++index) {
        const auto& association = _snapshot.associations_[index];
        if (!vqec_vision_ai_core_applc_is_identifier(association.app_id_) ||
            !vqec_vision_ai_core_applc_is_identifier(association.source_id_) ||
            !vqec_vision_ai_core_applc_is_text(
                association.app_version_, app_lifecycle_limits::g_max_version_bytes) ||
            association.release_sequence_ == 0 ||
            association.configuration_revision_ == 0 ||
            !vqec_vision_ai_cntr_ident_is_sha256_hex(
                association.configuration_sha256_) ||
            !vqec_vision_ai_core_applc_is_identifier(
                association.configuration_schema_id_) ||
            association.configuration_payload_.empty() ||
            association.configuration_payload_.size() >
                app_lifecycle_limits::g_max_document_bytes ||
            association.output_scopes_.size() > app_lifecycle_limits::g_max_scopes ||
            association.components_.empty() ||
            association.components_.size() > app_lifecycle_limits::g_max_components ||
            (association.entitled_ && association.entitlement_expires_utc_ns_ == 0) ||
            (!association.entitled_ && !association.output_scopes_.empty()) ||
            (association.desired_ && !association.installed_)) {
            return {status_code::invalid_argument, "invalid runtime app association"};
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            const auto& other = _snapshot.associations_[previous];
            if (other.app_id_ == association.app_id_ &&
                other.source_id_ == association.source_id_) {
                return {status_code::invalid_argument,
                    "duplicate runtime app association"};
            }
        }
        for (std::size_t scope = 0; scope < association.output_scopes_.size(); ++scope) {
            if (!vqec_vision_ai_core_applc_is_unique_identifier(
                    association.output_scopes_, scope)) {
                return {status_code::invalid_argument,
                    "invalid runtime output scope"};
            }
        }
        for (std::size_t component_index = 0;
             component_index < association.components_.size(); ++component_index) {
            const auto& component = association.components_[component_index];
            if (!vqec_vision_ai_core_applc_is_identifier(component.component_id_) ||
                !vqec_vision_ai_core_applc_is_text(component.component_version_,
                    app_lifecycle_limits::g_max_version_bytes) ||
                !vqec_vision_ai_core_applc_is_identifier(component.target_id_) ||
                !vqec_vision_ai_cntr_ident_is_sha256_hex(component.artifact_sha256_) ||
                component.artifact_bytes_ == 0 ||
                component.artifact_bytes_ > app_lifecycle_limits::g_max_component_bytes ||
                !vqec_vision_ai_cntr_ident_is_sha256_hex(
                    component.semantic_contract_sha256_) ||
                component.immutable_location_.empty() ||
                component.immutable_location_.front() != '/' ||
                component.immutable_location_.find('\0') != std::string::npos ||
                component.immutable_location_.find("..") != std::string::npos) {
                return {status_code::invalid_argument,
                    "invalid runtime app component"};
            }
            for (std::size_t previous = 0; previous < component_index; ++previous) {
                const auto& other = association.components_[previous];
                if (other.component_id_ == component.component_id_ &&
                    other.component_version_ == component.component_version_ &&
                    other.target_id_ == component.target_id_) {
                    return {status_code::invalid_argument,
                        "duplicate runtime app component"};
                }
            }
        }
    }
    return {};
}

status vqec_vision_ai_core_applc_validate_catalog(
    const usecase_app_catalog& _catalog) {
    if (_catalog.schema_version_ != app_lifecycle_limits::g_schema_version ||
        !vqec_vision_ai_core_applc_is_identifier(_catalog.catalog_id_) ||
        _catalog.revision_ == 0 || _catalog.applications_.empty() ||
        _catalog.applications_.size() > app_lifecycle_limits::g_max_applications) {
        return {status_code::invalid_argument, "invalid usecase app catalog identity"};
    }
    for (std::size_t index = 0; index < _catalog.applications_.size(); ++index) {
        const auto& application = _catalog.applications_[index];
        if (!vqec_vision_ai_core_applc_is_text(application.catalog_code_,
                app_lifecycle_limits::g_max_catalog_code_bytes) ||
            !vqec_vision_ai_core_applc_is_identifier(application.app_id_) ||
            !vqec_vision_ai_core_applc_is_text(application.display_name_,
                app_lifecycle_limits::g_max_display_name_bytes) ||
            !vqec_vision_ai_core_applc_is_text(application.app_version_,
                app_lifecycle_limits::g_max_version_bytes)) {
            return {status_code::invalid_argument, "invalid app catalog entry"};
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            const auto& other = _catalog.applications_[previous];
            if (other.catalog_code_ == application.catalog_code_ ||
                other.app_id_ == application.app_id_) {
                return {status_code::invalid_argument,
                    "duplicate app catalog identity"};
            }
        }
    }
    return {};
}

}  // namespace vqec::vision::ai
