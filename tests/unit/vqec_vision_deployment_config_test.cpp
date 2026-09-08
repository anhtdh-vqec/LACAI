#include <vqec/vision/ai/contracts/vqec_vision_deployment_config.hpp>
#include <vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;

source_deployment_config vqec_vision_ai_unit_dptst_make_source(unsigned _index) {
    source_deployment_config source;
    source.source_id_ = "source_" + std::to_string(_index);
    source.raw_source_ref_ = "fw_raw_" + std::to_string(_index);
    source.camera_id_ = _index;
    source.channel_id_ = 0;
    source.preview_output_ref_ = "preview_" + std::to_string(_index);
    source.profile_.width_ = 1920;
    source.profile_.height_ = 1080;
    source.profile_.fps_numerator_ = 25;
    source.profile_.fps_denominator_ = 1;
    source.memory_.max_frame_allocation_bytes_ = 4 * g_mib;
    source.memory_.max_inflight_frames_ = 2;
    source.memory_.preview_surface_count_ = 2;
    source.memory_.max_tensor_bytes_ = 8 * g_mib;
    source.memory_.max_temporal_bytes_ = 16 * g_mib;
    source.model_ids_ = {"person_detector", "person_attributes"};
    return source;
}

deployment_config vqec_vision_ai_unit_dptst_make_config(unsigned _source_count = 1) {
    deployment_config config;
    config.schema_version_ = deployment_limits::g_schema_version;
    config.revision_ = 1;
    config.model_catalog_ref_ = "models_qcs6490_v1";
    config.max_total_resident_bytes_ = 1024 * g_mib;
    config.max_model_resident_bytes_ = 64 * g_mib;
    config.sources_.reserve(_source_count);
    for (unsigned index = 0; index < _source_count; ++index) {
        config.sources_.push_back(vqec_vision_ai_unit_dptst_make_source(index));
    }
    return config;
}

void vqec_vision_ai_unit_dptst_require_status(
    const deployment_config& _config, status_code _expected,
    const char* _case_name) {
    std::uint64_t declared_bytes = 777;
    const auto result =
        vqec_vision_ai_core_dpval_validate_deployment(_config, declared_bytes);
    if (result.code_ != _expected) {
        throw std::runtime_error(std::string(_case_name) + ": " + result.message_);
    }
    if (_expected != status_code::ok && declared_bytes != 777) {
        throw std::runtime_error(std::string(_case_name) + ": changed output on failure");
    }
}

void vqec_vision_ai_unit_dptst_check_source_count_and_identity() {
    vqec_vision_ai_unit_dptst_require_status(
        vqec_vision_ai_unit_dptst_make_config(16), status_code::ok,
        "sixteen sources");
    auto config = vqec_vision_ai_unit_dptst_make_config(17);
    vqec_vision_ai_unit_dptst_require_status(
        config, status_code::invalid_argument, "seventeen sources");
    config = vqec_vision_ai_unit_dptst_make_config(2);
    config.sources_[1].source_id_ = config.sources_[0].source_id_;
    vqec_vision_ai_unit_dptst_require_status(
        config, status_code::invalid_argument, "duplicate source id");
    config = vqec_vision_ai_unit_dptst_make_config(2);
    config.sources_[1].preview_output_ref_ = config.sources_[0].preview_output_ref_;
    vqec_vision_ai_unit_dptst_require_status(
        config, status_code::invalid_argument, "duplicate preview output");
}

void vqec_vision_ai_unit_dptst_check_raw_source_contract() {
    auto config = vqec_vision_ai_unit_dptst_make_config();
    config.sources_[0].raw_source_ref_.clear();
    vqec_vision_ai_unit_dptst_require_status(
        config, status_code::invalid_argument, "missing FW RAW source reference");
    config = vqec_vision_ai_unit_dptst_make_config(2);
    config.sources_[1].raw_source_ref_ = config.sources_[0].raw_source_ref_;
    vqec_vision_ai_unit_dptst_require_status(
        config, status_code::invalid_argument, "duplicate FW RAW source reference");
    config = vqec_vision_ai_unit_dptst_make_config();
    config.sources_[0].memory_.preview_surface_count_ = 0;
    config.sources_[0].preview_output_ref_ = "invalid/output";
    vqec_vision_ai_unit_dptst_require_status(
        config, status_code::invalid_argument, "headless source output");
}

void vqec_vision_ai_unit_dptst_check_profile_and_memory() {
    auto config = vqec_vision_ai_unit_dptst_make_config();
    config.sources_[0].profile_.width_ = 0;
    vqec_vision_ai_unit_dptst_require_status(
        config, status_code::invalid_argument, "implicit width");
    config = vqec_vision_ai_unit_dptst_make_config();
    config.sources_[0].memory_.max_frame_allocation_bytes_ = 1;
    vqec_vision_ai_unit_dptst_require_status(
        config, status_code::invalid_argument, "undersized frame allocation");
    config = vqec_vision_ai_unit_dptst_make_config();
    config.max_total_resident_bytes_ = config.max_model_resident_bytes_;
    vqec_vision_ai_unit_dptst_require_status(
        config, status_code::resource_exhausted, "resident budget");
    config = vqec_vision_ai_unit_dptst_make_config();
    config.schema_version_ = 2;
    vqec_vision_ai_unit_dptst_require_status(
        config, status_code::unsupported, "schema version");
}

void vqec_vision_ai_unit_dptst_check_profile_binding() {
    auto config = vqec_vision_ai_unit_dptst_make_config();
    inference_plan plan;
    plan.tensor_width_ = 640;
    plan.tensor_height_ = 640;
    plan.placement_ = image_placement::centre;
    plan.model_path_ = "/opt/vqec/models/person.bin";
    plan.backend_path_ = "/usr/lib/libQnnHtp.so";
    plan.system_path_ = "/usr/lib/libQnnSystem.so";
    plan.input_queue_bytes_ = 8 * g_mib;
    plan.output_queue_buffers_ = 2;
    auto result = vqec_vision_ai_core_dpval_bind_source_profile(config.sources_[0], plan);
    if (result.code_ != status_code::ok || plan.source_width_ != 1920 ||
        plan.source_height_ != 1080 || plan.fps_numerator_ != 25) {
        throw std::runtime_error("source profile was not bound to model plan");
    }
    const auto preserved = plan;
    config.sources_[0].profile_.width_ = 0;
    result = vqec_vision_ai_core_dpval_bind_source_profile(config.sources_[0], plan);
    if (result.code_ == status_code::ok || plan.source_width_ != preserved.source_width_) {
        throw std::runtime_error("failed source binding changed model plan");
    }
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    try {
        vqec::vision::ai::vqec_vision_ai_unit_dptst_check_source_count_and_identity();
        vqec::vision::ai::vqec_vision_ai_unit_dptst_check_raw_source_contract();
        vqec::vision::ai::vqec_vision_ai_unit_dptst_check_profile_and_memory();
        vqec::vision::ai::vqec_vision_ai_unit_dptst_check_profile_binding();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
