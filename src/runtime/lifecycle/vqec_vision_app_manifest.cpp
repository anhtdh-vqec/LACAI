#include "vqec_vision_app_manifest.hpp"

#include <initializer_list>
#include <new>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace vqec::vision::ai {
namespace {

using nlohmann::json;

struct invalid_app_manifest {};

void vqec_vision_ai_lifec_apmft_require_keys(const json& _value,
    std::initializer_list<const char*> _required,
    std::initializer_list<const char*> _optional = {}) {
    if (!_value.is_object() ||
        _value.size() < _required.size() ||
        _value.size() > _required.size() + _optional.size()) {
        throw invalid_app_manifest{};
    }
    for (const auto* key : _required) {
        if (!_value.contains(key)) {
            throw invalid_app_manifest{};
        }
    }
    for (auto iterator = _value.begin(); iterator != _value.end(); ++iterator) {
        bool known = false;
        for (const auto* key : _required) {
            known = known || iterator.key() == key;
        }
        for (const auto* key : _optional) {
            known = known || iterator.key() == key;
        }
        if (!known) {
            throw invalid_app_manifest{};
        }
    }
}

std::string vqec_vision_ai_lifec_apmft_read_text(
    const json& _value, std::size_t _max_bytes) {
    if (!_value.is_string()) {
        throw invalid_app_manifest{};
    }
    auto result = _value.get<std::string>();
    if (result.empty() || result.size() > _max_bytes) {
        throw invalid_app_manifest{};
    }
    return result;
}

std::uint64_t vqec_vision_ai_lifec_apmft_read_uint(const json& _value) {
    if (!_value.is_number_unsigned()) {
        throw invalid_app_manifest{};
    }
    return _value.get<std::uint64_t>();
}

bool vqec_vision_ai_lifec_apmft_read_bool(const json& _value) {
    if (!_value.is_boolean()) {
        throw invalid_app_manifest{};
    }
    return _value.get<bool>();
}

double vqec_vision_ai_lifec_apmft_read_number(const json& _value) {
    if (!_value.is_number()) {
        throw invalid_app_manifest{};
    }
    return _value.get<double>();
}

std::vector<std::string> vqec_vision_ai_lifec_apmft_read_text_array(
    const json& _value, std::size_t _max_count, bool _allow_empty) {
    if (!_value.is_array() || (!_allow_empty && _value.empty()) ||
        _value.size() > _max_count) {
        throw invalid_app_manifest{};
    }
    std::vector<std::string> result;
    result.reserve(_value.size());
    for (const auto& item : _value) {
        result.push_back(vqec_vision_ai_lifec_apmft_read_text(
            item, app_lifecycle_limits::g_max_identifier_bytes));
    }
    return result;
}

app_component_type vqec_vision_ai_lifec_apmft_read_component_type(
    const json& _value) {
    const auto text = vqec_vision_ai_lifec_apmft_read_text(
        _value, app_lifecycle_limits::g_max_identifier_bytes);
    if (text == "model") {
        return app_component_type::model;
    }
    if (text == "labels") {
        return app_component_type::labels;
    }
    if (text == "ontology") {
        return app_component_type::ontology;
    }
    if (text == "rules") {
        return app_component_type::rules;
    }
    if (text == "configuration") {
        return app_component_type::configuration;
    }
    throw invalid_app_manifest{};
}

app_model_role vqec_vision_ai_lifec_apmft_read_model_role(
    const json& _value) {
    const auto text = vqec_vision_ai_lifec_apmft_read_text(
        _value, app_lifecycle_limits::g_max_identifier_bytes);
    if (text == "primary") {
        return app_model_role::primary;
    }
    if (text == "secondary") {
        return app_model_role::secondary;
    }
    if (text == "offline") {
        return app_model_role::offline;
    }
    throw invalid_app_manifest{};
}

}  // namespace

