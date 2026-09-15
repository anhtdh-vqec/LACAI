#include "vqec_vision_decoder_package.hpp"

#include <cmath>
#include <initializer_list>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {
namespace {

using decoder_json = nlohmann::json;
struct invalid_decoder_package {};

constexpr std::initializer_list<const char*> g_yolov8_allowed_keys = {
    "kind", "schema_version", "model_id", "decoder_contract", "class_count", "labels",
    "labels_ref", "confidence_threshold", "iou_threshold", "strides", "grids", "anchors",
    "box_tensor", "score_tensor", "box_layout", "box_format", "box_space", "box_input_size",
    "box_order", "score_semantics", "nms", "notes"};

constexpr std::initializer_list<const char*> g_yolov8_required_keys = {
    "decoder_contract", "class_count", "confidence_threshold", "iou_threshold",
    "box_tensor", "score_tensor"};

constexpr std::initializer_list<const char*> g_anchor_allowed_keys = {
    "kind", "decoder_contract", "class_id", "landmark_schema_id", "landmark_schema_version",
    "landmark_count", "anchor_offset_cells", "confidence_threshold", "iou_threshold",
    "max_candidates", "stages"};

constexpr std::initializer_list<const char*> g_anchor_required_keys = {
    "kind", "decoder_contract", "class_id", "landmark_schema_id", "landmark_schema_version",
    "landmark_count", "anchor_offset_cells", "confidence_threshold", "iou_threshold",
    "max_candidates", "stages"};

constexpr std::initializer_list<const char*> g_embedding_allowed_keys = {
    "kind", "decoder_contract", "output_tensor", "dimension", "min_norm",
    "landmark_schema_id", "landmark_schema_version", "destination_width",
    "destination_height", "reference_points", "color_matrix", "color_range",
    "channel_order"};

constexpr std::initializer_list<const char*> g_embedding_required_keys = {
    "kind", "decoder_contract", "output_tensor", "dimension", "landmark_schema_id",
    "landmark_schema_version", "destination_width", "destination_height", "reference_points"};

void vqec_vision_ai_mreg_dcpkg_require_keys(
    const decoder_json& _object, std::initializer_list<const char*> _allowed,
    std::initializer_list<const char*> _required) {
    if (!_object.is_object()) {
        throw invalid_decoder_package{};
    }
    for (auto iterator = _object.begin(); iterator != _object.end(); ++iterator) {
        bool allowed = false;
        for (const char* key : _allowed) {
            if (iterator.key() == key) {
                allowed = true;
                break;
            }
        }
        if (!allowed) {
            throw invalid_decoder_package{};
        }
    }
    for (const char* key : _required) {
        if (!_object.contains(key)) {
            throw invalid_decoder_package{};
        }
    }
}

std::string vqec_vision_ai_mreg_dcpkg_read_plain_text(const decoder_json& _value) {
    if (!_value.is_string()) {
        throw invalid_decoder_package{};
    }
    auto text = _value.get<std::string>();
    if (text.empty() || text.size() > decoder_package_limits::g_max_identifier_bytes ||
        text.find('\0') != std::string::npos) {
        throw invalid_decoder_package{};
    }
    return text;
}

std::string vqec_vision_ai_mreg_dcpkg_read_identifier(const decoder_json& _value) {
    const auto text = vqec_vision_ai_mreg_dcpkg_read_plain_text(_value);
    if (!vqec_vision_ai_cntr_ident_is_valid(text, decoder_package_limits::g_max_identifier_bytes)) {
        throw invalid_decoder_package{};
    }
    return text;
}

std::uint32_t vqec_vision_ai_mreg_dcpkg_read_u32(
    const decoder_json& _value, std::uint32_t _lower, std::uint32_t _upper) {
    if (!_value.is_number_unsigned()) {
        throw invalid_decoder_package{};
    }
    const auto number = _value.get<std::uint64_t>();
    if (number < _lower || number > _upper) {
        throw invalid_decoder_package{};
    }
    return static_cast<std::uint32_t>(number);
}

std::size_t vqec_vision_ai_mreg_dcpkg_read_size(
    const decoder_json& _value, std::size_t _lower, std::size_t _upper) {
    if (!_value.is_number_unsigned()) {
        throw invalid_decoder_package{};
    }
    const auto number = _value.get<std::uint64_t>();
    if (number < _lower || number > _upper) {
        throw invalid_decoder_package{};
    }
    return static_cast<std::size_t>(number);
}

double vqec_vision_ai_mreg_dcpkg_read_number(const decoder_json& _value) {
    if (!_value.is_number()) {
        throw invalid_decoder_package{};
    }
    const auto number = _value.get<double>();
    if (!std::isfinite(number)) {
        throw invalid_decoder_package{};
    }
    return number;
}

float vqec_vision_ai_mreg_dcpkg_read_closed_unit(const decoder_json& _value) {
    const auto number = vqec_vision_ai_mreg_dcpkg_read_number(_value);
    if (number < 0.0 || number > 1.0) {
        throw invalid_decoder_package{};
    }
    return static_cast<float>(number);
}

float vqec_vision_ai_mreg_dcpkg_read_open_zero_unit(const decoder_json& _value) {
    const auto number = vqec_vision_ai_mreg_dcpkg_read_number(_value);
    if (!(number > 0.0) || number > 1.0) {
        throw invalid_decoder_package{};
    }
    return static_cast<float>(number);
}

std::vector<std::string> vqec_vision_ai_mreg_dcpkg_read_labels(const decoder_json& _value) {
    if (!_value.is_array() || _value.size() > decoder_package_limits::g_max_labels) {
        throw invalid_decoder_package{};
    }
    std::vector<std::string> labels;
    labels.reserve(_value.size());
    for (const auto& entry : _value) {
        if (!entry.is_string()) {
            throw invalid_decoder_package{};
        }
        labels.push_back(entry.get<std::string>());
    }
    return labels;
}

// The informational YOLO metadata is not consumed by the runtime, but it is part of the
// package schema: its shape is validated so an unknown or malformed document cannot pass as
// a reviewed package.
void vqec_vision_ai_mreg_dcpkg_validate_yolov8_metadata(const decoder_json& _root) {
    if (_root.contains("schema_version")) {
        (void)vqec_vision_ai_mreg_dcpkg_read_u32(
            _root.at("schema_version"), 0U, std::numeric_limits<std::uint32_t>::max());
    }
    if (_root.contains("model_id")) {
        (void)vqec_vision_ai_mreg_dcpkg_read_identifier(_root.at("model_id"));
    }
    if (_root.contains("strides")) {
        const auto& strides = _root.at("strides");
        if (!strides.is_array() || strides.empty() ||
            strides.size() > decoder_package_limits::g_max_metadata_items) {
            throw invalid_decoder_package{};
        }
        for (const auto& stride : strides) {
            (void)vqec_vision_ai_mreg_dcpkg_read_u32(stride, 1U,
                std::numeric_limits<std::uint32_t>::max());
        }
    }
    if (_root.contains("grids")) {
        const auto& grids = _root.at("grids");
        if (!grids.is_array() || grids.empty() ||
            grids.size() > decoder_package_limits::g_max_metadata_items) {
            throw invalid_decoder_package{};
        }
        for (const auto& grid : grids) {
            if (!grid.is_object()) {
                throw invalid_decoder_package{};
            }
        }
    }
    if (_root.contains("anchors")) {
        (void)vqec_vision_ai_mreg_dcpkg_read_u32(
            _root.at("anchors"), 1U, std::numeric_limits<std::uint32_t>::max());
    }
    for (const char* key : {"box_layout", "box_format", "box_space", "box_order",
             "score_semantics", "nms"}) {
        if (_root.contains(key) && !_root.at(key).is_string()) {
            throw invalid_decoder_package{};
        }
    }
    if (_root.contains("box_input_size")) {
        const auto& size = _root.at("box_input_size");
        if (!size.is_array() || size.size() != 2U) {
            throw invalid_decoder_package{};
        }
        for (const auto& dimension : size) {
            (void)vqec_vision_ai_mreg_dcpkg_read_u32(
                dimension, 1U, std::numeric_limits<std::uint32_t>::max());
        }
    }
    if (_root.contains("notes") && !_root.at("notes").is_string()) {
        throw invalid_decoder_package{};
    }
}

decoder_package vqec_vision_ai_mreg_dcpkg_read_yolov8(const decoder_json& _root) {
    vqec_vision_ai_mreg_dcpkg_require_keys(
        _root, g_yolov8_allowed_keys, g_yolov8_required_keys);
    vqec_vision_ai_mreg_dcpkg_validate_yolov8_metadata(_root);
    const bool has_labels = _root.contains("labels");
    const bool has_labels_ref = _root.contains("labels_ref");
    if (has_labels && has_labels_ref) {
        throw invalid_decoder_package{};
    }
    decoder_package package;
    package.kind_ = decoder_package_kind::yolov8;
    package.decoder_contract_ = vqec_vision_ai_mreg_dcpkg_read_identifier(
        _root.at("decoder_contract"));
    package.box_tensor_ = vqec_vision_ai_mreg_dcpkg_read_identifier(_root.at("box_tensor"));
    package.score_tensor_ =
        vqec_vision_ai_mreg_dcpkg_read_identifier(_root.at("score_tensor"));
    if (package.box_tensor_ == package.score_tensor_) {
        throw invalid_decoder_package{};
    }
    package.class_count_ = vqec_vision_ai_mreg_dcpkg_read_size(
        _root.at("class_count"), 1U, decoder_package_limits::g_max_classes);
    package.confidence_threshold_ =
        vqec_vision_ai_mreg_dcpkg_read_closed_unit(_root.at("confidence_threshold"));
    package.iou_threshold_ =
        vqec_vision_ai_mreg_dcpkg_read_open_zero_unit(_root.at("iou_threshold"));
    if (has_labels) {
        package.labels_ = vqec_vision_ai_mreg_dcpkg_read_labels(_root.at("labels"));
        if (!package.labels_.empty() && package.labels_.size() != package.class_count_) {
            throw invalid_decoder_package{};
        }
    } else if (has_labels_ref) {
        package.labels_ref_ = vqec_vision_ai_mreg_dcpkg_read_identifier(
            _root.at("labels_ref"));
        // The reference is opened inside the package directory; reject the two dot names
        // that an identifier check alone would accept.
        if (package.labels_ref_ == "." || package.labels_ref_ == "..") {
            throw invalid_decoder_package{};
        }
    }
    return package;
}

decoder_package vqec_vision_ai_mreg_dcpkg_read_anchor_distance(const decoder_json& _root) {
    vqec_vision_ai_mreg_dcpkg_require_keys(
        _root, g_anchor_allowed_keys, g_anchor_required_keys);
    decoder_package package;
    package.kind_ = decoder_package_kind::anchor_distance;
    package.decoder_contract_ = vqec_vision_ai_mreg_dcpkg_read_identifier(
        _root.at("decoder_contract"));
    package.class_id_ = vqec_vision_ai_mreg_dcpkg_read_identifier(_root.at("class_id"));
    package.landmark_schema_id_ =
        vqec_vision_ai_mreg_dcpkg_read_identifier(_root.at("landmark_schema_id"));
    package.landmark_schema_version_ =
        vqec_vision_ai_mreg_dcpkg_read_identifier(_root.at("landmark_schema_version"));
    package.landmark_count_ = vqec_vision_ai_mreg_dcpkg_read_size(
        _root.at("landmark_count"), 1U, decoder_package_limits::g_max_landmarks);
    package.anchor_offset_cells_ =
        vqec_vision_ai_mreg_dcpkg_read_closed_unit(_root.at("anchor_offset_cells"));
    package.confidence_threshold_ =
        vqec_vision_ai_mreg_dcpkg_read_closed_unit(_root.at("confidence_threshold"));
    package.iou_threshold_ =
        vqec_vision_ai_mreg_dcpkg_read_open_zero_unit(_root.at("iou_threshold"));
    package.max_candidates_ = vqec_vision_ai_mreg_dcpkg_read_size(
        _root.at("max_candidates"), 1U, decoder_package_limits::g_max_candidates);
    const auto& stages = _root.at("stages");
    if (!stages.is_array() || stages.empty() ||
        stages.size() > decoder_package_limits::g_max_stages) {
        throw invalid_decoder_package{};
    }
    std::set<std::string> tensor_names;
    for (const auto& stage : stages) {
        vqec_vision_ai_mreg_dcpkg_require_keys(stage,
            {"score_tensor", "box_tensor", "landmark_tensor", "stride", "grid_width",
                "grid_height", "anchors_per_cell"}, {});
        decoder_package_stage parsed;
        parsed.score_tensor_ =
            vqec_vision_ai_mreg_dcpkg_read_identifier(stage.at("score_tensor"));
        parsed.box_tensor_ =
            vqec_vision_ai_mreg_dcpkg_read_identifier(stage.at("box_tensor"));
        parsed.landmark_tensor_ =
            vqec_vision_ai_mreg_dcpkg_read_identifier(stage.at("landmark_tensor"));
        parsed.stride_ = vqec_vision_ai_mreg_dcpkg_read_u32(
            stage.at("stride"), 1U, decoder_package_limits::g_max_stride);
        parsed.grid_width_ = vqec_vision_ai_mreg_dcpkg_read_u32(
            stage.at("grid_width"), 1U, std::numeric_limits<std::uint32_t>::max());
        parsed.grid_height_ = vqec_vision_ai_mreg_dcpkg_read_u32(
            stage.at("grid_height"), 1U, std::numeric_limits<std::uint32_t>::max());
        parsed.anchors_per_cell_ = vqec_vision_ai_mreg_dcpkg_read_u32(
            stage.at("anchors_per_cell"), 1U, std::numeric_limits<std::uint32_t>::max());
        for (const auto& name : {parsed.score_tensor_, parsed.box_tensor_,
                 parsed.landmark_tensor_}) {
            if (!tensor_names.insert(name).second) {
                throw invalid_decoder_package{};
            }
        }
        package.stages_.push_back(std::move(parsed));
    }
    return package;
}

decoder_package vqec_vision_ai_mreg_dcpkg_read_embedding(const decoder_json& _root) {
    vqec_vision_ai_mreg_dcpkg_require_keys(
        _root, g_embedding_allowed_keys, g_embedding_required_keys);
    decoder_package package;
    package.kind_ = decoder_package_kind::embedding;
    package.decoder_contract_ = vqec_vision_ai_mreg_dcpkg_read_identifier(
        _root.at("decoder_contract"));
    package.embedding_output_tensor_ = vqec_vision_ai_mreg_dcpkg_read_identifier(
        _root.at("output_tensor"));
    package.embedding_dimension_ = vqec_vision_ai_mreg_dcpkg_read_size(_root.at("dimension"),
        1U, decoder_package_limits::g_max_embedding_dimensions);
    if (_root.contains("min_norm")) {
        const auto number = vqec_vision_ai_mreg_dcpkg_read_number(_root.at("min_norm"));
        if (number < 0.0) {
            throw invalid_decoder_package{};
        }
        package.min_norm_ = static_cast<float>(number);
    }
    package.landmark_schema_id_ = vqec_vision_ai_mreg_dcpkg_read_identifier(
        _root.at("landmark_schema_id"));
    package.landmark_schema_version_ = vqec_vision_ai_mreg_dcpkg_read_identifier(
        _root.at("landmark_schema_version"));
    package.destination_width_ = vqec_vision_ai_mreg_dcpkg_read_u32(
        _root.at("destination_width"), decoder_package_limits::g_min_destination_dimension,
        decoder_package_limits::g_max_destination_dimension);
    package.destination_height_ = vqec_vision_ai_mreg_dcpkg_read_u32(
        _root.at("destination_height"), decoder_package_limits::g_min_destination_dimension,
        decoder_package_limits::g_max_destination_dimension);
    const auto& points = _root.at("reference_points");
    if (!points.is_array() || points.size() < 2U ||
        points.size() > decoder_package_limits::g_max_landmarks) {
        throw invalid_decoder_package{};
    }
    for (const auto& point : points) {
        vqec_vision_ai_mreg_dcpkg_require_keys(point, {"x", "y"}, {});
        landmark_point parsed;
        const auto x = vqec_vision_ai_mreg_dcpkg_read_number(point.at("x"));
        const auto y = vqec_vision_ai_mreg_dcpkg_read_number(point.at("y"));
        parsed.x_ = static_cast<float>(x);
        parsed.y_ = static_cast<float>(y);
        package.reference_points_.push_back(parsed);
    }
    package.landmark_count_ = package.reference_points_.size();
    if (_root.contains("color_matrix")) {
        const auto name = vqec_vision_ai_mreg_dcpkg_read_plain_text(_root.at("color_matrix"));
        if (name == "bt601") {
            package.color_matrix_ = color_matrix::bt601;
        } else if (name == "bt709") {
            package.color_matrix_ = color_matrix::bt709;
        } else {
            throw invalid_decoder_package{};
        }
    }
    if (_root.contains("color_range")) {
        const auto name = vqec_vision_ai_mreg_dcpkg_read_plain_text(_root.at("color_range"));
        if (name == "limited") {
            package.color_range_ = color_range::limited;
        } else if (name == "full") {
            package.color_range_ = color_range::full;
        } else {
            throw invalid_decoder_package{};
        }
    }
    if (_root.contains("channel_order")) {
        const auto name = vqec_vision_ai_mreg_dcpkg_read_plain_text(_root.at("channel_order"));
        if (name == "rgb") {
            package.channel_order_ = channel_order::rgb;
        } else if (name == "bgr") {
            package.channel_order_ = channel_order::bgr;
        } else {
            throw invalid_decoder_package{};
        }
    }
    return package;
}

}  // namespace

