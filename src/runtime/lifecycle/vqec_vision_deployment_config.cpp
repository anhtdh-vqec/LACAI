#include "vqec_vision_deployment_config.hpp"

#include <initializer_list>
#include <limits>
#include <new>
#include <set>
#include <utility>

#include <nlohmann/json.hpp>

namespace vqec::vision::ai {
namespace {

using deployment_json = nlohmann::json;
struct invalid_deployment {};

void vqec_vision_ai_life_dpcfg_require_keys(
    const deployment_json& _object, std::initializer_list<const char*> _keys) {
    if (!_object.is_object() || _object.size() != _keys.size()) {
        throw invalid_deployment{};
    }
    for (const auto* key : _keys) {
        if (!_object.contains(key)) {
            throw invalid_deployment{};
        }
    }
}

std::uint64_t vqec_vision_ai_life_dpcfg_read_uint(const deployment_json& _value) {
    if (!_value.is_number_unsigned()) {
        throw invalid_deployment{};
    }
    return _value.get<std::uint64_t>();
}

std::uint32_t vqec_vision_ai_life_dpcfg_read_u32(const deployment_json& _value) {
    const auto number = vqec_vision_ai_life_dpcfg_read_uint(_value);
    if (number > std::numeric_limits<std::uint32_t>::max()) {
        throw invalid_deployment{};
    }
    return static_cast<std::uint32_t>(number);
}

unsigned vqec_vision_ai_life_dpcfg_read_unsigned(const deployment_json& _value) {
    const auto number = vqec_vision_ai_life_dpcfg_read_uint(_value);
    if (number > std::numeric_limits<unsigned>::max()) {
        throw invalid_deployment{};
    }
    return static_cast<unsigned>(number);
}

std::string vqec_vision_ai_life_dpcfg_read_text(
    const deployment_json& _value, bool _allow_empty) {
    if (!_value.is_string()) {
        throw invalid_deployment{};
    }
    auto text = _value.get<std::string>();
    if ((!_allow_empty && text.empty()) ||
        text.size() > deployment_limits::g_max_identifier_bytes ||
        text.find('\0') != std::string::npos) {
        throw invalid_deployment{};
    }
    return text;
}

}  // namespace

status vqec_vision_ai_life_dpcfg_load(
    std::istream& _stream, deployment_config& _config,
    std::uint64_t& _declared_resident_bytes) {
    std::string document;
    try {
        document.reserve(4096);
        char byte = 0;
        while (_stream.get(byte)) {
            if (document.size() == deployment_document_limits::g_max_document_bytes) {
                return {status_code::resource_exhausted, "deployment config exceeds document limit"};
            }
            document.push_back(byte);
        }
    } catch (const std::ios_base::failure&) {
        if (!_stream.eof() || _stream.bad()) {
            return {status_code::io_error, "deployment config stream read failed"};
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "deployment config document allocation failed"};
    }
    if (_stream.bad() || (!_stream.eof() && _stream.fail())) {
        return {status_code::io_error, "cannot read deployment config"};
    }
    try {
        std::vector<std::set<std::string>> object_keys;
        const auto callback = [&](int _depth, deployment_json::parse_event_t _event,
                                  deployment_json& _value) {
            if (_depth > deployment_document_limits::g_max_json_depth) {
                throw invalid_deployment{};
            }
            if (_event == deployment_json::parse_event_t::object_start) {
                object_keys.emplace_back();
            } else if (_event == deployment_json::parse_event_t::key) {
                if (object_keys.empty() ||
                    !object_keys.back().insert(_value.get<std::string>()).second) {
                    throw invalid_deployment{};
                }
            } else if (_event == deployment_json::parse_event_t::object_end) {
                object_keys.pop_back();
            }
            return true;
        };
        const auto root = deployment_json::parse(document, callback);
        vqec_vision_ai_life_dpcfg_require_keys(root,
            {"schema_version", "revision", "model_catalog_ref",
             "max_total_resident_bytes", "max_model_resident_bytes", "sources"});
        deployment_config candidate;
        candidate.schema_version_ =
            vqec_vision_ai_life_dpcfg_read_u32(root.at("schema_version"));
        candidate.revision_ = vqec_vision_ai_life_dpcfg_read_uint(root.at("revision"));
        candidate.model_catalog_ref_ =
            vqec_vision_ai_life_dpcfg_read_text(root.at("model_catalog_ref"), false);
        candidate.max_total_resident_bytes_ =
            vqec_vision_ai_life_dpcfg_read_uint(root.at("max_total_resident_bytes"));
        candidate.max_model_resident_bytes_ =
            vqec_vision_ai_life_dpcfg_read_uint(root.at("max_model_resident_bytes"));
        const auto& sources = root.at("sources");
        if (!sources.is_array() || sources.empty() ||
            sources.size() > deployment_limits::g_max_sources) {
            throw invalid_deployment{};
        }
        candidate.sources_.reserve(sources.size());
        for (const auto& source_json : sources) {
            vqec_vision_ai_life_dpcfg_require_keys(source_json,
                {"source_id", "raw_source_ref", "camera_id", "channel_id",
                 "preview_output_ref", "width", "height", "fps_numerator",
                 "fps_denominator", "max_frame_allocation_bytes", "max_inflight_frames",
                 "preview_surface_count", "max_tensor_bytes", "max_temporal_bytes",
                 "model_ids"});
            source_deployment_config source;
            source.source_id_ =
                vqec_vision_ai_life_dpcfg_read_text(source_json.at("source_id"), false);
            source.raw_source_ref_ =
                vqec_vision_ai_life_dpcfg_read_text(
                    source_json.at("raw_source_ref"), false);
            source.camera_id_ = vqec_vision_ai_life_dpcfg_read_u32(source_json.at("camera_id"));
            source.channel_id_ =
                vqec_vision_ai_life_dpcfg_read_u32(source_json.at("channel_id"));
            source.preview_output_ref_ =
                vqec_vision_ai_life_dpcfg_read_text(source_json.at("preview_output_ref"), true);
            source.profile_.width_ = vqec_vision_ai_life_dpcfg_read_u32(source_json.at("width"));
            source.profile_.height_ =
                vqec_vision_ai_life_dpcfg_read_u32(source_json.at("height"));
            source.profile_.fps_numerator_ =
                vqec_vision_ai_life_dpcfg_read_u32(source_json.at("fps_numerator"));
            source.profile_.fps_denominator_ =
                vqec_vision_ai_life_dpcfg_read_u32(source_json.at("fps_denominator"));
            source.memory_.max_frame_allocation_bytes_ =
                vqec_vision_ai_life_dpcfg_read_uint(
                    source_json.at("max_frame_allocation_bytes"));
            source.memory_.max_inflight_frames_ =
                vqec_vision_ai_life_dpcfg_read_unsigned(source_json.at("max_inflight_frames"));
            source.memory_.preview_surface_count_ =
                vqec_vision_ai_life_dpcfg_read_unsigned(source_json.at("preview_surface_count"));
            source.memory_.max_tensor_bytes_ =
                vqec_vision_ai_life_dpcfg_read_uint(source_json.at("max_tensor_bytes"));
            source.memory_.max_temporal_bytes_ =
                vqec_vision_ai_life_dpcfg_read_uint(source_json.at("max_temporal_bytes"));
            const auto& models = source_json.at("model_ids");
            if (!models.is_array() || models.empty() ||
                models.size() > deployment_limits::g_max_models_per_source) {
                throw invalid_deployment{};
            }
            source.model_ids_.reserve(models.size());
            for (const auto& model : models) {
                source.model_ids_.push_back(
                    vqec_vision_ai_life_dpcfg_read_text(model, false));
            }
            candidate.sources_.push_back(std::move(source));
        }
        std::uint64_t declared_bytes = 0;
        const auto valid =
            vqec_vision_ai_core_dpval_validate_deployment(candidate, declared_bytes);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        _config = std::move(candidate);
        _declared_resident_bytes = declared_bytes;
        return {};
    } catch (const invalid_deployment&) {
        return {status_code::invalid_argument, "invalid deployment config structure or value"};
    } catch (const deployment_json::exception&) {
        return {status_code::invalid_argument, "malformed deployment config JSON"};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "deployment config parse allocation failed"};
    }
}

}  // namespace vqec::vision::ai
