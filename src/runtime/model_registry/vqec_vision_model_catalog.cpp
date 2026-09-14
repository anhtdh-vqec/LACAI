#include "vqec_vision_model_catalog.hpp"

#include <array>
#include <initializer_list>
#include <limits>
#include <set>
#include <utility>

#include <nlohmann/json.hpp>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

using catalog_json = nlohmann::json;
struct invalid_catalog_document {};

void vqec_vision_ai_mreg_mdcat_require_keys(
    const catalog_json& _object, std::initializer_list<const char*> _keys) {
    if (!_object.is_object() || _object.size() != _keys.size()) {
        throw invalid_catalog_document{};
    }
    for (const auto* key : _keys) {
        if (!_object.contains(key)) {
            throw invalid_catalog_document{};
        }
    }
}

std::uint64_t vqec_vision_ai_mreg_mdcat_read_uint(const catalog_json& _value) {
    if (!_value.is_number_unsigned()) {
        throw invalid_catalog_document{};
    }
    return _value.get<std::uint64_t>();
}

std::uint32_t vqec_vision_ai_mreg_mdcat_read_u32(const catalog_json& _value) {
    const auto number = vqec_vision_ai_mreg_mdcat_read_uint(_value);
    if (number > std::numeric_limits<std::uint32_t>::max()) {
        throw invalid_catalog_document{};
    }
    return static_cast<std::uint32_t>(number);
}

unsigned vqec_vision_ai_mreg_mdcat_read_unsigned(const catalog_json& _value) {
    const auto number = vqec_vision_ai_mreg_mdcat_read_uint(_value);
    if (number > std::numeric_limits<unsigned>::max()) {
        throw invalid_catalog_document{};
    }
    return static_cast<unsigned>(number);
}

std::string vqec_vision_ai_mreg_mdcat_read_text(const catalog_json& _value) {
    if (!_value.is_string()) {
        throw invalid_catalog_document{};
    }
    auto text = _value.get<std::string>();
    if (text.empty() || text.size() > model_catalog_limits::g_max_identifier_bytes ||
        text.find('\0') != std::string::npos) {
        throw invalid_catalog_document{};
    }
    return text;
}

std::array<double, 3> vqec_vision_ai_mreg_mdcat_read_coefficients(
    const catalog_json& _value) {
    if (!_value.is_array() || _value.size() != 3) {
        throw invalid_catalog_document{};
    }
    std::array<double, 3> coefficients{};
    for (std::size_t index = 0; index < coefficients.size(); ++index) {
        if (!_value[index].is_number()) {
            throw invalid_catalog_document{};
        }
        coefficients[index] = _value[index].get<double>();
    }
    return coefficients;
}

tensor_element_type vqec_vision_ai_mreg_mdcat_read_tensor_type(
    const catalog_json& _value) {
    const auto name = vqec_vision_ai_mreg_mdcat_read_text(_value);
    const auto type = vqec_vision_ai_core_tnctr_element_type_from_name(name);
    if (type == tensor_element_type::unknown) {
        throw invalid_catalog_document{};
    }
    return type;
}

channel_order vqec_vision_ai_mreg_mdcat_read_channel_order(
    const catalog_json& _value) {
    const auto name = vqec_vision_ai_mreg_mdcat_read_text(_value);
    if (name == "rgb") {
        return channel_order::rgb;
    }
    if (name == "bgr") {
        return channel_order::bgr;
    }
    throw invalid_catalog_document{};
}

image_placement vqec_vision_ai_mreg_mdcat_read_placement(
    const catalog_json& _value) {
    const auto name = vqec_vision_ai_mreg_mdcat_read_text(_value);
    if (name == "top_left") {
        return image_placement::top_left;
    }
    if (name == "centre") {
        return image_placement::centre;
    }
    if (name == "stretch") {
        return image_placement::stretch;
    }
    throw invalid_catalog_document{};
}


