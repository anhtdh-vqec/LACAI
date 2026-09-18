#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "vqec_vision_deployment_config.hpp"

namespace vqec::vision::ai {
namespace {

constexpr char g_valid_document[] = R"({
  "schema_version": 1,
  "revision": 42,
  "model_catalog_ref": "models_qcs6490_v1",
  "max_total_resident_bytes": 536870912,
  "max_model_resident_bytes": 67108864,
  "sources": [{
    "source_id": "front_gate",
    "raw_source_ref": "fw_raw_front_gate",
    "camera_id": 0,
    "channel_id": 0,
    "preview_output_ref": "detect0",
    "width": 3840,
    "height": 2160,
    "fps_numerator": 25,
    "fps_denominator": 1,
    "max_frame_allocation_bytes": 16777216,
    "max_inflight_frames": 2,
    "preview_surface_count": 2,
    "max_tensor_bytes": 16777216,
    "max_temporal_bytes": 33554432,
    "model_ids": ["person_detector"]
  }]
})";

void vqec_vision_ai_unit_dltst_require_parse(
    const std::string& _text, status_code _expected) {
    std::istringstream stream(_text);
    deployment_config config;
    config.revision_ = 999;
    std::uint64_t declared_bytes = 777;
    const auto result =
        vqec_vision_ai_life_dpcfg_load(stream, config, declared_bytes);
    if (result.code_ != _expected) {
        throw std::runtime_error(result.message_);
    }
    if (_expected == status_code::ok) {
        if (config.revision_ != 42 || config.sources_.size() != 1 ||
            declared_bytes <= config.max_model_resident_bytes_) {
            throw std::runtime_error("valid deployment was not loaded");
        }
    } else if (config.revision_ != 999 || declared_bytes != 777) {
        throw std::runtime_error("loader changed outputs on failure");
    }
}

std::string vqec_vision_ai_unit_dltst_replace(
    const std::string& _from, const std::string& _to) {
    std::string document = g_valid_document;
    const auto position = document.find(_from);
    if (position == std::string::npos) {
        throw std::runtime_error("loader test fixture token is missing");
    }
    document.replace(position, _from.size(), _to);
    return document;
}

void vqec_vision_ai_unit_dltst_check_loader() {
    vqec_vision_ai_unit_dltst_require_parse(g_valid_document, status_code::ok);
    vqec_vision_ai_unit_dltst_require_parse("", status_code::invalid_argument);
    vqec_vision_ai_unit_dltst_require_parse(
        vqec_vision_ai_unit_dltst_replace(
            "\"revision\": 42", "\"revision\": 42, \"revision\": 43"),
        status_code::invalid_argument);
    vqec_vision_ai_unit_dltst_require_parse(
        vqec_vision_ai_unit_dltst_replace(
            "\"schema_version\": 1", "\"schema_version\": 2"),
        status_code::unsupported);
    vqec_vision_ai_unit_dltst_require_parse(
        vqec_vision_ai_unit_dltst_replace(
            "\"raw_source_ref\": \"fw_raw_front_gate\"",
            "\"raw_source_ref\": \"invalid/raw/ref\""),
        status_code::invalid_argument);
    vqec_vision_ai_unit_dltst_require_parse(
        std::string(g_valid_document) + std::string(
            deployment_document_limits::g_max_document_bytes, ' '),
        status_code::resource_exhausted);
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    try {
        vqec::vision::ai::vqec_vision_ai_unit_dltst_check_loader();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
