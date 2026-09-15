#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "vqec_vision_model_catalog.hpp"

namespace vqec::vision::ai {
namespace {

constexpr char g_valid_catalog[] = R"({
"schema_version":1,"revision":3,"catalog_id":"models_qcs6490_v1","models":[{
"model_id":"person_detector","model_version":"1.0.0",
"target_id":"qcs6490_qlinux_1_8","artifact_ref":"person_detector_qnn_v1",
"artifact_sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
"output_manifest_ref":"person_detector_outputs_v1",
"decoder_contract":"person_detector.decoder.v1",
"preprocess_contract":"nv12_rgb_letterbox_v1","graph_name":"person_detector_graph",
"input":{"width":640,"height":640,"dtype":"uint8","channel_order":"rgb",
"placement":"centre","mean":[0,0,0],"sigma":[1,1,1]},
"inference_cadence":{"numerator":10,"denominator":1},
"source_constraints":{"min_width":640,"min_height":480,"max_width":4096,
"max_height":2160,"min_fps_numerator":10,"min_fps_denominator":1},
"resources":{"resident_bytes":33554432,"max_tensor_bytes_per_source":8388608,
"output_queue_buffers":2,"max_concurrent_sources":16,
"can_share_context_across_sources":true}}]})";

constexpr char g_valid_catalog_v2[] = R"({
"schema_version":2,"revision":3,"catalog_id":"models_qcs6490_v2","models":[
{"model_id":"person_detector","model_version":"1.0.0","target_id":"qcs6490_qlinux_1_8",
"artifact_ref":"person_detector_qnn_v1",
"artifact_sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
"output_manifest_ref":"person_detector_outputs_v1",
"decoder_contract":"person_detector.decoder.v1",
"preprocess_contract":"nv12_rgb_letterbox_v1","graph_name":"person_detector_graph",
"input":{"width":640,"height":640,"dtype":"uint8","channel_order":"rgb",
"placement":"centre","mean":[0,0,0],"sigma":[1,1,1]},
"inference_cadence":{"numerator":10,"denominator":1},
"source_constraints":{"min_width":640,"min_height":480,"max_width":4096,
"max_height":2160,"min_fps_numerator":10,"min_fps_denominator":1},
"resources":{"resident_bytes":33554432,"max_tensor_bytes_per_source":8388608,
"output_queue_buffers":2,"max_concurrent_sources":16,
"can_share_context_across_sources":true},
"role":"primary"}]})";

constexpr char g_valid_catalog_v2_secondary[] = R"({
"schema_version":2,"revision":3,"catalog_id":"models_face_v2","models":[
{"model_id":"person_detector","model_version":"1.0.0","target_id":"qcs6490_qlinux_1_8",
"artifact_ref":"person_detector_qnn_v1",
"artifact_sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
"output_manifest_ref":"person_detector_outputs_v1",
"decoder_contract":"person_detector.decoder.v1",
"preprocess_contract":"nv12_rgb_letterbox_v1","graph_name":"person_detector_graph",
"input":{"width":640,"height":640,"dtype":"uint8","channel_order":"rgb",
"placement":"centre","mean":[0,0,0],"sigma":[1,1,1]},
"inference_cadence":{"numerator":10,"denominator":1},
"source_constraints":{"min_width":640,"min_height":480,"max_width":4096,
"max_height":2160,"min_fps_numerator":10,"min_fps_denominator":1},
"resources":{"resident_bytes":33554432,"max_tensor_bytes_per_source":8388608,
"output_queue_buffers":2,"max_concurrent_sources":16,
"can_share_context_across_sources":true},
"role":"primary"},
{"model_id":"face_embedding","model_version":"1.0.0","target_id":"qcs6490_qlinux_1_8",
"artifact_ref":"face_embedding_qnn_v1",
"artifact_sha256":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
"output_manifest_ref":"face_embedding_outputs_v1",
"decoder_contract":"face.embedding.v1",
"preprocess_contract":"nv12_rgb_crop_v1","graph_name":"face_embedding_graph",
"input":{"width":112,"height":112,"dtype":"uint8","channel_order":"rgb",
"placement":"centre","mean":[0,0,0],"sigma":[1,1,1]},
"inference_cadence":{"numerator":25,"denominator":1},
"source_constraints":{"min_width":640,"min_height":480,"max_width":4096,
"max_height":2160,"min_fps_numerator":10,"min_fps_denominator":1},
"resources":{"resident_bytes":33554432,"max_tensor_bytes_per_source":8388608,
"output_queue_buffers":2,"max_concurrent_sources":16,
"can_share_context_across_sources":true},
"role":"secondary","depends_on":[{"model_id":"person_detector","model_version":"1.0.0","target_id":"qcs6490_qlinux_1_8"}]}]})";

std::string vqec_vision_ai_unit_mltst_replace(
    const std::string& _from, const std::string& _to) {
    std::string document = g_valid_catalog;
    const auto position = document.find(_from);
    if (position == std::string::npos) {
        throw std::runtime_error("catalog loader fixture token missing");
    }
    document.replace(position, _from.size(), _to);
    return document;
}

std::string vqec_vision_ai_unit_mltst_replace_v2(
    const std::string& _from, const std::string& _to) {
    std::string document = g_valid_catalog_v2_secondary;
    const auto position = document.find(_from);
    if (position == std::string::npos) {
        throw std::runtime_error("catalog loader v2 fixture token missing");
    }
    document.replace(position, _from.size(), _to);
    return document;
}

