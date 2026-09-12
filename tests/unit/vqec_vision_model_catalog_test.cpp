#include <vqec/vision/ai/contracts/vqec_vision_model_catalog.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;

model_catalog_entry vqec_vision_ai_unit_mctst_make_model(const std::string& _id) {
    model_catalog_entry model;
    model.model_id_ = _id;
    model.model_version_ = "1.0.0";
    model.target_id_ = "qcs6490_qlinux_1_8";
    model.artifact_ref_ = _id + "_artifact";
    model.artifact_sha256_ = std::string(64, 'a');
    model.output_manifest_ref_ = _id + "_outputs";
    model.decoder_contract_ = _id + ".decoder.v1";
    model.preprocess_contract_ = "nv12_rgb_letterbox_v1";
    model.graph_name_ = _id + "_graph";
    model.tensor_width_ = 640;
    model.tensor_height_ = 640;
    model.placement_ = image_placement::centre;
    model.inference_fps_numerator_ = 10;
    model.inference_fps_denominator_ = 1;
    model.source_constraints_.min_width_ = 640;
    model.source_constraints_.min_height_ = 480;
    model.source_constraints_.max_width_ = 4096;
    model.source_constraints_.max_height_ = 2160;
    model.source_constraints_.min_fps_numerator_ = 10;
    model.source_constraints_.min_fps_denominator_ = 1;
    model.resources_.resident_bytes_ = 32 * g_mib;
    model.resources_.max_tensor_bytes_per_source_ = 8 * g_mib;
    model.resources_.output_queue_buffers_ = 2;
    model.resources_.max_concurrent_sources_ = 16;
    model.resources_.can_share_context_across_sources_ = true;
    return model;
}

model_catalog vqec_vision_ai_unit_mctst_make_catalog() {
    model_catalog catalog;
    catalog.schema_version_ = model_catalog_limits::g_schema_version;
    catalog.revision_ = 7;
    catalog.catalog_id_ = "models_qcs6490_v1";
    catalog.models_ = {
        vqec_vision_ai_unit_mctst_make_model("person_detector"),
        vqec_vision_ai_unit_mctst_make_model("person_attributes")};
    return catalog;
}

source_deployment_config vqec_vision_ai_unit_mctst_make_source(unsigned _index) {
    source_deployment_config source;
    source.source_id_ = "source_" + std::to_string(_index);
    source.raw_source_ref_ = "fw_raw_" + std::to_string(_index);
    source.camera_id_ = _index;
    source.channel_id_ = 0;
    source.profile_.width_ = 1920;
    source.profile_.height_ = 1080;
    source.profile_.fps_numerator_ = 25;
    source.profile_.fps_denominator_ = 1;
    source.memory_.max_frame_allocation_bytes_ = 4 * g_mib;
    source.memory_.max_inflight_frames_ = 2;
    source.memory_.max_tensor_bytes_ = 16 * g_mib;
    source.memory_.max_temporal_bytes_ = 4 * g_mib;
    source.model_ids_ = {"person_detector", "person_attributes"};
    return source;
}

deployment_config vqec_vision_ai_unit_mctst_make_deployment(unsigned _source_count = 1) {
    deployment_config deployment;
    deployment.schema_version_ = deployment_limits::g_schema_version;
    deployment.revision_ = 9;
    deployment.model_catalog_ref_ = "models_qcs6490_v1";
    deployment.max_total_resident_bytes_ = 1024 * g_mib;
    deployment.max_model_resident_bytes_ = 128 * g_mib;
    for (unsigned index = 0; index < _source_count; ++index) {
        deployment.sources_.push_back(vqec_vision_ai_unit_mctst_make_source(index));
    }
    return deployment;
}

void vqec_vision_ai_unit_mctst_check_catalog() {
    auto catalog = vqec_vision_ai_unit_mctst_make_catalog();
    std::uint64_t bytes = 0;
    auto result = vqec_vision_ai_core_mdcat_validate_catalog(catalog, bytes);
    if (result.code_ != status_code::ok || bytes != 64 * g_mib) {
        throw std::runtime_error("valid model catalog rejected");
    }
    catalog.models_[0].artifact_sha256_[0] = 'A';
    bytes = 777;
    result = vqec_vision_ai_core_mdcat_validate_catalog(catalog, bytes);
    if (result.code_ == status_code::ok || bytes != 777) {
        throw std::runtime_error("invalid digest changed catalog output");
    }
}

