#include "vqec_vision_runtime_control_snapshot.hpp"

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <new>
#include <set>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "vqec/vision/ai/contracts/features/vqec_vision_feature_catalog.hpp"

namespace vqec::vision::ai {
namespace {

using json = nlohmann::json;
struct invalid_runtime_control_snapshot {};

void vqec_vision_ai_lifec_rcsnp_require_keys(const json& _object,
    std::initializer_list<const char*> _required,
    std::initializer_list<const char*> _optional = {}) {
    if (!_object.is_object()) {
        throw invalid_runtime_control_snapshot{};
    }
    for (const auto* key : _required) {
        if (!_object.contains(key)) {
            throw invalid_runtime_control_snapshot{};
        }
    }
    for (auto iterator = _object.begin(); iterator != _object.end(); ++iterator) {
        bool allowed = false;
        for (const auto* key : _required) {
            allowed = allowed || iterator.key() == key;
        }
        for (const auto* key : _optional) {
            allowed = allowed || iterator.key() == key;
        }
        if (!allowed) {
            throw invalid_runtime_control_snapshot{};
        }
    }
}

std::uint64_t vqec_vision_ai_lifec_rcsnp_uint(const json& _value) {
    if (!_value.is_number_unsigned()) {
        throw invalid_runtime_control_snapshot{};
    }
    return _value.get<std::uint64_t>();
}

bool vqec_vision_ai_lifec_rcsnp_bool(const json& _value) {
    if (!_value.is_boolean()) {
        throw invalid_runtime_control_snapshot{};
    }
    return _value.get<bool>();
}

std::string vqec_vision_ai_lifec_rcsnp_text(
    const json& _value, std::size_t _max_bytes) {
    if (!_value.is_string()) {
        throw invalid_runtime_control_snapshot{};
    }
    auto value = _value.get<std::string>();
    if (value.empty() || value.size() > _max_bytes ||
        value.find('\0') != std::string::npos) {
        throw invalid_runtime_control_snapshot{};
    }
    return value;
}

std::vector<std::string> vqec_vision_ai_lifec_rcsnp_scopes(const json& _value) {
    if (!_value.is_array() || _value.size() > app_lifecycle_limits::g_max_scopes) {
        throw invalid_runtime_control_snapshot{};
    }
    std::vector<std::string> scopes;
    scopes.reserve(_value.size());
    for (const auto& value : _value) {
        auto scope = vqec_vision_ai_lifec_rcsnp_text(
            value, app_lifecycle_limits::g_max_identifier_bytes);
        if (std::find(scopes.begin(), scopes.end(), scope) != scopes.end()) {
            throw invalid_runtime_control_snapshot{};
        }
        scopes.push_back(std::move(scope));
    }
    return scopes;
}

app_runtime_association vqec_vision_ai_lifec_rcsnp_association(
    const json& _value) {
    vqec_vision_ai_lifec_rcsnp_require_keys(_value,
        {"app_id", "source_id", "installed", "entitled", "desired", "supported",
            "compatible", "admitted", "configuration_revision",
            "configuration_sha256", "configuration_schema_id",
            "configuration_payload", "output_scopes"},
        {"reason_code", "entitlement_expires_utc_ns"});
    if (!_value.at("configuration_payload").is_object()) {
        throw invalid_runtime_control_snapshot{};
    }
    app_runtime_association association;
    association.app_id_ = vqec_vision_ai_lifec_rcsnp_text(
        _value.at("app_id"), app_lifecycle_limits::g_max_identifier_bytes);
    association.source_id_ = vqec_vision_ai_lifec_rcsnp_text(
        _value.at("source_id"), app_lifecycle_limits::g_max_identifier_bytes);
    association.installed_ = vqec_vision_ai_lifec_rcsnp_bool(_value.at("installed"));
    association.entitled_ = vqec_vision_ai_lifec_rcsnp_bool(_value.at("entitled"));
    association.desired_ = vqec_vision_ai_lifec_rcsnp_bool(_value.at("desired"));
    association.supported_ = vqec_vision_ai_lifec_rcsnp_bool(_value.at("supported"));
    association.compatible_ = vqec_vision_ai_lifec_rcsnp_bool(_value.at("compatible"));
    association.admitted_ = vqec_vision_ai_lifec_rcsnp_bool(_value.at("admitted"));
    association.configuration_revision_ =
        vqec_vision_ai_lifec_rcsnp_uint(_value.at("configuration_revision"));
    association.configuration_sha256_ = vqec_vision_ai_lifec_rcsnp_text(
        _value.at("configuration_sha256"), 64);
    association.configuration_schema_id_ = vqec_vision_ai_lifec_rcsnp_text(
        _value.at("configuration_schema_id"),
        app_lifecycle_limits::g_max_identifier_bytes);
    const auto configuration = _value.at("configuration_payload").dump();
    if (configuration.empty() ||
        configuration.size() > feature_catalog_limits::g_max_configuration_bytes) {
        throw invalid_runtime_control_snapshot{};
    }
    association.configuration_payload_.assign(configuration.begin(), configuration.end());
    association.output_scopes_ =
        vqec_vision_ai_lifec_rcsnp_scopes(_value.at("output_scopes"));
    if (_value.contains("reason_code")) {
        association.reason_code_ = vqec_vision_ai_lifec_rcsnp_text(
            _value.at("reason_code"), app_lifecycle_limits::g_max_version_bytes);
    }
    if (_value.contains("entitlement_expires_utc_ns")) {
        association.entitlement_expires_utc_ns_ =
            vqec_vision_ai_lifec_rcsnp_uint(
                _value.at("entitlement_expires_utc_ns"));
    }
    return association;
}

}  // namespace

status vqec_vision_ai_lifec_rcsnp_load(
    std::istream& _stream, runtime_control_snapshot& _snapshot) {
    std::string document;
    try {
        char byte = 0;
        while (_stream.get(byte)) {
            if (document.size() == app_lifecycle_limits::g_max_document_bytes) {
                return {status_code::resource_exhausted,
                    "runtime control snapshot exceeds document limit"};
            }
            document.push_back(byte);
        }
        if (_stream.bad() || (!_stream.eof() && _stream.fail())) {
            return {status_code::io_error, "runtime control snapshot read failed"};
        }
        std::vector<std::set<std::string>> object_keys;
        const auto callback = [&](int _depth, json::parse_event_t _event,
                                  json& _value) {
            if (_depth > static_cast<int>(app_lifecycle_limits::g_max_json_depth)) {
                throw invalid_runtime_control_snapshot{};
            }
            if (_event == json::parse_event_t::object_start) {
                object_keys.emplace_back();
            } else if (_event == json::parse_event_t::key) {
                if (object_keys.empty() ||
                    !object_keys.back().insert(_value.get<std::string>()).second) {
                    throw invalid_runtime_control_snapshot{};
                }
            } else if (_event == json::parse_event_t::object_end) {
                object_keys.pop_back();
            }
            return true;
        };
        const auto root = json::parse(document, callback);
        vqec_vision_ai_lifec_rcsnp_require_keys(root,
            {"schema_version", "snapshot_revision", "inventory_revision",
                "entitlement_revision", "desired_revision", "associations"});
        const auto& values = root.at("associations");
        if (!values.is_array() ||
            values.size() > app_lifecycle_limits::g_max_associations) {
            throw invalid_runtime_control_snapshot{};
        }
        runtime_control_snapshot candidate;
        const auto schema = vqec_vision_ai_lifec_rcsnp_uint(
            root.at("schema_version"));
        if (schema > std::numeric_limits<std::uint32_t>::max()) {
            throw invalid_runtime_control_snapshot{};
        }
        candidate.schema_version_ = static_cast<std::uint32_t>(schema);
        candidate.snapshot_revision_ =
            vqec_vision_ai_lifec_rcsnp_uint(root.at("snapshot_revision"));
        candidate.inventory_revision_ =
            vqec_vision_ai_lifec_rcsnp_uint(root.at("inventory_revision"));
        candidate.entitlement_revision_ =
            vqec_vision_ai_lifec_rcsnp_uint(root.at("entitlement_revision"));
        candidate.desired_revision_ =
            vqec_vision_ai_lifec_rcsnp_uint(root.at("desired_revision"));
        candidate.associations_.reserve(values.size());
        for (const auto& value : values) {
            candidate.associations_.push_back(
                vqec_vision_ai_lifec_rcsnp_association(value));
        }
        const auto valid =
            vqec_vision_ai_core_applc_validate_runtime_snapshot(candidate);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        _snapshot = std::move(candidate);
        return {};
    } catch (const invalid_runtime_control_snapshot&) {
        return {status_code::invalid_argument,
            "invalid runtime control snapshot structure or value"};
    } catch (const json::exception&) {
        return {status_code::invalid_argument,
            "malformed runtime control snapshot JSON"};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "runtime control snapshot allocation failed"};
    }
}

