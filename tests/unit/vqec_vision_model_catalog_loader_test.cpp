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

void vqec_vision_ai_unit_mltst_require_load(
    const std::string& _document, status_code _expected) {
    std::istringstream stream(_document);
    model_catalog catalog;
    catalog.revision_ = 777;
    std::uint64_t resident_bytes = 999;
    const auto result =
        vqec_vision_ai_mreg_mdcat_load_catalog(stream, catalog, resident_bytes);
    if (result.code_ != _expected) {
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
    vqec_vision_ai_unit_mltst_require_load(
        vqec_vision_ai_unit_mltst_replace(
            "\"schema_version\":1", "\"schema_version\":2"),
        status_code::unsupported);
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