void vqec_vision_ai_unit_mctst_check_deployment_models() {
    auto deployment = vqec_vision_ai_unit_mctst_make_deployment(16);
    auto catalog = vqec_vision_ai_unit_mctst_make_catalog();
    std::uint64_t bytes = 0;
    auto result = vqec_vision_ai_core_mdcat_validate_deployment_models(
        deployment, catalog, bytes);
    if (result.code_ != status_code::ok || bytes != 64 * g_mib) {
        throw std::runtime_error("valid shared multi-source models rejected");
    }
    deployment.sources_[0].model_ids_[0] = "missing_model";
    result = vqec_vision_ai_core_mdcat_validate_deployment_models(
        deployment, catalog, bytes);
    if (result.code_ != status_code::invalid_argument) {
        throw std::runtime_error("unknown deployment model accepted");
    }
    deployment = vqec_vision_ai_unit_mctst_make_deployment();
    deployment.sources_[0].profile_.width_ = 8192;
    deployment.sources_[0].memory_.max_frame_allocation_bytes_ = 16 * g_mib;
    result = vqec_vision_ai_core_mdcat_validate_deployment_models(
        deployment, catalog, bytes);
    if (result.code_ != status_code::unsupported) {
        throw std::runtime_error("unsupported source envelope accepted");
    }
    deployment = vqec_vision_ai_unit_mctst_make_deployment();
    deployment.sources_[0].memory_.max_tensor_bytes_ = 8 * g_mib;
    result = vqec_vision_ai_core_mdcat_validate_deployment_models(
        deployment, catalog, bytes);
    if (result.code_ != status_code::resource_exhausted) {
        throw std::runtime_error("undersized tensor budget accepted");
    }
}

void vqec_vision_ai_unit_mctst_check_plan_composition() {
    const auto source = vqec_vision_ai_unit_mctst_make_source(0);
    const auto model = vqec_vision_ai_unit_mctst_make_model("person_detector");
    resolved_model_paths paths;
    paths.model_id_ = model.model_id_;
    paths.target_id_ = model.target_id_;
    paths.artifact_ref_ = model.artifact_ref_;
    paths.model_path_ = "/opt/vqec/models/person.bin";
    paths.backend_path_ = "/usr/lib/libQnnHtp.so";
    paths.system_path_ = "/usr/lib/libQnnSystem.so";
    inference_plan plan;
    auto result = vqec_vision_ai_core_mdcat_compose_inference_plan(
        source, model, paths, plan);
    if (result.code_ != status_code::ok || plan.source_width_ != 1920 ||
        plan.tensor_width_ != 640 || plan.input_queue_bytes_ != 4 * g_mib) {
        throw std::runtime_error("inference plan composition failed");
    }
    paths.artifact_ref_ = "other_artifact";
    const auto preserved_width = plan.source_width_;
    result = vqec_vision_ai_core_mdcat_compose_inference_plan(
        source, model, paths, plan);
    if (result.code_ == status_code::ok || plan.source_width_ != preserved_width) {
        throw std::runtime_error("failed plan composition changed output");
    }
}

void vqec_vision_ai_unit_mctst_check_output_binding() {
    const auto model = vqec_vision_ai_unit_mctst_make_model("person_detector");
    model_outputs outputs;
    outputs.model_id_ = model.model_id_;
    outputs.model_version_ = model.model_version_;
    outputs.artifact_sha256_ = model.artifact_sha256_;
    outputs.decoder_contract_ = model.decoder_contract_;
    outputs.max_output_bytes_ = 16;
    tensor_spec boxes;
    boxes.name_ = "boxes";
    boxes.dimensions_ = {1, 4};
    outputs.outputs_.push_back(boxes);
    std::uint64_t required_bytes = 777;
    auto result = vqec_vision_ai_core_mdcat_validate_model_outputs(
        model, model.output_manifest_ref_, outputs, required_bytes);
    if (result.code_ != status_code::ok || required_bytes != 16) {
        throw std::runtime_error("valid output manifest binding rejected");
    }
    outputs.model_version_ = "other";
    required_bytes = 777;
    result = vqec_vision_ai_core_mdcat_validate_model_outputs(
        model, model.output_manifest_ref_, outputs, required_bytes);
    if (result.code_ == status_code::ok || required_bytes != 777) {
        throw std::runtime_error("mismatched output manifest changed output");
    }
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    try {
        vqec::vision::ai::vqec_vision_ai_unit_mctst_check_catalog();
        vqec::vision::ai::vqec_vision_ai_unit_mctst_check_deployment_models();
        vqec::vision::ai::vqec_vision_ai_unit_mctst_check_plan_composition();
        vqec::vision::ai::vqec_vision_ai_unit_mctst_check_output_binding();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