status vqec_vision_ai_lifec_apmft_load(
    std::istream& _stream, usecase_app_manifest& _manifest) {
    std::string document;
    try {
        document.reserve(8192);
        char byte = 0;
        while (_stream.get(byte)) {
            if (document.size() == app_lifecycle_limits::g_max_document_bytes) {
                return {status_code::resource_exhausted,
                    "usecase app manifest exceeds document limit"};
            }
            document.push_back(byte);
        }
    } catch (const std::ios_base::failure&) {
        if (!_stream.eof() || _stream.bad()) {
            return {status_code::io_error, "usecase app manifest read failed"};
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "usecase app manifest allocation failed"};
    }
    if (_stream.bad() || (!_stream.eof() && _stream.fail())) {
        return {status_code::io_error, "cannot read usecase app manifest"};
    }

    try {
        const auto depth_guard = [](int _depth, json::parse_event_t _event, json&) -> bool {
            if (_depth > static_cast<int>(app_lifecycle_limits::g_max_json_depth)) {
                throw invalid_app_manifest{};
            }
            return _event != json::parse_event_t::key ||
                _depth <= static_cast<int>(app_lifecycle_limits::g_max_json_depth);
        };
        const json root = json::parse(document, depth_guard);
        vqec_vision_ai_lifec_apmft_require_keys(root,
            {"schema_version", "app_id", "usecase_id", "app_version",
                "usecase_version", "release_sequence", "runtime_abi", "target_ids",
                "components", "features", "configuration", "requested_scopes",
                "resources", "data_policy", "supply_chain"},
            {"rollback_predecessor"});

        usecase_app_manifest candidate;
        candidate.schema_version_ = static_cast<std::uint32_t>(
            vqec_vision_ai_lifec_apmft_read_uint(root.at("schema_version")));
        candidate.app_id_ = vqec_vision_ai_lifec_apmft_read_text(
            root.at("app_id"), app_lifecycle_limits::g_max_identifier_bytes);
        candidate.usecase_id_ = vqec_vision_ai_lifec_apmft_read_text(
            root.at("usecase_id"), app_lifecycle_limits::g_max_identifier_bytes);
        candidate.app_version_ = vqec_vision_ai_lifec_apmft_read_text(
            root.at("app_version"), app_lifecycle_limits::g_max_version_bytes);
        candidate.usecase_version_ = vqec_vision_ai_lifec_apmft_read_text(
            root.at("usecase_version"), app_lifecycle_limits::g_max_version_bytes);
        candidate.release_sequence_ =
            vqec_vision_ai_lifec_apmft_read_uint(root.at("release_sequence"));

        const auto& runtime_abi = root.at("runtime_abi");
        vqec_vision_ai_lifec_apmft_require_keys(runtime_abi, {"major", "minor"});
        candidate.runtime_abi_major_ = static_cast<std::uint32_t>(
            vqec_vision_ai_lifec_apmft_read_uint(runtime_abi.at("major")));
        candidate.runtime_abi_minor_ = static_cast<std::uint32_t>(
            vqec_vision_ai_lifec_apmft_read_uint(runtime_abi.at("minor")));
        candidate.target_ids_ = vqec_vision_ai_lifec_apmft_read_text_array(
            root.at("target_ids"), app_lifecycle_limits::g_max_targets, false);

        const auto& components = root.at("components");
        if (!components.is_array() || components.empty() ||
            components.size() > app_lifecycle_limits::g_max_components) {
            throw invalid_app_manifest{};
        }
        candidate.components_.reserve(components.size());
        for (const auto& value : components) {
            vqec_vision_ai_lifec_apmft_require_keys(value,
                {"component_id", "component_version", "component_type", "target_id",
                    "artifact_sha256", "semantic_contract_sha256", "required"},
                {"model_role", "quality_receipt_ref"});
            app_component_manifest component;
            component.component_id_ = vqec_vision_ai_lifec_apmft_read_text(
                value.at("component_id"), app_lifecycle_limits::g_max_identifier_bytes);
            component.component_version_ = vqec_vision_ai_lifec_apmft_read_text(
                value.at("component_version"), app_lifecycle_limits::g_max_version_bytes);
            component.type_ =
                vqec_vision_ai_lifec_apmft_read_component_type(value.at("component_type"));
            component.target_id_ = vqec_vision_ai_lifec_apmft_read_text(
                value.at("target_id"), app_lifecycle_limits::g_max_identifier_bytes);
            component.artifact_sha256_ = vqec_vision_ai_lifec_apmft_read_text(
                value.at("artifact_sha256"), 64);
            component.semantic_contract_sha256_ = vqec_vision_ai_lifec_apmft_read_text(
                value.at("semantic_contract_sha256"), 64);
            component.required_ =
                vqec_vision_ai_lifec_apmft_read_bool(value.at("required"));
            if (value.contains("model_role")) {
                component.model_role_ =
                    vqec_vision_ai_lifec_apmft_read_model_role(value.at("model_role"));
            }
            if (value.contains("quality_receipt_ref")) {
                component.quality_receipt_ref_ = vqec_vision_ai_lifec_apmft_read_text(
                    value.at("quality_receipt_ref"),
                    app_lifecycle_limits::g_max_reference_bytes);
            }
            candidate.components_.push_back(std::move(component));
        }

        const auto& features = root.at("features");
        if (!features.is_array() || features.empty() ||
            features.size() > app_lifecycle_limits::g_max_features) {
            throw invalid_app_manifest{};
        }
        candidate.features_.reserve(features.size());
        for (const auto& value : features) {
            vqec_vision_ai_lifec_apmft_require_keys(value,
                {"feature_id", "processor_contract", "configuration_schema_id"});
            app_feature_manifest feature;
            feature.feature_id_ = vqec_vision_ai_lifec_apmft_read_text(
                value.at("feature_id"), app_lifecycle_limits::g_max_identifier_bytes);
            feature.processor_contract_ = vqec_vision_ai_lifec_apmft_read_text(
                value.at("processor_contract"), app_lifecycle_limits::g_max_identifier_bytes);
            feature.configuration_schema_id_ = vqec_vision_ai_lifec_apmft_read_text(
                value.at("configuration_schema_id"),
                app_lifecycle_limits::g_max_identifier_bytes);
            candidate.features_.push_back(std::move(feature));
        }

        const auto& configuration = root.at("configuration");
        vqec_vision_ai_lifec_apmft_require_keys(configuration,
            {"schema_id", "schema_version", "defaults_sha256"});
        candidate.configuration_schema_id_ = vqec_vision_ai_lifec_apmft_read_text(
            configuration.at("schema_id"), app_lifecycle_limits::g_max_identifier_bytes);
        candidate.configuration_schema_version_ = static_cast<std::uint32_t>(
            vqec_vision_ai_lifec_apmft_read_uint(configuration.at("schema_version")));
        candidate.configuration_defaults_sha256_ =
            vqec_vision_ai_lifec_apmft_read_text(
                configuration.at("defaults_sha256"), 64);

        const auto& scopes = root.at("requested_scopes");
        vqec_vision_ai_lifec_apmft_require_keys(
            scopes, {"sources", "outputs", "queries", "evidence"});
        candidate.requested_scopes_.sources_ =
            vqec_vision_ai_lifec_apmft_read_text_array(
                scopes.at("sources"), app_lifecycle_limits::g_max_sources, true);
        candidate.requested_scopes_.outputs_ =
            vqec_vision_ai_lifec_apmft_read_text_array(
                scopes.at("outputs"), app_lifecycle_limits::g_max_scopes, true);
        candidate.requested_scopes_.queries_ =
            vqec_vision_ai_lifec_apmft_read_text_array(
                scopes.at("queries"), app_lifecycle_limits::g_max_scopes, true);
        candidate.requested_scopes_.evidence_ =
            vqec_vision_ai_lifec_apmft_read_text_array(
                scopes.at("evidence"), app_lifecycle_limits::g_max_scopes, true);

        const auto& resources = root.at("resources");
        vqec_vision_ai_lifec_apmft_require_keys(resources,
            {"max_resident_bytes", "max_tensor_bytes", "max_active_incidents",
                "max_events_per_second"});
        candidate.resources_.max_resident_bytes_ =
            vqec_vision_ai_lifec_apmft_read_uint(resources.at("max_resident_bytes"));
        candidate.resources_.max_tensor_bytes_ =
            vqec_vision_ai_lifec_apmft_read_uint(resources.at("max_tensor_bytes"));
        candidate.resources_.max_active_incidents_ = static_cast<std::size_t>(
            vqec_vision_ai_lifec_apmft_read_uint(resources.at("max_active_incidents")));
        candidate.resources_.max_events_per_second_ =
            vqec_vision_ai_lifec_apmft_read_number(resources.at("max_events_per_second"));

        const auto& data_policy = root.at("data_policy");
        vqec_vision_ai_lifec_apmft_require_keys(
            data_policy, {"metadata_profile_ref", "uninstall_purges_data"});
        candidate.metadata_profile_ref_ = vqec_vision_ai_lifec_apmft_read_text(
            data_policy.at("metadata_profile_ref"),
            app_lifecycle_limits::g_max_identifier_bytes);
        candidate.uninstall_purges_data_ = vqec_vision_ai_lifec_apmft_read_bool(
            data_policy.at("uninstall_purges_data"));

        const auto& supply_chain = root.at("supply_chain");
        vqec_vision_ai_lifec_apmft_require_keys(
            supply_chain, {"sbom_ref", "provenance_ref", "known_limits_ref"});
        candidate.sbom_ref_ = vqec_vision_ai_lifec_apmft_read_text(
            supply_chain.at("sbom_ref"), app_lifecycle_limits::g_max_reference_bytes);
        candidate.provenance_ref_ = vqec_vision_ai_lifec_apmft_read_text(
            supply_chain.at("provenance_ref"),
            app_lifecycle_limits::g_max_reference_bytes);
        candidate.known_limits_ref_ = vqec_vision_ai_lifec_apmft_read_text(
            supply_chain.at("known_limits_ref"),
            app_lifecycle_limits::g_max_reference_bytes);
        if (root.contains("rollback_predecessor")) {
            candidate.rollback_predecessor_ = vqec_vision_ai_lifec_apmft_read_text(
                root.at("rollback_predecessor"), app_lifecycle_limits::g_max_version_bytes);
        }

        const auto valid = vqec_vision_ai_core_applc_validate_manifest(candidate);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        _manifest = std::move(candidate);
        return {};
    } catch (const invalid_app_manifest&) {
        return {status_code::invalid_argument, "invalid usecase app manifest document"};
    } catch (const json::exception&) {
        return {status_code::invalid_argument, "invalid usecase app manifest JSON"};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "usecase app manifest parse allocation failed"};
    }
}

}  // namespace vqec::vision::ai

