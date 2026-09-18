#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>

#include "vqec_vision_feature_catalog.hpp"

using namespace vqec::vision::ai;

namespace {

constexpr char g_valid_catalog[] = R"({
"schema_version":1,"revision":5,"catalog_id":"features_qcs6490_v1",
"model_catalog_ref":"models_qcs6490_v1","features":[{
"feature_id":"counting","feature_version":"1.0.0",
"processor_contract":"counting.processor.v1",
"configuration_schema":"counting.configuration.v1",
"input_mode":"single_model","model_dependencies":[{
"role_id":"person_tracks","model_id":"person_detector"}],
"attribute_dependencies":[],"resources":{
"max_temporal_bytes_per_source":1048576,"max_events_per_update":4,
"max_track_references_per_event":8,"max_fields_per_event":8}}]})";

std::string vqec_vision_ai_unit_fclt_replace(
    const std::string& _from, const std::string& _to) {
    std::string document = g_valid_catalog;
    const auto position = document.find(_from);
    if (position == std::string::npos) {
        throw std::runtime_error("feature catalog fixture token missing");
    }
    document.replace(position, _from.size(), _to);
    return document;
}

void vqec_vision_ai_unit_fclt_require_load(
    const std::string& _document, status_code _expected) {
    std::istringstream stream(_document);
    feature_catalog catalog;
    catalog.revision_ = 777;
    const auto result =
        vqec_vision_ai_ftmgr_ftcat_load_catalog(stream, catalog);
    if (result.code_ != _expected) {
        throw std::runtime_error(result.message_);
    }
    if (_expected == status_code::ok) {
        if (catalog.revision_ != 5 || catalog.features_.size() != 1 ||
            catalog.features_[0].feature_id_ != "counting" ||
            catalog.features_[0].model_dependencies_.size() != 1) {
            throw std::runtime_error("valid feature catalog not loaded");
        }
    } else if (catalog.revision_ != 777) {
        throw std::runtime_error("feature catalog changed output on failure");
    }
}

}  // namespace

int main() {
    try {
        vqec_vision_ai_unit_fclt_require_load(g_valid_catalog, status_code::ok);
        vqec_vision_ai_unit_fclt_require_load("", status_code::invalid_argument);
        vqec_vision_ai_unit_fclt_require_load(
            vqec_vision_ai_unit_fclt_replace(
                "\"revision\":5", "\"revision\":5,\"revision\":6"),
            status_code::invalid_argument);
        vqec_vision_ai_unit_fclt_require_load(
            vqec_vision_ai_unit_fclt_replace(
                "\"input_mode\":\"single_model\"",
                "\"input_mode\":\"unknown\""),
            status_code::invalid_argument);
        vqec_vision_ai_unit_fclt_require_load(
            vqec_vision_ai_unit_fclt_replace(
                "\"model_dependencies\":[{",
                "\"model_dependencies\":[{\"extra\":1,"),
            status_code::invalid_argument);
        vqec_vision_ai_unit_fclt_require_load(
            std::string(g_valid_catalog) + std::string(
                feature_catalog_document_limits::g_max_document_bytes, ' '),
            status_code::resource_exhausted);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
