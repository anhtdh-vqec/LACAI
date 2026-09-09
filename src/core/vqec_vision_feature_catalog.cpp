#include "vqec/vision/ai/contracts/vqec_vision_feature_catalog.hpp"

#include <new>
#include <utility>

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_ftcat_is_identifier(const std::string& _value) noexcept {
    if (_value.empty() || _value.size() > feature_catalog_limits::g_max_identifier_bytes) {
        return false;
    }
    for (const unsigned char character : _value) {
        if (!((character >= 'a' && character <= 'z') ||
              (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == '_' ||
              character == '-' || character == '.' || character == ':')) {
            return false;
        }
    }
    return true;
}

bool vqec_vision_ai_core_ftcat_is_input_mode(feature_input_mode _mode) noexcept {
    return _mode == feature_input_mode::single_model ||
        _mode == feature_input_mode::temporal_join;
}

status vqec_vision_ai_core_ftcat_validate_entry(
    const feature_catalog_entry& _feature) {
    if (!vqec_vision_ai_core_ftcat_is_identifier(_feature.feature_id_) ||
        !vqec_vision_ai_core_ftcat_is_identifier(_feature.feature_version_) ||
        !vqec_vision_ai_core_ftcat_is_identifier(_feature.processor_contract_) ||
        !vqec_vision_ai_core_ftcat_is_identifier(_feature.configuration_schema_) ||
        !vqec_vision_ai_core_ftcat_is_input_mode(_feature.input_mode_)) {
        return {status_code::invalid_argument, "invalid feature catalog identity"};
    }
    const auto model_count = _feature.model_dependencies_.size();
    if (model_count == 0 ||
        model_count > feature_catalog_limits::g_max_model_dependencies ||
        (_feature.input_mode_ == feature_input_mode::single_model && model_count != 1) ||
        (_feature.input_mode_ == feature_input_mode::temporal_join && model_count < 2)) {
        return {status_code::invalid_argument, "feature input mode and model count differ"};
    }
    for (std::size_t index = 0; index < model_count; ++index) {
        const auto& dependency = _feature.model_dependencies_[index];
        if (!vqec_vision_ai_core_ftcat_is_identifier(dependency.role_id_) ||
            !vqec_vision_ai_core_ftcat_is_identifier(dependency.model_id_)) {
            return {status_code::invalid_argument, "invalid feature model dependency"};
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            const auto& other = _feature.model_dependencies_[previous];
            if (other.role_id_ == dependency.role_id_ ||
                other.model_id_ == dependency.model_id_) {
                return {status_code::invalid_argument,
                    "duplicate feature model role or dependency"};
            }
        }
    }
    if (_feature.attribute_dependencies_.size() >
        feature_catalog_limits::g_max_attribute_dependencies) {
        return {status_code::resource_exhausted,
            "feature attribute dependency limit exceeded"};
    }
    for (std::size_t index = 0;
         index < _feature.attribute_dependencies_.size(); ++index) {
        const auto& dependency = _feature.attribute_dependencies_[index];
        if (!vqec_vision_ai_core_ftcat_is_identifier(dependency.schema_id_) ||
            !vqec_vision_ai_core_ftcat_is_identifier(dependency.schema_version_) ||
            dependency.max_age_ns_ == 0 || dependency.max_age_ns_ == UINT64_MAX) {
            return {status_code::invalid_argument, "invalid feature attribute dependency"};
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (_feature.attribute_dependencies_[previous].schema_id_ ==
                dependency.schema_id_) {
                return {status_code::invalid_argument,
                    "duplicate feature attribute schema"};
            }
        }
    }
    const auto& resources = _feature.resources_;
    if (resources.max_temporal_bytes_per_source_ >
            deployment_limits::g_max_temporal_bytes_per_source ||
        resources.max_events_per_update_ == 0 ||
        resources.max_events_per_update_ > feature_event_limits::g_max_events_per_batch ||
        resources.max_track_references_per_event_ == 0 ||
        resources.max_track_references_per_event_ >
            feature_event_limits::g_max_track_references_per_event ||
        resources.max_fields_per_event_ == 0 ||
        resources.max_fields_per_event_ > feature_event_limits::g_max_fields_per_event) {
        return {status_code::invalid_argument, "invalid feature resource profile"};
    }
    return {};
}

const model_catalog_entry* vqec_vision_ai_core_ftcat_find_model(
    const model_catalog& _catalog, const std::string& _model_id) noexcept {
    for (const auto& model : _catalog.models_) {
        if (model.model_id_ == _model_id) {
            return &model;
        }
    }
    return nullptr;
}

}  // namespace

