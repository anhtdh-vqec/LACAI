#include "vqec_vision_feature_catalog.hpp"

#include <initializer_list>
#include <limits>
#include <new>
#include <set>
#include <utility>

#include <nlohmann/json.hpp>

namespace vqec::vision::ai {
namespace {

using feature_catalog_json = nlohmann::json;
struct invalid_feature_catalog_document {};

void vqec_vision_ai_ftmgr_ftcat_require_keys(
    const feature_catalog_json& _object,
    std::initializer_list<const char*> _keys) {
    if (!_object.is_object() || _object.size() != _keys.size()) {
        throw invalid_feature_catalog_document{};
    }
    for (const auto* key : _keys) {
        if (!_object.contains(key)) {
            throw invalid_feature_catalog_document{};
        }
    }
}

std::uint64_t vqec_vision_ai_ftmgr_ftcat_read_uint(
    const feature_catalog_json& _value) {
    if (!_value.is_number_unsigned()) {
        throw invalid_feature_catalog_document{};
    }
    return _value.get<std::uint64_t>();
}

std::size_t vqec_vision_ai_ftmgr_ftcat_read_size(
    const feature_catalog_json& _value) {
    const auto number = vqec_vision_ai_ftmgr_ftcat_read_uint(_value);
    if (number > std::numeric_limits<std::size_t>::max()) {
        throw invalid_feature_catalog_document{};
    }
    return static_cast<std::size_t>(number);
}

std::string vqec_vision_ai_ftmgr_ftcat_read_text(
    const feature_catalog_json& _value) {
    if (!_value.is_string()) {
        throw invalid_feature_catalog_document{};
    }
    auto text = _value.get<std::string>();
    if (text.empty() ||
        text.size() > feature_catalog_limits::g_max_identifier_bytes ||
        text.find('\0') != std::string::npos) {
        throw invalid_feature_catalog_document{};
    }
    return text;
}

feature_input_mode vqec_vision_ai_ftmgr_ftcat_read_input_mode(
    const feature_catalog_json& _value) {
    const auto name = vqec_vision_ai_ftmgr_ftcat_read_text(_value);
    if (name == "single_model") {
        return feature_input_mode::single_model;
    }
    if (name == "temporal_join") {
        return feature_input_mode::temporal_join;
    }
    throw invalid_feature_catalog_document{};
}

feature_model_dependency vqec_vision_ai_ftmgr_ftcat_read_model_dependency(
    const feature_catalog_json& _value) {
    vqec_vision_ai_ftmgr_ftcat_require_keys(_value, {"role_id", "model_id"});
    return {vqec_vision_ai_ftmgr_ftcat_read_text(_value.at("role_id")),
            vqec_vision_ai_ftmgr_ftcat_read_text(_value.at("model_id"))};
}

feature_attribute_dependency
vqec_vision_ai_ftmgr_ftcat_read_attribute_dependency(
    const feature_catalog_json& _value) {
    vqec_vision_ai_ftmgr_ftcat_require_keys(
        _value, {"schema_id", "schema_version", "max_age_ns"});
    return {vqec_vision_ai_ftmgr_ftcat_read_text(_value.at("schema_id")),
            vqec_vision_ai_ftmgr_ftcat_read_text(_value.at("schema_version")),
            vqec_vision_ai_ftmgr_ftcat_read_uint(_value.at("max_age_ns"))};
}

feature_catalog_entry vqec_vision_ai_ftmgr_ftcat_read_feature(
    const feature_catalog_json& _value) {
    vqec_vision_ai_ftmgr_ftcat_require_keys(
        _value, {"feature_id", "feature_version", "processor_contract",
                 "configuration_schema", "input_mode", "model_dependencies",
                 "attribute_dependencies", "resources"});
    feature_catalog_entry feature;
    feature.feature_id_ =
        vqec_vision_ai_ftmgr_ftcat_read_text(_value.at("feature_id"));
    feature.feature_version_ =
        vqec_vision_ai_ftmgr_ftcat_read_text(_value.at("feature_version"));
    feature.processor_contract_ =
        vqec_vision_ai_ftmgr_ftcat_read_text(_value.at("processor_contract"));
    feature.configuration_schema_ =
        vqec_vision_ai_ftmgr_ftcat_read_text(_value.at("configuration_schema"));
    feature.input_mode_ =
        vqec_vision_ai_ftmgr_ftcat_read_input_mode(_value.at("input_mode"));

    const auto& model_dependencies = _value.at("model_dependencies");
    if (!model_dependencies.is_array() || model_dependencies.empty() ||
        model_dependencies.size() > feature_catalog_limits::g_max_model_dependencies) {
        throw invalid_feature_catalog_document{};
    }
    feature.model_dependencies_.reserve(model_dependencies.size());
    for (const auto& dependency : model_dependencies) {
        feature.model_dependencies_.push_back(
            vqec_vision_ai_ftmgr_ftcat_read_model_dependency(dependency));
    }

    const auto& attribute_dependencies = _value.at("attribute_dependencies");
    if (!attribute_dependencies.is_array() ||
        attribute_dependencies.size() >
            feature_catalog_limits::g_max_attribute_dependencies) {
        throw invalid_feature_catalog_document{};
    }
    feature.attribute_dependencies_.reserve(attribute_dependencies.size());
    for (const auto& dependency : attribute_dependencies) {
        feature.attribute_dependencies_.push_back(
            vqec_vision_ai_ftmgr_ftcat_read_attribute_dependency(dependency));
    }

    const auto& resources = _value.at("resources");
    vqec_vision_ai_ftmgr_ftcat_require_keys(
        resources, {"max_temporal_bytes_per_source", "max_events_per_update",
                    "max_track_references_per_event", "max_fields_per_event"});
    feature.resources_.max_temporal_bytes_per_source_ =
        vqec_vision_ai_ftmgr_ftcat_read_uint(
            resources.at("max_temporal_bytes_per_source"));
    feature.resources_.max_events_per_update_ =
        vqec_vision_ai_ftmgr_ftcat_read_size(resources.at("max_events_per_update"));
    feature.resources_.max_track_references_per_event_ =
        vqec_vision_ai_ftmgr_ftcat_read_size(
            resources.at("max_track_references_per_event"));
    feature.resources_.max_fields_per_event_ =
        vqec_vision_ai_ftmgr_ftcat_read_size(resources.at("max_fields_per_event"));
    return feature;
}

}  // namespace

status vqec_vision_ai_ftmgr_ftcat_load_catalog(
    std::istream& _stream, feature_catalog& _catalog) {
    std::string document;
    try {
        document.reserve(8192);
        char byte = 0;
        while (_stream.get(byte)) {
            if (document.size() ==
                feature_catalog_document_limits::g_max_document_bytes) {
                return {status_code::resource_exhausted,
                    "feature catalog exceeds document limit"};
            }
            document.push_back(byte);
        }
    } catch (const std::ios_base::failure&) {
        if (!_stream.eof() || _stream.bad()) {
            return {status_code::io_error, "feature catalog stream read failed"};
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "feature catalog document allocation failed"};
    }
    if (_stream.bad() || (!_stream.eof() && _stream.fail())) {
        return {status_code::io_error, "cannot read feature catalog"};
    }
    try {
        std::vector<std::set<std::string>> object_keys;
        const auto callback = [&](int _depth,
                                  feature_catalog_json::parse_event_t _event,
                                  feature_catalog_json& _value) {
            if (_depth > feature_catalog_document_limits::g_max_json_depth) {
                throw invalid_feature_catalog_document{};
            }
            if (_event == feature_catalog_json::parse_event_t::object_start) {
                object_keys.emplace_back();
            } else if (_event == feature_catalog_json::parse_event_t::key) {
                if (object_keys.empty() ||
                    !object_keys.back().insert(_value.get<std::string>()).second) {
                    throw invalid_feature_catalog_document{};
                }
            } else if (_event == feature_catalog_json::parse_event_t::object_end) {
                object_keys.pop_back();
            }
            return true;
        };
        const auto root = feature_catalog_json::parse(document, callback);
        vqec_vision_ai_ftmgr_ftcat_require_keys(
            root, {"schema_version", "revision", "catalog_id",
                   "model_catalog_ref", "features"});
        feature_catalog candidate;
        const auto schema_version =
            vqec_vision_ai_ftmgr_ftcat_read_uint(root.at("schema_version"));
        if (schema_version > std::numeric_limits<std::uint32_t>::max()) {
            throw invalid_feature_catalog_document{};
        }
        candidate.schema_version_ = static_cast<std::uint32_t>(schema_version);
        candidate.revision_ =
            vqec_vision_ai_ftmgr_ftcat_read_uint(root.at("revision"));
        candidate.catalog_id_ =
            vqec_vision_ai_ftmgr_ftcat_read_text(root.at("catalog_id"));
        candidate.model_catalog_ref_ =
            vqec_vision_ai_ftmgr_ftcat_read_text(root.at("model_catalog_ref"));
        const auto& features = root.at("features");
        if (!features.is_array() || features.empty() ||
            features.size() > feature_catalog_limits::g_max_features) {
            throw invalid_feature_catalog_document{};
        }
        candidate.features_.reserve(features.size());
        for (const auto& feature : features) {
            candidate.features_.push_back(
                vqec_vision_ai_ftmgr_ftcat_read_feature(feature));
        }
        const auto valid = vqec_vision_ai_core_ftcat_validate_catalog(candidate);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        _catalog = std::move(candidate);
        return {};
    } catch (const invalid_feature_catalog_document&) {
        return {status_code::invalid_argument,
            "invalid feature catalog structure or value"};
    } catch (const feature_catalog_json::exception&) {
        return {status_code::invalid_argument,
            "malformed feature catalog JSON"};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "feature catalog parse allocation failed"};
    }
}

}  // namespace vqec::vision::ai
