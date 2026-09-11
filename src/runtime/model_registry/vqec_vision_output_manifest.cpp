#include "vqec_vision_output_manifest.hpp"

#include <cstddef>
#include <initializer_list>
#include <new>
#include <set>
#include <utility>

#include <nlohmann/json.hpp>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

using manifest_json = nlohmann::json;
struct invalid_manifest {};

constexpr std::size_t g_max_manifest_document_bytes = 64U * 1024U;
constexpr int g_max_manifest_json_depth = 16;

void vqec_vision_ai_mreg_otman_require_keys(
    const manifest_json& _object, std::initializer_list<const char*> _keys) {
    if (!_object.is_object() || _object.size() != _keys.size()) {
        throw invalid_manifest{};
    }
    for (const auto* key : _keys) {
        if (!_object.contains(key)) {
            throw invalid_manifest{};
        }
    }
}

std::uint64_t vqec_vision_ai_mreg_otman_read_uint(const manifest_json& _value) {
    if (!_value.is_number_unsigned()) {
        throw invalid_manifest{};
    }
    return _value.get<std::uint64_t>();
}

std::string vqec_vision_ai_mreg_otman_read_text(const manifest_json& _value, bool _is_identifier) {
    if (!_value.is_string()) {
        throw invalid_manifest{};
    }
    auto text = _value.get<std::string>();
    if (text.empty() || text.size() > 128 || text.find('\0') != std::string::npos) {
        throw invalid_manifest{};
    }
    if (_is_identifier) {
        for (const unsigned char byte : text) {
            if (!((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
                  (byte >= '0' && byte <= '9') || byte == '_' || byte == '-' ||
                  byte == '.' || byte == ':')) {
                throw invalid_manifest{};
            }
        }
    }
    return text;
}

}  // namespace

status vqec_vision_ai_mreg_otman_load_manifest(std::istream& _stream, model_outputs& _manifest) {
    std::string document;
    try {
        document.reserve(4096);
        char byte = 0;
        while (_stream.get(byte)) {
            if (document.size() == g_max_manifest_document_bytes) {
                return {status_code::resource_exhausted, "manifest exceeds 64 KiB"};
            }
            document.push_back(byte);
        }
    } catch (const std::ios_base::failure&) {
        if (!_stream.eof() || _stream.bad()) {
            return {status_code::io_error, "manifest stream read failed"};
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "manifest document allocation failed"};
    }
    if (_stream.bad() || (!_stream.eof() && _stream.fail())) {
        return {status_code::io_error, "cannot read model output manifest"};
    }
    try {
        std::vector<std::set<std::string>> object_keys;
        const auto callback = [&](int _depth, manifest_json::parse_event_t _event,
                                  manifest_json& _value) {
            if (_depth > g_max_manifest_json_depth) {
                throw invalid_manifest{};
            }
            if (_event == manifest_json::parse_event_t::object_start) {
                object_keys.emplace_back();
            } else if (_event == manifest_json::parse_event_t::key) {
                if (object_keys.empty() ||
                    !object_keys.back().insert(_value.get<std::string>()).second) {
                    throw invalid_manifest{};
                }
            } else if (_event == manifest_json::parse_event_t::object_end) {
                object_keys.pop_back();
            }
            return true;
        };
        const auto root = manifest_json::parse(document, callback);
        vqec_vision_ai_mreg_otman_require_keys(root,
            {"schema_version", "model_id", "model_version", "artifact_sha256",
             "decoder_contract", "max_output_bytes", "outputs"});
        if (vqec_vision_ai_mreg_otman_read_uint(root.at("schema_version")) != 1) {
            return {status_code::unsupported, "unsupported output manifest version"};
        }
        model_outputs candidate;
        candidate.model_id_ = vqec_vision_ai_mreg_otman_read_text(root.at("model_id"), true);
        candidate.model_version_ =
            vqec_vision_ai_mreg_otman_read_text(root.at("model_version"), true);
        candidate.decoder_contract_ =
            vqec_vision_ai_mreg_otman_read_text(root.at("decoder_contract"), true);
        candidate.artifact_sha256_ =
            vqec_vision_ai_mreg_otman_read_text(root.at("artifact_sha256"), false);
        if (candidate.artifact_sha256_.size() != 64 ||
            candidate.artifact_sha256_.find_first_not_of("0123456789abcdef") != std::string::npos) {
            throw invalid_manifest{};
        }
        candidate.max_output_bytes_ =
            vqec_vision_ai_mreg_otman_read_uint(root.at("max_output_bytes"));
        const auto& outputs = root.at("outputs");
        if (!outputs.is_array() || outputs.empty() ||
            outputs.size() > tensor_contract_limits::g_max_outputs) {
            throw invalid_manifest{};
        }
        for (const auto& output : outputs) {
            vqec_vision_ai_mreg_otman_require_keys(output, {"name", "dtype", "shape"});
            if (!output.at("dtype").is_string() || output.at("dtype") != "float32") {
                return {status_code::unsupported, "only FLOAT32 output metadata is supported"};
            }
            float_tensor_spec spec;
            spec.name_ = vqec_vision_ai_mreg_otman_read_text(output.at("name"), false);
            const auto& shape = output.at("shape");
            if (!shape.is_array() || shape.empty() ||
                shape.size() > tensor_contract_limits::g_max_rank) {
                throw invalid_manifest{};
            }
            for (const auto& axis : shape) {
                const auto dimension = vqec_vision_ai_mreg_otman_read_uint(axis);
                if (dimension == 0 || dimension > INT32_MAX) {
                    throw invalid_manifest{};
                }
                spec.dimensions_.push_back(static_cast<std::uint32_t>(dimension));
            }
            candidate.outputs_.push_back(std::move(spec));
        }
        std::uint64_t required_bytes = 0;
        const auto valid = vqec_vision_ai_core_tnctr_validate_outputs(
            candidate.outputs_, candidate.max_output_bytes_, required_bytes);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        _manifest = std::move(candidate);
        return {};
    } catch (const invalid_manifest&) {
        return {status_code::invalid_argument, "invalid manifest structure or value"};
    } catch (const manifest_json::exception&) {
        return {status_code::invalid_argument, "malformed model output manifest JSON"};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "manifest parse allocation failed"};
    }
}

}  // namespace vqec::vision::ai