status vqec_vision_ai_core_ftcat_validate_catalog(const feature_catalog& _catalog) {
    if (_catalog.schema_version_ != feature_catalog_limits::g_schema_version) {
        return {status_code::unsupported, "unsupported feature catalog schema"};
    }
    if (_catalog.revision_ == 0 || _catalog.revision_ == UINT64_MAX ||
        !vqec_vision_ai_core_ftcat_is_identifier(_catalog.catalog_id_) ||
        !vqec_vision_ai_core_ftcat_is_identifier(_catalog.model_catalog_ref_) ||
        _catalog.features_.empty() ||
        _catalog.features_.size() > feature_catalog_limits::g_max_features) {
        return {status_code::invalid_argument, "invalid feature catalog root"};
    }
    for (std::size_t index = 0; index < _catalog.features_.size(); ++index) {
        const auto valid =
            vqec_vision_ai_core_ftcat_validate_entry(_catalog.features_[index]);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (_catalog.features_[previous].feature_id_ ==
                _catalog.features_[index].feature_id_) {
                return {status_code::invalid_argument,
                    "duplicate feature catalog identity"};
            }
        }
    }
    return {};
}

status vqec_vision_ai_core_ftcat_validate_model_dependencies(
    const feature_catalog& _features, const model_catalog& _models) {
    const auto valid_features = vqec_vision_ai_core_ftcat_validate_catalog(_features);
    if (valid_features.code_ != status_code::ok) {
        return valid_features;
    }
    std::uint64_t catalog_bytes = 0;
    const auto valid_models =
        vqec_vision_ai_core_mdcat_validate_catalog(_models, catalog_bytes);
    if (valid_models.code_ != status_code::ok) {
        return valid_models;
    }
    if (_features.model_catalog_ref_ != _models.catalog_id_) {
        return {status_code::invalid_argument,
            "feature catalog references another model catalog"};
    }
    for (const auto& feature : _features.features_) {
        for (const auto& dependency : feature.model_dependencies_) {
            if (vqec_vision_ai_core_ftcat_find_model(
                    _models, dependency.model_id_) == nullptr) {
                return {status_code::invalid_argument,
                    "feature references an unknown model"};
            }
        }
    }
    return {};
}

status vqec_vision_ai_core_ftcat_validate_configuration(
    const feature_catalog_entry& _feature,
    const feature_configuration& _configuration) {
    const auto valid_feature = vqec_vision_ai_core_ftcat_validate_entry(_feature);
    if (valid_feature.code_ != status_code::ok) {
        return valid_feature;
    }
    if (_configuration.schema_id_ != _feature.configuration_schema_ ||
        _configuration.revision_ == 0 || _configuration.revision_ == UINT64_MAX) {
        return {status_code::invalid_argument,
            "feature configuration identity differs from catalog"};
    }
    if (_configuration.payload_.size() >
        feature_catalog_limits::g_max_configuration_bytes) {
        return {status_code::resource_exhausted,
            "feature configuration payload exceeds limit"};
    }
    return {};
}

status vqec_vision_ai_core_ftcat_compose_processor_config(
    const feature_catalog_entry& _feature, const std::string& _source_id,
    std::uint64_t _activation_revision, feature_processor_config& _config) {
    const auto valid_feature = vqec_vision_ai_core_ftcat_validate_entry(_feature);
    if (valid_feature.code_ != status_code::ok) {
        return valid_feature;
    }
    try {
        feature_processor_config candidate{
            _source_id,
            _feature.feature_id_,
            _activation_revision,
            _feature.resources_.max_events_per_update_,
            _feature.resources_.max_track_references_per_event_,
            _feature.resources_.max_fields_per_event_};
        const auto valid_config =
            vqec_vision_ai_core_ftevt_validate_processor_config(candidate);
        if (valid_config.code_ != status_code::ok) {
            return valid_config;
        }
        _config = std::move(candidate);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "feature processor configuration allocation failed"};
    }
    return {};
}

}  // namespace vqec::vision::ai
