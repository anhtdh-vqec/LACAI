#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "vqec_vision_activation_snapshot.hpp"
#include "vqec_vision_hardware_admission_profile.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;

hardware_admission_profile vqec_vision_ai_unit_astst_make_profile() {
    hardware_admission_profile profile;
    profile.profile_id_ = "unit_fixture";
    profile.target_id_ = "qcs6490";
    profile.measurement_reference_ = "unit-test-fixture";
    profile.revision_ = 1;
    profile.max_total_resident_bytes_ = 4096ULL * g_mib;
    profile.max_frame_pool_bytes_ = 1024ULL * g_mib;
    profile.max_tensor_pool_bytes_ = 1024ULL * g_mib;
    profile.max_encoder_pool_bytes_ = 512ULL * g_mib;
    profile.max_cascade_roi_bytes_ = 512ULL * g_mib;
    profile.max_ddr_bandwidth_mbps_ = 12000;
    profile.max_fw_concurrency_slots_ = 16;
    profile.max_worker_concurrency_ = 64;
    profile.min_thermal_headroom_pct_ = 10;
    return profile;
}

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

void vqec_vision_ai_unit_astst_check_snapshot(const std::string& _profile_path) {
    model_catalog catalog;
    catalog.schema_version_ = 1;
    catalog.revision_ = 8;
    catalog.catalog_id_ = "catalog_v1";
    catalog.models_.push_back(vqec_vision_ai_unit_astst_make_model());
    deployment_config deployment;
    deployment.schema_version_ = 1;
    deployment.revision_ = 9;
    deployment.model_catalog_ref_ = catalog.catalog_id_;
    deployment.max_total_resident_bytes_ = 1024 * g_mib;
    deployment.max_model_resident_bytes_ = 64 * g_mib;
    for (unsigned index = 0; index < 16; ++index) {
        source_deployment_config source;
        source.source_id_ = "source_" + std::to_string(index);
        source.raw_source_ref_ = "fw_raw_" + std::to_string(index);
        source.preview_output_ref_ = "preview_ring_" + std::to_string(index);
        source.camera_id_ = index;
        source.channel_id_ = 0;
        source.profile_ = {1920, 1080, 25, 1};
        source.memory_.max_frame_allocation_bytes_ = 4 * g_mib;
        source.memory_.max_inflight_frames_ = 1;
        source.memory_.preview_surface_count_ = 1;
        source.memory_.max_tensor_bytes_ = 8 * g_mib;
        source.cascade_ = {2, 4, 16 * g_mib};
        source.model_ids_.push_back("detector");
        deployment.sources_.push_back(source);
    }
    activation_snapshot snapshot;
    const auto fixture_profile = vqec_vision_ai_unit_astst_make_profile();
    const auto result = vqec_vision_ai_admis_actsp_build_snapshot(
        deployment, catalog, fixture_profile, snapshot);
    if (result.code_ != status_code::ok) {
        throw std::runtime_error("valid fixed-capacity activation snapshot rejected: " + result.message_);
    }
    if (snapshot.source_count_ != 16 ||
        snapshot.active_model_count_ != 1 ||
        snapshot.models_[0].assignment_count_ != 16 ||
        snapshot.models_[0].context_instance_count_ != 1 ||
        snapshot.sources_[15].catalog_model_indices_[0] != 0) {
        throw std::runtime_error("valid fixed-capacity activation snapshot fields mismatch");
    }

    // Verify resource breakdown is computed and non-zero
    if (snapshot.resources_.frame_pool_bytes_ == 0 ||
        snapshot.resources_.tensor_pool_bytes_ != 16ULL * 8ULL * g_mib ||
        snapshot.resources_.encoder_pool_bytes_ == 0 ||
        snapshot.resources_.cascade_roi_bytes_ == 0 ||
        snapshot.resources_.estimated_ddr_bandwidth_mbps_ == 0 ||
        snapshot.resources_.fw_concurrency_slots_ != 16 ||
        snapshot.resources_.worker_concurrency_ == 0 ||
        snapshot.resources_.thermal_headroom_pct_ == 0) {
        throw std::runtime_error("resource envelope breakdown not properly populated");
    }

    auto insufficient_tensors = fixture_profile;
    insufficient_tensors.max_tensor_pool_bytes_ = 32ULL * g_mib;
    activation_snapshot tensor_rejected;
    if (vqec_vision_ai_admis_actsp_build_snapshot(deployment, catalog,
            insufficient_tensors, tensor_rejected).code_ != status_code::resource_exhausted) {
        throw std::runtime_error("tensor admission used shared model bytes instead of source pools");
    }

    // Negative test 1: invalid model ID in deployment
    auto invalid = deployment;
    invalid.sources_[0].model_ids_[0] = "unknown";
    const auto preserved_revision = snapshot.deployment_revision_;
    const auto failed = vqec_vision_ai_admis_actsp_build_snapshot(
        invalid, catalog, fixture_profile, snapshot);
    if (failed.code_ == status_code::ok ||
        snapshot.deployment_revision_ != preserved_revision) {
        throw std::runtime_error("failed snapshot build changed output");
    }

    // Negative test 2: invalid hardware profile rejected fail-closed
    hardware_admission_profile invalid_hw{};
    invalid_hw.max_total_resident_bytes_ = 0;  // invalid
    activation_snapshot hw_snap;
    if (vqec_vision_ai_admis_actsp_build_snapshot(
            deployment, catalog, invalid_hw, hw_snap).code_ != status_code::unsupported) {
        throw std::runtime_error("invalid hardware profile did not fail-closed with unsupported");
    }

    // Negative test 3: FW stream concurrency exceeds limit
    hardware_admission_profile low_fw_hw = fixture_profile;
    low_fw_hw.max_fw_concurrency_slots_ = 4;  // deployment has 16
    if (vqec_vision_ai_admis_actsp_build_snapshot(
            deployment, catalog, low_fw_hw, hw_snap).code_ != status_code::unsupported) {
        throw std::runtime_error("FW stream concurrency limit not enforced");
    }

    // Negative test 4: Memory budget exceeded
    hardware_admission_profile low_mem_hw = fixture_profile;
    low_mem_hw.max_total_resident_bytes_ = 10 * g_mib;  // far too small
    if (vqec_vision_ai_admis_actsp_build_snapshot(
            deployment, catalog, low_mem_hw, hw_snap).code_ != status_code::resource_exhausted) {
        throw std::runtime_error("memory budget limit not enforced");
    }

    // Negative test 5: DDR bandwidth exceeded
    hardware_admission_profile low_ddr_hw = fixture_profile;
    low_ddr_hw.max_ddr_bandwidth_mbps_ = 10;  // 10 MB/s is far too low for 16 sources
    if (vqec_vision_ai_admis_actsp_build_snapshot(
            deployment, catalog, low_ddr_hw, hw_snap).code_ != status_code::resource_exhausted) {
        throw std::runtime_error("DDR bandwidth limit not enforced");
    }

    // Strict loader records identity/provenance and rejects unknown keys transactionally.
    std::istringstream profile_stream{
        R"({"schema_version":1,"profile_id":"qcs6490_lab_r1","target_id":"qcs6490","revision":7,"measurement_reference":"board-report-2026-09-17","max_total_resident_bytes":4294967296,"max_frame_pool_bytes":1073741824,"max_tensor_pool_bytes":1073741824,"max_encoder_pool_bytes":536870912,"max_cascade_roi_bytes":536870912,"max_ddr_bandwidth_mbps":12000,"max_fw_concurrency_slots":16,"max_worker_concurrency":64,"min_thermal_headroom_pct":10})"};
    hardware_admission_profile loaded;
    if (vqec_vision_ai_admis_hwprf_load(profile_stream, loaded).code_ != status_code::ok ||
        loaded.profile_id_ != "qcs6490_lab_r1" || loaded.revision_ != 7) {
        throw std::runtime_error("valid hardware profile was not loaded");
    }
    const auto preserved_profile_id = loaded.profile_id_;
    std::istringstream unknown_key{
        R"({"schema_version":1,"profile_id":"x","target_id":"qcs6490","revision":1,"measurement_reference":"fixture","max_total_resident_bytes":1,"max_frame_pool_bytes":1,"max_tensor_pool_bytes":1,"max_encoder_pool_bytes":1,"max_cascade_roi_bytes":1,"max_ddr_bandwidth_mbps":1,"max_fw_concurrency_slots":1,"max_worker_concurrency":1,"min_thermal_headroom_pct":0,"unexpected":1})"};
    if (vqec_vision_ai_admis_hwprf_load(unknown_key, loaded).code_ == status_code::ok ||
        loaded.profile_id_ != preserved_profile_id) {
        throw std::runtime_error("invalid hardware profile changed output");
    }

    // The observed profile admits one source, never the 16-source schema ceiling.
    const std::string candidate_paths[] = {
        _profile_path,
        "/opt/lacai/config/hardware_admission_profile.qcs6490.example.json",
        "/opt/lacai/config/hardware_admission_profile.json"};
    std::ifstream example_file;
    for (const auto& path : candidate_paths) {
        if (path.empty()) {
            continue;
        }
        example_file.clear();
        example_file.open(path);
        if (example_file.is_open()) {
            break;
        }
    }
    if (!example_file.is_open()) {
        throw std::runtime_error("could not open hardware_admission_profile.qcs6490.example.json");
    }
    hardware_admission_profile qcs_profile;
    const auto load_res = vqec_vision_ai_admis_hwprf_load(example_file, qcs_profile);
    if (load_res.code_ != status_code::ok) {
        throw std::runtime_error("failed to load QCS6490 example profile: " + load_res.message_);
    }
    if (qcs_profile.profile_id_ != "qcs6490_rb3gen2_single_source_observed" ||
        qcs_profile.target_id_ != "qcs6490" ||
        qcs_profile.revision_ != 1 || qcs_profile.max_fw_concurrency_slots_ != 1) {
        throw std::runtime_error("QCS6490 example profile fields mismatch");
    }
    activation_snapshot qcs_snapshot;
    if (vqec_vision_ai_admis_actsp_build_snapshot(
            deployment, catalog, qcs_profile, qcs_snapshot).code_ !=
        status_code::unsupported) {
        throw std::runtime_error("single-source QCS6490 profile admitted 16 sources");
    }
    auto single_source = deployment;
    single_source.sources_.resize(1);
    single_source.max_total_resident_bytes_ = 256 * g_mib;
    const auto adm_res = vqec_vision_ai_admis_actsp_build_snapshot(
        single_source, catalog, qcs_profile, qcs_snapshot);
    if (adm_res.code_ != status_code::ok) {
        throw std::runtime_error("QCS6490 example profile failed admission: " + adm_res.message_);
    }
}

}  // namespace
}  // namespace vqec::vision::ai

int main(int _argc, char** _argv) {
    try {
        if (_argc > 2) {
            throw std::runtime_error("usage: vqec_vision_ai_activation_snapshot_test [profile]");
        }
        const std::string profile_path = _argc == 2 ? _argv[1] : "";
        vqec::vision::ai::vqec_vision_ai_unit_astst_check_snapshot(profile_path);
        std::cout << "vqec_vision_activation_snapshot_test: all tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
