#include "vqec_vision_usecase_config.hpp"

#include <initializer_list>
#include <limits>
#include <new>
#include <set>
#include <utility>

#include <nlohmann/json.hpp>

namespace vqec::vision::ai {
namespace {

using usecase_json = nlohmann::json;
struct invalid_usecase_config {};

void vqec_vision_ai_ftmgr_ucfg_require_keys(
    const usecase_json& _object, std::initializer_list<const char*> _keys) {
    if (!_object.is_object() || _object.size() != _keys.size()) {
        throw invalid_usecase_config{};
    }
    for (const auto* key : _keys) {
        if (!_object.contains(key)) {
            throw invalid_usecase_config{};
        }
    }
}

std::uint64_t vqec_vision_ai_ftmgr_ucfg_read_uint(const usecase_json& _value) {
    if (!_value.is_number_unsigned()) {
        throw invalid_usecase_config{};
    }
    return _value.get<std::uint64_t>();
}

std::string vqec_vision_ai_ftmgr_ucfg_read_text(const usecase_json& _value) {
    if (!_value.is_string()) {
        throw invalid_usecase_config{};
    }
    auto value = _value.get<std::string>();
    if (value.empty() || value.size() > usecase_activation_limits::g_max_identifier_bytes ||
        value.find('\0') != std::string::npos) {
        throw invalid_usecase_config{};
    }
    return value;
}

bool vqec_vision_ai_ftmgr_ucfg_read_bool(const usecase_json& _value) {
    if (!_value.is_boolean()) {
        throw invalid_usecase_config{};
    }
    return _value.get<bool>();
}

std::vector<std::string> vqec_vision_ai_ftmgr_ucfg_read_identifiers(
    const usecase_json& _value, std::size_t _maximum, bool _require_nonempty) {
    if (!_value.is_array() || (_require_nonempty && _value.empty()) ||
        _value.size() > _maximum) {
        throw invalid_usecase_config{};
    }
    std::vector<std::string> identifiers;
    identifiers.reserve(_value.size());
    for (const auto& item : _value) {
        identifiers.push_back(vqec_vision_ai_ftmgr_ucfg_read_text(item));
    }
    return identifiers;
}

}  // namespace

status vqec_vision_ai_ftmgr_ucfg_load_snapshot(
    std::istream& _stream, usecase_control_snapshot& _snapshot) {
    std::string document;
    try {
        document.reserve(4096);
        char byte = 0;
        while (_stream.get(byte)) {
            if (document.size() == usecase_config_limits::g_max_document_bytes) {
                return {status_code::resource_exhausted,
                    "usecase snapshot exceeds document limit"};
            }
            document.push_back(byte);
        }
    } catch (const std::ios_base::failure&) {
        if (!_stream.eof() || _stream.bad()) {
            return {status_code::io_error, "usecase snapshot stream read failed"};
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "usecase snapshot document allocation failed"};
    }
    if (_stream.bad() || (!_stream.eof() && _stream.fail())) {
        return {status_code::io_error, "cannot read usecase snapshot"};
    }
    try {
        std::vector<std::set<std::string>> object_keys;
        const auto callback = [&](int _depth, usecase_json::parse_event_t _event,
                                  usecase_json& _value) {
            if (_depth > usecase_config_limits::g_max_json_depth) {
                throw invalid_usecase_config{};
            }
            if (_event == usecase_json::parse_event_t::object_start) {
                object_keys.emplace_back();
            } else if (_event == usecase_json::parse_event_t::key) {
                if (object_keys.empty() ||
                    !object_keys.back().insert(_value.get<std::string>()).second) {
                    throw invalid_usecase_config{};
                }
            } else if (_event == usecase_json::parse_event_t::object_end) {
                object_keys.pop_back();
            }
            return true;
        };
        const auto root = usecase_json::parse(document, callback);
        vqec_vision_ai_ftmgr_ucfg_require_keys(root,
            {"schema_version", "control_revision", "entitlement_revision",
             "deployment_revision", "catalog", "associations"});
        if (vqec_vision_ai_ftmgr_ucfg_read_uint(root.at("schema_version")) !=
            usecase_activation_limits::g_schema_version) {
            return {status_code::unsupported, "unsupported usecase snapshot schema"};
        }
        usecase_control_snapshot candidate;
        candidate.control_revision_ =
            vqec_vision_ai_ftmgr_ucfg_read_uint(root.at("control_revision"));
        candidate.entitlement_revision_ =
            vqec_vision_ai_ftmgr_ucfg_read_uint(root.at("entitlement_revision"));
        candidate.deployment_revision_ =
            vqec_vision_ai_ftmgr_ucfg_read_uint(root.at("deployment_revision"));
        if (candidate.control_revision_ == 0 || candidate.entitlement_revision_ == 0 ||
            candidate.deployment_revision_ == 0) {
            throw invalid_usecase_config{};
        }

        const auto& catalog = root.at("catalog");
        vqec_vision_ai_ftmgr_ucfg_require_keys(catalog,
            {"schema_version", "revision", "catalog_id", "model_catalog_ref", "usecases"});
        const auto catalog_schema =
            vqec_vision_ai_ftmgr_ucfg_read_uint(catalog.at("schema_version"));
        if (catalog_schema > std::numeric_limits<std::uint32_t>::max()) {
            throw invalid_usecase_config{};
        }
        candidate.catalog_.schema_version_ = static_cast<std::uint32_t>(catalog_schema);
        candidate.catalog_.revision_ =
            vqec_vision_ai_ftmgr_ucfg_read_uint(catalog.at("revision"));
        candidate.catalog_.catalog_id_ =
            vqec_vision_ai_ftmgr_ucfg_read_text(catalog.at("catalog_id"));
        candidate.catalog_.model_catalog_ref_ =
            vqec_vision_ai_ftmgr_ucfg_read_text(catalog.at("model_catalog_ref"));
        const auto& usecases = catalog.at("usecases");
        if (!usecases.is_array() || usecases.empty() ||
            usecases.size() > usecase_activation_limits::g_max_usecases) {
            throw invalid_usecase_config{};
        }
        candidate.catalog_.usecases_.reserve(usecases.size());
        for (const auto& value : usecases) {
            vqec_vision_ai_ftmgr_ucfg_require_keys(value,
                {"usecase_id", "usecase_version", "root_model_ids", "feature_ids"});
            usecase_catalog_entry entry;
            entry.usecase_id_ =
                vqec_vision_ai_ftmgr_ucfg_read_text(value.at("usecase_id"));
            entry.usecase_version_ =
                vqec_vision_ai_ftmgr_ucfg_read_text(value.at("usecase_version"));
            entry.root_model_ids_ = vqec_vision_ai_ftmgr_ucfg_read_identifiers(
                value.at("root_model_ids"), usecase_activation_limits::g_max_root_models, true);
            entry.feature_ids_ = vqec_vision_ai_ftmgr_ucfg_read_identifiers(
                value.at("feature_ids"), usecase_activation_limits::g_max_feature_ids, false);
            candidate.catalog_.usecases_.push_back(std::move(entry));
        }

        const auto& associations = root.at("associations");
        if (!associations.is_array() || associations.empty() ||
            associations.size() > usecase_activation_limits::g_max_associations) {
            throw invalid_usecase_config{};
        }
        candidate.requests_.reserve(associations.size());
        for (const auto& value : associations) {
            vqec_vision_ai_ftmgr_ucfg_require_keys(value,
                {"source_id", "usecase_id", "desired", "installed", "entitled",
                 "supported", "compatible", "admitted"});
            candidate.requests_.push_back({
                vqec_vision_ai_ftmgr_ucfg_read_text(value.at("source_id")),
                vqec_vision_ai_ftmgr_ucfg_read_text(value.at("usecase_id")),
                vqec_vision_ai_ftmgr_ucfg_read_bool(value.at("desired")),
                vqec_vision_ai_ftmgr_ucfg_read_bool(value.at("installed")),
                vqec_vision_ai_ftmgr_ucfg_read_bool(value.at("entitled")),
                vqec_vision_ai_ftmgr_ucfg_read_bool(value.at("supported")),
                vqec_vision_ai_ftmgr_ucfg_read_bool(value.at("compatible")),
                vqec_vision_ai_ftmgr_ucfg_read_bool(value.at("admitted"))});
        }
        _snapshot = std::move(candidate);
        return {};
    } catch (const invalid_usecase_config&) {
        return {status_code::invalid_argument,
            "invalid usecase snapshot structure or value"};
    } catch (const usecase_json::exception&) {
        return {status_code::invalid_argument, "malformed usecase snapshot JSON"};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "usecase snapshot parse allocation failed"};
    }
}

}  // namespace vqec::vision::ai