status vqec_vision_ai_mreg_dcpkg_load(
    std::istream& _stream, decoder_package& _package) {
    std::string document;
    try {
        document.reserve(4096);
        char byte = 0;
        while (_stream.get(byte)) {
            if (document.size() ==
                decoder_package_document_limits::g_max_document_bytes) {
                return {status_code::resource_exhausted,
                    "decoder package exceeds document limit"};
            }
            document.push_back(byte);
        }
    } catch (const std::ios_base::failure&) {
        if (!_stream.eof() || _stream.bad()) {
            return {status_code::io_error, "decoder package stream read failed"};
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "decoder package allocation failed"};
    }
    if (_stream.bad() || (!_stream.eof() && _stream.fail())) {
        return {status_code::io_error, "cannot read decoder package"};
    }

    try {
        std::vector<std::set<std::string>> object_keys;
        const auto callback = [&object_keys](int _depth, decoder_json::parse_event_t _event,
                                   decoder_json& _value) -> bool {
            if (_depth > decoder_package_document_limits::g_max_json_depth) {
                throw invalid_decoder_package{};
            }
            if (_event == decoder_json::parse_event_t::object_start) {
                object_keys.emplace_back();
            } else if (_event == decoder_json::parse_event_t::key) {
                if (object_keys.empty() ||
                    !object_keys.back().insert(_value.get<std::string>()).second) {
                    throw invalid_decoder_package{};
                }
            } else if (_event == decoder_json::parse_event_t::object_end) {
                if (!object_keys.empty()) {
                    object_keys.pop_back();
                }
            }
            return true;
        };
        const decoder_json root = decoder_json::parse(document, callback);
        if (!root.is_object() || root.empty()) {
            throw invalid_decoder_package{};
        }
        decoder_package_kind kind = decoder_package_kind::yolov8;
        if (root.contains("kind")) {
            const auto name = vqec_vision_ai_mreg_dcpkg_read_plain_text(root.at("kind"));
            if (name == "anchor_distance") {
                kind = decoder_package_kind::anchor_distance;
            } else if (name == "embedding") {
                kind = decoder_package_kind::embedding;
            } else if (name != "yolov8") {
                throw invalid_decoder_package{};
            }
        }
        decoder_package candidate;
        if (kind == decoder_package_kind::anchor_distance) {
            candidate = vqec_vision_ai_mreg_dcpkg_read_anchor_distance(root);
        } else if (kind == decoder_package_kind::embedding) {
            candidate = vqec_vision_ai_mreg_dcpkg_read_embedding(root);
        } else {
            candidate = vqec_vision_ai_mreg_dcpkg_read_yolov8(root);
        }
        _package = std::move(candidate);
        return {};
    } catch (const invalid_decoder_package&) {
        return {status_code::invalid_argument, "invalid decoder package document"};
    } catch (const decoder_json::exception&) {
        return {status_code::invalid_argument, "malformed decoder package JSON"};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "decoder package parse allocation failed"};
    }
}

}  // namespace vqec::vision::ai