preprocess_spec vqec_vision_ai_mreg_mdcat_read_preprocess(const catalog_json& _value) {
    if (!_value.is_object()) {
        throw invalid_catalog_document{};
    }
    preprocess_spec spec;
    const auto source = _value.value("source_format", std::string{"nv12"});
    spec.source_format_ = source == "nv12" ? source_pixel_format::nv12 :
        (source == "rgb888" ? source_pixel_format::rgb888 :
            (source == "bgr888" ? source_pixel_format::bgr888 :
                (source == "rgba8888" ? source_pixel_format::rgba8888 :
                    source_pixel_format::unknown)));
    const auto matrix = _value.value("color_matrix", std::string{});
    spec.matrix_ = matrix == "bt601" ? color_matrix::bt601 :
        (matrix == "bt709" ? color_matrix::bt709 :
            (matrix == "bt2020" ? color_matrix::bt2020 : color_matrix::unspecified));
    const auto range = _value.value("color_range", std::string{});
    spec.range_ = range == "limited" ? color_range::limited :
        (range == "full" ? color_range::full : color_range::unspecified);
    const auto resize = _value.value("resize", std::string{"letterbox"});
    spec.resize_ = resize == "stretch" ? resize_mode::stretch :
        (resize == "crop" ? resize_mode::crop : resize_mode::letterbox);
    const auto interpolation = _value.value("interpolation", std::string{"bilinear"});
    spec.interpolation_ = interpolation == "nearest" ? interpolation_mode::nearest :
        (interpolation == "bilinear" ? interpolation_mode::bilinear : interpolation_mode::area);
    spec.placement_ = image_placement::centre;
    if (_value.contains("pad_value")) {
        const auto pad = _value["pad_value"].get<std::vector<float>>();
        for (std::size_t i = 0; i < 3U && i < pad.size(); ++i) {
            spec.pad_value_[i] = pad[i];
        }
    }
    spec.channels_ = _value.value("channel_order", std::string{"rgb"}) == "bgr" ?
        channel_order::bgr : channel_order::rgb;
    if (_value.contains("normalization")) {
        const auto& normalization = _value["normalization"];
        const auto formula = normalization.value("formula", std::string{"offset_scale"});
        spec.normalization_ = formula == "mean_std" ? normalization_formula::mean_std :
            (formula == "none" ? normalization_formula::none :
                normalization_formula::offset_scale);
        spec.offset_ = {0.0F, 0.0F, 0.0F};
        spec.scale_ = {1.0F, 1.0F, 1.0F};
        if (normalization.contains("offset")) {
            const auto offset = normalization["offset"].get<std::vector<float>>();
            for (std::size_t i = 0; i < 3U && i < offset.size(); ++i) {
                spec.offset_[i] = offset[i];
            }
        }
        if (normalization.contains("scale")) {
            const auto scale = normalization["scale"].get<std::vector<float>>();
            for (std::size_t i = 0; i < 3U && i < scale.size(); ++i) {
                spec.scale_[i] = scale[i];
            }
        }
    }
    const auto coordinates = _value.value("coordinates", std::string{"tensor_pixels_xywh"});
    spec.coordinates_ = coordinates == "tensor_pixels_xyxy" ?
        coordinate_convention::tensor_pixels_xyxy :
        (coordinates == "normalized_xywh" ? coordinate_convention::normalized_xywh :
            (coordinates == "normalized_xyxy" ? coordinate_convention::normalized_xyxy :
                coordinate_convention::tensor_pixels_xywh));
    return spec;
}

