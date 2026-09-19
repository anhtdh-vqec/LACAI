#include "vqec_vision_app_entitlement.hpp"

#include <initializer_list>
#include <new>
#include <set>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace vqec::vision::ai {
namespace {

using json = nlohmann::json;
struct invalid_app_entitlement {};

void vqec_vision_ai_lifec_apent_require_keys(const json& _object,
    std::initializer_list<const char*> _required) {
    if (!_object.is_object()) {
        throw invalid_app_entitlement{};
    }
    for (const auto* key : _required) {
        if (!_object.contains(key)) {
            throw invalid_app_entitlement{};
        }
    }
    for (auto iterator = _object.begin(); iterator != _object.end(); ++iterator) {
        bool allowed = false;
        for (const auto* key : _required) {
            allowed = allowed || iterator.key() == key;
        }
        if (!allowed) {
            throw invalid_app_entitlement{};
        }
    }
}

std::uint64_t vqec_vision_ai_lifec_apent_uint(const json& _value) {
    if (!_value.is_number_unsigned()) {
        throw invalid_app_entitlement{};
    }
    return _value.get<std::uint64_t>();
}

std::string vqec_vision_ai_lifec_apent_text(const json& _value) {
    if (!_value.is_string()) {
        throw invalid_app_entitlement{};
    }
    auto value = _value.get<std::string>();
    if (value.empty() || value.size() > app_lifecycle_limits::g_max_identifier_bytes ||
        value.find('\0') != std::string::npos) {
        throw invalid_app_entitlement{};
    }
    return value;
}

}  // namespace

status vqec_vision_ai_lifec_apent_load(
    std::istream& _stream, app_entitlement_grant& _grant) {
    std::string document;
    try {
        char byte = 0;
        while (_stream.get(byte)) {
            if (document.size() == app_lifecycle_limits::g_max_document_bytes) {
                return {status_code::resource_exhausted,
                    "app entitlement exceeds document limit"};
            }
            document.push_back(byte);
        }
        if (_stream.bad() || (!_stream.eof() && _stream.fail())) {
            return {status_code::io_error, "app entitlement read failed"};
        }
        std::vector<std::set<std::string>> object_keys;
        const auto callback = [&](int _depth, json::parse_event_t _event,
                                  json& _value) {
            if (_depth > static_cast<int>(app_lifecycle_limits::g_max_json_depth)) {
                throw invalid_app_entitlement{};
            }
            if (_event == json::parse_event_t::object_start) {
                object_keys.emplace_back();
            } else if (_event == json::parse_event_t::key) {
                if (object_keys.empty() ||
                    !object_keys.back().insert(_value.get<std::string>()).second) {
                    throw invalid_app_entitlement{};
                }
            } else if (_event == json::parse_event_t::object_end) {
                object_keys.pop_back();
            }
            return true;
        };
        const auto root = json::parse(document, callback);
        vqec_vision_ai_lifec_apent_require_keys(root,
            {"schema_version", "grant_id", "grant_revision",
                "expected_entitlement_revision", "issuer_id", "key_id", "customer_id",
                "device_id", "target_id", "app_id", "source_id", "not_before_utc_ns",
                "expires_utc_ns", "granted", "output_scopes"});
        const auto schema = vqec_vision_ai_lifec_apent_uint(root.at("schema_version"));
        if (schema > UINT32_MAX || !root.at("granted").is_boolean() ||
            !root.at("output_scopes").is_array() ||
            root.at("output_scopes").size() > app_lifecycle_limits::g_max_scopes) {
            throw invalid_app_entitlement{};
        }
        app_entitlement_grant candidate;
        candidate.schema_version_ = static_cast<std::uint32_t>(schema);
        candidate.grant_id_ = vqec_vision_ai_lifec_apent_text(root.at("grant_id"));
        candidate.grant_revision_ =
            vqec_vision_ai_lifec_apent_uint(root.at("grant_revision"));
        candidate.expected_entitlement_revision_ =
            vqec_vision_ai_lifec_apent_uint(root.at("expected_entitlement_revision"));
        candidate.issuer_id_ = vqec_vision_ai_lifec_apent_text(root.at("issuer_id"));
        candidate.key_id_ = vqec_vision_ai_lifec_apent_text(root.at("key_id"));
        candidate.customer_id_ = vqec_vision_ai_lifec_apent_text(root.at("customer_id"));
        candidate.device_id_ = vqec_vision_ai_lifec_apent_text(root.at("device_id"));
        candidate.target_id_ = vqec_vision_ai_lifec_apent_text(root.at("target_id"));
        candidate.app_id_ = vqec_vision_ai_lifec_apent_text(root.at("app_id"));
        candidate.source_id_ = vqec_vision_ai_lifec_apent_text(root.at("source_id"));
        candidate.not_before_utc_ns_ =
            vqec_vision_ai_lifec_apent_uint(root.at("not_before_utc_ns"));
        candidate.expires_utc_ns_ =
            vqec_vision_ai_lifec_apent_uint(root.at("expires_utc_ns"));
        candidate.granted_ = root.at("granted").get<bool>();
        candidate.output_scopes_.reserve(root.at("output_scopes").size());
        for (const auto& scope : root.at("output_scopes")) {
            candidate.output_scopes_.push_back(
                vqec_vision_ai_lifec_apent_text(scope));
        }
        const auto valid = vqec_vision_ai_core_applc_validate_entitlement(candidate);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        _grant = std::move(candidate);
        return {};
    } catch (const invalid_app_entitlement&) {
        return {status_code::invalid_argument,
            "invalid app entitlement structure or value"};
    } catch (const json::exception&) {
        return {status_code::invalid_argument, "malformed app entitlement JSON"};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "app entitlement allocation failed"};
    }
}

}  // namespace vqec::vision::ai
