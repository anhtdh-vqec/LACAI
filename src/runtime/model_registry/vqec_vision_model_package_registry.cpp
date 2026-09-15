#include "vqec_vision_model_package_registry.hpp"

#include <initializer_list>
#include <utility>

#include <nlohmann/json.hpp>

namespace vqec::vision::ai {
namespace {

using nlohmann::json;

struct invalid_model_package_registry {};

void vqec_vision_ai_mreg_mprld_require_keys(
    const json& _value, std::initializer_list<const char*> _required) {
    if (!_value.is_object() || _value.size() != _required.size()) {
        throw invalid_model_package_registry{};
    }
    for (const char* key : _required) {
        if (!_value.contains(key)) {
            throw invalid_model_package_registry{};
        }
    }
}

std::string vqec_vision_ai_mreg_mprld_read_text(const json& _value) {
    if (!_value.is_string()) {
        throw invalid_model_package_registry{};
    }
    return _value.get<std::string>();
}

}  // namespace

status vqec_vision_ai_mreg_mprld_load_registry(
    std::istream& _stream, model_package_registry& _registry) {
    std::string document;
    try {
        document.reserve(8192);
        char byte = 0;
        while (_stream.get(byte)) {
            if (document.size() ==
                model_package_registry_document_limits::g_max_document_bytes) {
                return {status_code::resource_exhausted,
                    "model package registry exceeds document limit"};
            }
            document.push_back(byte);
        }
    } catch (const std::ios_base::failure&) {
        if (!_stream.eof() || _stream.bad()) {
            return {status_code::io_error, "model package registry stream read failed"};
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "model package registry allocation failed"};
    }
    if (_stream.bad() || (!_stream.eof() && _stream.fail())) {
        return {status_code::io_error, "cannot read model package registry"};
    }

    try {
        const auto depth_guard = [](int _depth, json::parse_event_t _event,
                                     json&) -> bool {
            if (_depth > model_package_registry_document_limits::g_max_json_depth) {
                throw invalid_model_package_registry{};
            }
            return _event != json::parse_event_t::key ||
                _depth <= model_package_registry_document_limits::g_max_json_depth;
        };
        const json root = json::parse(document, depth_guard);
        vqec_vision_ai_mreg_mprld_require_keys(root, {"schema_version", "packages"});
        if (!root.at("schema_version").is_number_unsigned() ||
            !root.at("packages").is_array() || root.at("packages").empty() ||
            root.at("packages").size() > model_package_registry_limits::g_max_bindings) {
            throw invalid_model_package_registry{};
        }
        model_package_registry candidate;
        candidate.schema_version_ = root.at("schema_version").get<std::uint32_t>();
        for (const auto& value : root.at("packages")) {
            vqec_vision_ai_mreg_mprld_require_keys(value,
                {"model_id", "model_version", "target_id", "artifact_ref",
                    "package_dir", "model_library"});
            model_package_binding binding;
            binding.model_id_ =
                vqec_vision_ai_mreg_mprld_read_text(value.at("model_id"));
            binding.model_version_ =
                vqec_vision_ai_mreg_mprld_read_text(value.at("model_version"));
            binding.target_id_ =
                vqec_vision_ai_mreg_mprld_read_text(value.at("target_id"));
            binding.artifact_ref_ =
                vqec_vision_ai_mreg_mprld_read_text(value.at("artifact_ref"));
            binding.package_dir_ =
                vqec_vision_ai_mreg_mprld_read_text(value.at("package_dir"));
            binding.model_library_ =
                vqec_vision_ai_mreg_mprld_read_text(value.at("model_library"));
            candidate.bindings_.push_back(std::move(binding));
        }
        _registry = std::move(candidate);
        return {};
    } catch (const invalid_model_package_registry&) {
        return {status_code::invalid_argument, "invalid model package registry document"};
    } catch (const json::exception&) {
        return {status_code::invalid_argument, "invalid model package registry JSON"};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "model package registry parse allocation failed"};
    }
}

}  // namespace vqec::vision::ai
