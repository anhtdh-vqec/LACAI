#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "vqec_vision_activation_snapshot.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;

model_catalog_entry vqec_vision_ai_unit_astst_make_model() {
    model_catalog_entry model;
    model.model_id_ = "detector";
    model.model_version_ = "1.0";
    model.target_id_ = "qcs6490";
    model.artifact_ref_ = "detector_artifact";
    model.artifact_sha256_ = std::string(64, 'a');
    model.output_manifest_ref_ = "detector_outputs";
    model.decoder_contract_ = "detector.decoder.v1";
    model.preprocess_contract_ = "nv12_rgb_v1";
    model.graph_name_ = "detector_graph";
    model.tensor_width_ = 640;
    model.tensor_height_ = 640;
    model.placement_ = image_placement::centre;
    model.inference_fps_numerator_ = 10;
    model.inference_fps_denominator_ = 1;
    model.source_constraints_ = {640, 480, 4096, 2160, 10, 1};
    model.resources_ = {32 * g_mib, 8 * g_mib, 2, 16, true};
    return model;
}

void vqec_vision_ai_unit_astst_check_snapshot() {
    model_catalog catalog;
    catalog.schema_version_ = 1;
    catalog.revision_ = 8;
    catalog.catalog_id_ = "catalog_v1";
    catalog.models_.push_back(vqec_vision_ai_unit_astst_make_model());
    deployment_config deployment;
    deployment.schema_version_ = 1;
    deployment.revision_ = 9;
    deployment.model_catalog_ref_ = catalog.catalog_id_;
    deployment.max_total_resident_bytes_ = 512 * g_mib;
    deployment.max_model_resident_bytes_ = 64 * g_mib;
    for (unsigned index = 0; index < 16; ++index) {
        source_deployment_config source;
        source.source_id_ = "source_" + std::to_string(index);
        source.raw_source_ref_ = "fw_raw_" + std::to_string(index);
        source.camera_id_ = index;
        source.profile_ = {1920, 1080, 25, 1};
        source.memory_.max_frame_allocation_bytes_ = 4 * g_mib;
        source.memory_.max_inflight_frames_ = 1;
        source.memory_.max_tensor_bytes_ = 8 * g_mib;
        source.model_ids_.push_back("detector");
        deployment.sources_.push_back(source);
    }
    activation_snapshot snapshot;
    const auto result = vqec_vision_ai_admis_actsp_build_snapshot(
        deployment, catalog, snapshot);
    if (result.code_ != status_code::ok || snapshot.source_count_ != 16 ||
        snapshot.active_model_count_ != 1 ||
        snapshot.models_[0].assignment_count_ != 16 ||
        snapshot.models_[0].context_instance_count_ != 1 ||
        snapshot.sources_[15].catalog_model_indices_[0] != 0) {
        throw std::runtime_error("valid fixed-capacity activation snapshot rejected");
    }
    auto invalid = deployment;
    invalid.sources_[0].model_ids_[0] = "unknown";
    const auto preserved_revision = snapshot.deployment_revision_;
    const auto failed = vqec_vision_ai_admis_actsp_build_snapshot(
        invalid, catalog, snapshot);
    if (failed.code_ == status_code::ok ||
        snapshot.deployment_revision_ != preserved_revision) {
        throw std::runtime_error("failed snapshot build changed output");
    }
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    try {
        vqec::vision::ai::vqec_vision_ai_unit_astst_check_snapshot();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
