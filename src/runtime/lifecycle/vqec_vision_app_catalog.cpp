#include "vqec_vision_app_catalog.hpp"

#include <initializer_list>
#include <new>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace vqec::vision::ai {
namespace {

using nlohmann::json;

struct invalid_app_catalog {};

void vqec_vision_ai_lifec_apcat_require_keys(
    const json& _value, std::initializer_list<const char*> _required) {
    if (!_value.is_object() || _value.size() != _required.size()) {
        throw invalid_app_catalog{};
    }
    for (const auto* key : _required) {
        if (!_value.contains(key)) {
            throw invalid_app_catalog{};
        }
    }
}

std::string vqec_vision_ai_lifec_apcat_read_text(
    const json& _value, std::size_t _max_bytes) {
    if (!_value.is_string()) {
        throw invalid_app_catalog{};
    }
    auto result = _value.get<std::string>();
    if (result.empty() || result.size() > _max_bytes) {
        throw invalid_app_catalog{};
    }
    return result;
}

std::uint64_t vqec_vision_ai_lifec_apcat_read_uint(const json& _value) {
    if (!_value.is_number_unsigned()) {
        throw invalid_app_catalog{};
    }
    return _value.get<std::uint64_t>();
}

}  // namespace

status vqec_vision_ai_lifec_apcat_load(
    std::istream& _stream, usecase_app_catalog& _catalog) {
    std::string document;
    try {
        document.reserve(8192);
        char byte = 0;
        while (_stream.get(byte)) {
            if (document.size() == app_lifecycle_limits::g_max_document_bytes) {
                return {status_code::resource_exhausted,
                    "usecase app catalog exceeds document limit"};
            }
            document.push_back(byte);
        }
    } catch (const std::ios_base::failure&) {
        if (!_stream.eof() || _stream.bad()) {
            return {status_code::io_error, "usecase app catalog read failed"};
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "usecase app catalog allocation failed"};
    }
    if (_stream.bad() || (!_stream.eof() && _stream.fail())) {
        return {status_code::io_error, "cannot read usecase app catalog"};
    }
    try {
        const auto depth_guard = [](int _depth, json::parse_event_t _event, json&) -> bool {
            if (_depth > static_cast<int>(app_lifecycle_limits::g_max_json_depth)) {
                throw invalid_app_catalog{};
            }
            return _event != json::parse_event_t::key ||
                _depth <= static_cast<int>(app_lifecycle_limits::g_max_json_depth);
        };
        const json root = json::parse(document, depth_guard);
        vqec_vision_ai_lifec_apcat_require_keys(
            root, {"schema_version", "catalog_id", "revision", "applications"});
        usecase_app_catalog candidate;
        candidate.schema_version_ = static_cast<std::uint32_t>(
            vqec_vision_ai_lifec_apcat_read_uint(root.at("schema_version")));
        candidate.catalog_id_ = vqec_vision_ai_lifec_apcat_read_text(
            root.at("catalog_id"), app_lifecycle_limits::g_max_identifier_bytes);
        candidate.revision_ =
            vqec_vision_ai_lifec_apcat_read_uint(root.at("revision"));
        const auto& applications = root.at("applications");
        if (!applications.is_array() || applications.empty() ||
            applications.size() > app_lifecycle_limits::g_max_applications) {
            throw invalid_app_catalog{};
        }
        candidate.applications_.reserve(applications.size());
        for (const auto& value : applications) {
            vqec_vision_ai_lifec_apcat_require_keys(value,
                {"catalog_code", "app_id", "display_name", "app_version", "published"});
            if (!value.at("published").is_boolean()) {
                throw invalid_app_catalog{};
            }
            app_catalog_entry entry;
            entry.catalog_code_ = vqec_vision_ai_lifec_apcat_read_text(
                value.at("catalog_code"), app_lifecycle_limits::g_max_catalog_code_bytes);
            entry.app_id_ = vqec_vision_ai_lifec_apcat_read_text(
                value.at("app_id"), app_lifecycle_limits::g_max_identifier_bytes);
            entry.display_name_ = vqec_vision_ai_lifec_apcat_read_text(
                value.at("display_name"), app_lifecycle_limits::g_max_display_name_bytes);
            entry.app_version_ = vqec_vision_ai_lifec_apcat_read_text(
                value.at("app_version"), app_lifecycle_limits::g_max_version_bytes);
            entry.published_ = value.at("published").get<bool>();
            candidate.applications_.push_back(std::move(entry));
        }
        const auto valid = vqec_vision_ai_core_applc_validate_catalog(candidate);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        _catalog = std::move(candidate);
        return {};
    } catch (const invalid_app_catalog&) {
        return {status_code::invalid_argument, "invalid usecase app catalog document"};
    } catch (const json::exception&) {
        return {status_code::invalid_argument, "invalid usecase app catalog JSON"};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "usecase app catalog parse allocation failed"};
    }
}

}  // namespace vqec::vision::ai