model_catalog_entry vqec_vision_ai_mreg_mdcat_read_model(
    const catalog_json& _value) {
    if (!_value.is_object() || _value.size() < 13U || _value.size() > 14U) {
        throw invalid_catalog_document{};
    }
    for (const char* key : {"model_id", "model_version", "target_id", "artifact_ref",
             "artifact_sha256", "output_manifest_ref", "decoder_contract",
             "preprocess_contract", "graph_name", "input", "inference_cadence",
             "source_constraints", "resources"}) {
        if (!_value.contains(key)) {
            throw invalid_catalog_document{};
        }
    }
    model_catalog_entry model;
    model.model_id_ = vqec_vision_ai_mreg_mdcat_read_text(_value.at("model_id"));
    model.model_version_ =
        vqec_vision_ai_mreg_mdcat_read_text(_value.at("model_version"));
    model.target_id_ = vqec_vision_ai_mreg_mdcat_read_text(_value.at("target_id"));
    model.artifact_ref_ = vqec_vision_ai_mreg_mdcat_read_text(_value.at("artifact_ref"));
    model.artifact_sha256_ =
        vqec_vision_ai_mreg_mdcat_read_text(_value.at("artifact_sha256"));
    model.output_manifest_ref_ =
        vqec_vision_ai_mreg_mdcat_read_text(_value.at("output_manifest_ref"));
    model.decoder_contract_ =
        vqec_vision_ai_mreg_mdcat_read_text(_value.at("decoder_contract"));
    model.preprocess_contract_ =
        vqec_vision_ai_mreg_mdcat_read_text(_value.at("preprocess_contract"));
    model.graph_name_ = vqec_vision_ai_mreg_mdcat_read_text(_value.at("graph_name"));
    if (_value.contains("preprocess")) {
        model.preprocess_ = vqec_vision_ai_mreg_mdcat_read_preprocess(_value.at("preprocess"));
    }

    const auto& input = _value.at("input");
    vqec_vision_ai_mreg_mdcat_require_keys(
        input, {"width", "height", "dtype", "channel_order", "placement",
                "mean", "sigma"});
    model.tensor_width_ = vqec_vision_ai_mreg_mdcat_read_u32(input.at("width"));
    model.tensor_height_ = vqec_vision_ai_mreg_mdcat_read_u32(input.at("height"));
    model.input_type_ = vqec_vision_ai_mreg_mdcat_read_tensor_type(input.at("dtype"));
    model.channel_order_ =
        vqec_vision_ai_mreg_mdcat_read_channel_order(input.at("channel_order"));
    model.placement_ =
        vqec_vision_ai_mreg_mdcat_read_placement(input.at("placement"));
    model.mean_ = vqec_vision_ai_mreg_mdcat_read_coefficients(input.at("mean"));
    model.sigma_ = vqec_vision_ai_mreg_mdcat_read_coefficients(input.at("sigma"));

    const auto& cadence = _value.at("inference_cadence");
    vqec_vision_ai_mreg_mdcat_require_keys(cadence, {"numerator", "denominator"});
    model.inference_fps_numerator_ =
        vqec_vision_ai_mreg_mdcat_read_u32(cadence.at("numerator"));
    model.inference_fps_denominator_ =
        vqec_vision_ai_mreg_mdcat_read_u32(cadence.at("denominator"));

    const auto& constraints = _value.at("source_constraints");
    vqec_vision_ai_mreg_mdcat_require_keys(
        constraints, {"min_width", "min_height", "max_width", "max_height",
                      "min_fps_numerator", "min_fps_denominator"});
    model.source_constraints_.min_width_ =
        vqec_vision_ai_mreg_mdcat_read_u32(constraints.at("min_width"));
    model.source_constraints_.min_height_ =
        vqec_vision_ai_mreg_mdcat_read_u32(constraints.at("min_height"));
    model.source_constraints_.max_width_ =
        vqec_vision_ai_mreg_mdcat_read_u32(constraints.at("max_width"));
    model.source_constraints_.max_height_ =
        vqec_vision_ai_mreg_mdcat_read_u32(constraints.at("max_height"));
    model.source_constraints_.min_fps_numerator_ =
        vqec_vision_ai_mreg_mdcat_read_u32(constraints.at("min_fps_numerator"));
    model.source_constraints_.min_fps_denominator_ =
        vqec_vision_ai_mreg_mdcat_read_u32(constraints.at("min_fps_denominator"));

    const auto& resources = _value.at("resources");
    vqec_vision_ai_mreg_mdcat_require_keys(
        resources, {"resident_bytes", "max_tensor_bytes_per_source",
                    "output_queue_buffers", "max_concurrent_sources",
                    "can_share_context_across_sources"});
    model.resources_.resident_bytes_ =
        vqec_vision_ai_mreg_mdcat_read_uint(resources.at("resident_bytes"));
    model.resources_.max_tensor_bytes_per_source_ =
        vqec_vision_ai_mreg_mdcat_read_uint(
            resources.at("max_tensor_bytes_per_source"));
    model.resources_.output_queue_buffers_ =
        vqec_vision_ai_mreg_mdcat_read_u32(resources.at("output_queue_buffers"));
    model.resources_.max_concurrent_sources_ =
        vqec_vision_ai_mreg_mdcat_read_unsigned(resources.at("max_concurrent_sources"));
    const auto& share = resources.at("can_share_context_across_sources");
    if (!share.is_boolean()) {
        throw invalid_catalog_document{};
    }
    model.resources_.can_share_context_across_sources_ = share.get<bool>();
    return model;
}

}  // namespace