status vqec_vision_ai_lifec_rcsnp_write(
    const runtime_control_snapshot& _snapshot, std::ostream& _stream) {
    const auto valid = vqec_vision_ai_core_applc_validate_runtime_snapshot(_snapshot);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    try {
        json root;
        root["schema_version"] = _snapshot.schema_version_;
        root["snapshot_revision"] = _snapshot.snapshot_revision_;
        root["inventory_revision"] = _snapshot.inventory_revision_;
        root["entitlement_revision"] = _snapshot.entitlement_revision_;
        root["desired_revision"] = _snapshot.desired_revision_;
        root["associations"] = json::array();
        for (const auto& association : _snapshot.associations_) {
            json value;
            value["app_id"] = association.app_id_;
            value["source_id"] = association.source_id_;
            value["installed"] = association.installed_;
            value["entitled"] = association.entitled_;
            value["desired"] = association.desired_;
            value["supported"] = association.supported_;
            value["compatible"] = association.compatible_;
            value["admitted"] = association.admitted_;
            value["configuration_revision"] = association.configuration_revision_;
            value["configuration_sha256"] = association.configuration_sha256_;
            value["configuration_schema_id"] = association.configuration_schema_id_;
            value["configuration_payload"] = json::parse(
                association.configuration_payload_.begin(),
                association.configuration_payload_.end());
            value["output_scopes"] = association.output_scopes_;
            if (!association.reason_code_.empty()) {
                value["reason_code"] = association.reason_code_;
            }
            if (association.entitlement_expires_utc_ns_ != 0) {
                value["entitlement_expires_utc_ns"] =
                    association.entitlement_expires_utc_ns_;
            }
            root["associations"].push_back(std::move(value));
        }
        const auto document = root.dump();
        if (document.size() > app_lifecycle_limits::g_max_document_bytes) {
            return {status_code::resource_exhausted,
                "serialized runtime control snapshot exceeds document limit"};
        }
        _stream << document;
        return _stream.good() ? status{} :
            status{status_code::io_error, "runtime control snapshot write failed"};
    } catch (const json::exception&) {
        return {status_code::invalid_argument,
            "runtime control configuration payload is not JSON"};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "runtime control snapshot serialization allocation failed"};
    }
}

}  // namespace vqec::vision::ai