void vqec_vision_ai_unit_mltst_require_v2_load(
    const std::string& _document, status_code _expected) {
    std::istringstream stream(_document);
    model_catalog catalog;
    std::uint64_t resident_bytes = 0;
    const auto result =
        vqec_vision_ai_mreg_mdcat_load_catalog(stream, catalog, resident_bytes);
    if (result.code_ != _expected) {
        std::cerr << "v2 loader expected " << static_cast<int>(_expected) << " got "
                  << static_cast<int>(result.code_) << ": " << result.message_ << '\n';
        throw std::runtime_error(result.message_);
    }
}

void vqec_vision_ai_unit_mltst_require_load(
    const std::string& _document, status_code _expected) {
    std::istringstream stream(_document);
    model_catalog catalog;
    catalog.revision_ = 777;
    std::uint64_t resident_bytes = 999;
    const auto result =
        vqec_vision_ai_mreg_mdcat_load_catalog(stream, catalog, resident_bytes);
    if (result.code_ != _expected) {
        std::cerr << "v1 loader expected " << static_cast<int>(_expected) << " got "
                  << static_cast<int>(result.code_) << ": " << result.message_ << '\n';
        throw std::runtime_error(result.message_);
    }
    if (_expected == status_code::ok) {
        if (catalog.revision_ != 3 || catalog.models_.size() != 1 ||
            resident_bytes != 33554432) {
            throw std::runtime_error("valid model catalog not loaded");
        }
    } else if (catalog.revision_ != 777 || resident_bytes != 999) {
        throw std::runtime_error("catalog loader changed output on failure");
    }
}

void vqec_vision_ai_unit_mltst_check_loader() {
    vqec_vision_ai_unit_mltst_require_load(g_valid_catalog, status_code::ok);
    vqec_vision_ai_unit_mltst_require_load("", status_code::invalid_argument);
    vqec_vision_ai_unit_mltst_require_load(
        vqec_vision_ai_unit_mltst_replace(
            "\"revision\":3", "\"revision\":3,\"revision\":4"),
        status_code::invalid_argument);
    // A version 1 document is migrated: normalized to version 2 with primary roles.
    {
        std::istringstream stream(g_valid_catalog);
        model_catalog catalog;
        std::uint64_t resident_bytes = 0;
        if (vqec_vision_ai_mreg_mdcat_load_catalog(stream, catalog, resident_bytes).code_ !=
                status_code::ok ||
            catalog.schema_version_ != model_catalog_limits::g_schema_version ||
            catalog.models_.size() != 1 ||
            catalog.models_[0].role_ != model_role::primary ||
            !catalog.models_[0].depends_on_.empty()) {
            throw std::runtime_error("version 1 catalog was not migrated to primary");
        }
    }
    // Version 2 requires an explicit role.
    vqec_vision_ai_unit_mltst_require_load(
        vqec_vision_ai_unit_mltst_replace(
            "\"schema_version\":1", "\"schema_version\":2"),
        status_code::invalid_argument);
    vqec_vision_ai_unit_mltst_require_v2_load(g_valid_catalog_v2, status_code::ok);
    vqec_vision_ai_unit_mltst_require_v2_load(
        g_valid_catalog_v2_secondary, status_code::ok);
    // A primary model must not declare dependencies.
    vqec_vision_ai_unit_mltst_require_v2_load(
        vqec_vision_ai_unit_mltst_replace_v2(
            "\"role\":\"primary\"},",
            "\"role\":\"primary\",\"depends_on\":[{\"model_id\":\"x\","
            "\"model_version\":\"1.0.0\",\"target_id\":\"qcs6490_qlinux_1_8\"}]},"),
        status_code::invalid_argument);
    // A secondary model requires dependencies.
    vqec_vision_ai_unit_mltst_require_v2_load(
        vqec_vision_ai_unit_mltst_replace_v2(
            ",\"depends_on\":[{\"model_id\":\"person_detector\",\"model_version\":\"1.0.0\","
            "\"target_id\":\"qcs6490_qlinux_1_8\"}]",
            ""),
        status_code::invalid_argument);
    // An unknown role and an unknown dependency key are rejected.
    vqec_vision_ai_unit_mltst_require_v2_load(
        vqec_vision_ai_unit_mltst_replace_v2(
            "\"role\":\"secondary\"", "\"role\":\"tertiary\""),
        status_code::invalid_argument);
    vqec_vision_ai_unit_mltst_require_v2_load(
        vqec_vision_ai_unit_mltst_replace_v2(
            "\"target_id\":\"qcs6490_qlinux_1_8\"}]",
            "\"target_id\":\"qcs6490_qlinux_1_8\",\"extra\":1}]"),
        status_code::invalid_argument);
    // Any reviewed dtype is accepted; an unknown spelling is rejected.
    vqec_vision_ai_unit_mltst_require_load(
        vqec_vision_ai_unit_mltst_replace(
            "\"dtype\":\"uint8\"", "\"dtype\":\"float16\""),
        status_code::ok);
    vqec_vision_ai_unit_mltst_require_load(
        vqec_vision_ai_unit_mltst_replace(
            "\"dtype\":\"uint8\"", "\"dtype\":\"bfloat16\""),
        status_code::invalid_argument);
    vqec_vision_ai_unit_mltst_require_load(
        std::string(g_valid_catalog) + std::string(
            model_catalog_document_limits::g_max_document_bytes, ' '),
        status_code::resource_exhausted);
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    try {
        vqec::vision::ai::vqec_vision_ai_unit_mltst_check_loader();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