status vqec_vision_ai_mreg_mdcat_load_catalog(
    std::istream& _stream, model_catalog& _catalog,
    std::uint64_t& _declared_resident_bytes) {
    std::string document;
    try {
        document.reserve(8192);
        char byte = 0;
        while (_stream.get(byte)) {
            if (document.size() == model_catalog_document_limits::g_max_document_bytes) {
                return {status_code::resource_exhausted,
                        "model catalog exceeds document limit"};
            }
            document.push_back(byte);
        }
    } catch (const std::ios_base::failure&) {
        if (!_stream.eof() || _stream.bad()) {
            return {status_code::io_error, "model catalog stream read failed"};
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
                "model catalog document allocation failed"};
    }
    if (_stream.bad() || (!_stream.eof() && _stream.fail())) {
        return {status_code::io_error, "cannot read model catalog"};
    }
    try {
        std::vector<std::set<std::string>> object_keys;
        const auto callback = [&](int _depth, catalog_json::parse_event_t _event,
                                  catalog_json& _value) {
            if (_depth > model_catalog_document_limits::g_max_json_depth) {
                throw invalid_catalog_document{};
            }
            if (_event == catalog_json::parse_event_t::object_start) {
                object_keys.emplace_back();
            } else if (_event == catalog_json::parse_event_t::key) {
                if (object_keys.empty() ||
                    !object_keys.back().insert(_value.get<std::string>()).second) {
                    throw invalid_catalog_document{};
                }
            } else if (_event == catalog_json::parse_event_t::object_end) {
                object_keys.pop_back();
            }
            return true;
        };
        const auto root = catalog_json::parse(document, callback);
        vqec_vision_ai_mreg_mdcat_require_keys(
            root, {"schema_version", "revision", "catalog_id", "models"});
        model_catalog candidate;
        candidate.schema_version_ =
            vqec_vision_ai_mreg_mdcat_read_u32(root.at("schema_version"));
        candidate.revision_ = vqec_vision_ai_mreg_mdcat_read_uint(root.at("revision"));
        candidate.catalog_id_ = vqec_vision_ai_mreg_mdcat_read_text(root.at("catalog_id"));
        const auto& models = root.at("models");
        if (!models.is_array() || models.empty() ||
            models.size() > model_catalog_limits::g_max_models) {
            throw invalid_catalog_document{};
        }
        candidate.models_.reserve(models.size());
        for (const auto& model : models) {
            candidate.models_.push_back(vqec_vision_ai_mreg_mdcat_read_model(model));
        }
        std::uint64_t declared_bytes = 0;
        const auto valid =
            vqec_vision_ai_core_mdcat_validate_catalog(candidate, declared_bytes);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        _catalog = std::move(candidate);
        _declared_resident_bytes = declared_bytes;
        return {};
    } catch (const invalid_catalog_document&) {
        return {status_code::invalid_argument, "invalid model catalog structure or value"};
    } catch (const catalog_json::exception&) {
        return {status_code::invalid_argument, "malformed model catalog JSON"};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "model catalog parse allocation failed"};
    }
}

}  // namespace vqec::vision::ai
