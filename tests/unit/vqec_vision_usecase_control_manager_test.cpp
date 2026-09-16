#include "vqec_vision_usecase_control_manager.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;

model_catalog_entry vqec_vision_ai_unit_ucmtst_make_model(const std::string& _id) {
    model_catalog_entry model;
    model.model_id_ = _id;
    model.model_version_ = "1.0";
    model.target_id_ = "target";
    model.artifact_ref_ = _id + "_artifact";
    model.artifact_sha256_ = std::string(64, 'a');
    model.output_manifest_ref_ = _id + "_outputs";
    model.decoder_contract_ = _id + ".decoder";
    model.preprocess_contract_ = "nv12_rgb";
    model.graph_name_ = _id + "_graph";
    model.tensor_width_ = 640;
    model.tensor_height_ = 640;
    model.placement_ = image_placement::centre;
    model.inference_fps_numerator_ = 30;
    model.inference_fps_denominator_ = 1;
    model.source_constraints_ = {640, 480, 4096, 2160, 10, 1};
    model.resources_ = {16 * g_mib, 8 * g_mib, 2, 16, true};
    return model;
}

void vqec_vision_ai_unit_ucmtst_make_fixture(
    model_catalog& _models, deployment_config& _deployment,
    usecase_control_snapshot& _control) {
    _models.schema_version_ = model_catalog_limits::g_schema_version;
    _models.revision_ = 3;
    _models.catalog_id_ = "models";
    _models.models_ = {vqec_vision_ai_unit_ucmtst_make_model("person_model"),
        vqec_vision_ai_unit_ucmtst_make_model("face_model")};

    _deployment.schema_version_ = deployment_limits::g_schema_version;
    _deployment.revision_ = 5;
    _deployment.model_catalog_ref_ = "models";
    _deployment.max_total_resident_bytes_ = 256 * g_mib;
    _deployment.max_model_resident_bytes_ = 64 * g_mib;
    source_deployment_config source;
    source.source_id_ = "camera_front";
    source.raw_source_ref_ = "fw_camera_front";
    source.preview_output_ref_ = "detect0";
    source.profile_ = {1920, 1080, 30, 1};
    source.memory_ = {4 * g_mib, 2, 2, 32 * g_mib, 16 * g_mib};
    source.model_ids_ = {"person_model", "face_model"};
    _deployment.sources_.push_back(std::move(source));

    _control.control_revision_ = 1;
    _control.entitlement_revision_ = 2;
    _control.deployment_revision_ = _deployment.revision_;
    _control.catalog_.schema_version_ = usecase_activation_limits::g_schema_version;
    _control.catalog_.revision_ = 4;
    _control.catalog_.catalog_id_ = "usecases";
    _control.catalog_.model_catalog_ref_ = "models";
    _control.catalog_.usecases_ = {
        {"person_detection", "1.0", {"person_model"}, {}},
        {"face_recognition", "1.0", {"face_model"}, {"identity"}}};
    _control.requests_ = {
        {"camera_front", "person_detection", true, true, true, true, true, true},
        {"camera_front", "face_recognition", false, true, true, true, true, true}};
}

void vqec_vision_ai_unit_ucmtst_check_apply_publish_and_idempotency() {
    model_catalog models;
    deployment_config deployment;
    usecase_control_snapshot control;
    vqec_vision_ai_unit_ucmtst_make_fixture(models, deployment, control);
    usecase_control_manager manager;
    if (manager.vqec_vision_ai_ftmgr_ucmgr_configure(
            control, deployment, models).code_ != status_code::ok ||
        manager.vqec_vision_ai_ftmgr_ucmgr_publish_initial(1).code_ != status_code::ok) {
        throw std::runtime_error("usecase manager configure failed");
    }

    usecase_desired_plan plan;
    plan.request_id_ = "request_face_only";
    plan.expected_control_revision_ = 1;
    plan.entries_.push_back({"camera_front", "face_recognition", true});
    usecase_apply_receipt receipt;
    if (manager.vqec_vision_ai_ports_ucctl_apply_desired_plan(plan, receipt).code_ !=
            status_code::ok ||
        !receipt.accepted_ || receipt.control_revision_ != 2 ||
        receipt.apply_state_ != usecase_apply_state::reconciling) {
        throw std::runtime_error("face-only plan was not accepted");
    }
    usecase_control_snapshot pending;
    deployment_config effective;
    if (manager.vqec_vision_ai_ftmgr_ucmgr_get_pending(pending, effective).code_ !=
            status_code::ok ||
        effective.sources_.size() != 1 ||
        effective.sources_[0].model_ids_ != std::vector<std::string>{"face_model"}) {
        throw std::runtime_error("pending face-only generation retained another model");
    }
    usecase_control_status status_snapshot;
    if (manager.vqec_vision_ai_ports_ucctl_get_status(status_snapshot).code_ !=
            status_code::ok ||
        status_snapshot.entries_[1].effective_state_ != usecase_runtime_state::loading ||
        status_snapshot.entries_[0].effective_state_ != usecase_runtime_state::draining ||
        !status_snapshot.entries_[0].loaded_ || status_snapshot.entries_[0].running_) {
        throw std::runtime_error("reconciling status does not distinguish old and new state");
    }
    if (manager.vqec_vision_ai_ftmgr_ucmgr_publish_pending(2, 2).code_ != status_code::ok ||
        manager.vqec_vision_ai_ports_ucctl_get_status(status_snapshot).code_ !=
            status_code::ok ||
        status_snapshot.entries_[1].effective_state_ != usecase_runtime_state::running ||
        !status_snapshot.entries_[1].loaded_ || !status_snapshot.entries_[1].running_) {
        throw std::runtime_error("published face-only generation is not running");
    }
    usecase_apply_receipt retry;
    if (manager.vqec_vision_ai_ports_ucctl_apply_desired_plan(plan, retry).code_ !=
            status_code::ok || retry.control_revision_ != receipt.control_revision_ ||
        retry.apply_state_ != receipt.apply_state_) {
        throw std::runtime_error("idempotent retry did not return the original receipt");
    }
    plan.expected_control_revision_ = 2;
    if (manager.vqec_vision_ai_ports_ucctl_apply_desired_plan(plan, retry).code_ !=
        status_code::invalid_argument) {
        throw std::runtime_error("request ID reuse with another revision was accepted");
    }
}

void vqec_vision_ai_unit_ucmtst_check_rejections_and_capabilities() {
    model_catalog models;
    deployment_config deployment;
    usecase_control_snapshot control;
    vqec_vision_ai_unit_ucmtst_make_fixture(models, deployment, control);
    usecase_control_manager manager;
    if (manager.vqec_vision_ai_ftmgr_ucmgr_configure(
            control, deployment, models).code_ != status_code::ok) {
        throw std::runtime_error("usecase manager configure failed");
    }
    usecase_desired_plan duplicate{"duplicate", 1,
        {{"camera_front", "person_detection", true},
         {"camera_front", "person_detection", false}}};
    usecase_apply_receipt receipt;
    if (manager.vqec_vision_ai_ports_ucctl_apply_desired_plan(duplicate, receipt).code_ !=
        status_code::invalid_argument) {
        throw std::runtime_error("duplicate desired association was accepted");
    }
    usecase_desired_plan unknown{"unknown", 1,
        {{"camera_front", "unknown_usecase", true}}};
    if (manager.vqec_vision_ai_ports_ucctl_apply_desired_plan(unknown, receipt).code_ !=
        status_code::invalid_argument) {
        throw std::runtime_error("unknown desired association was accepted");
    }
    usecase_capability_snapshot capabilities;
    if (manager.vqec_vision_ai_ports_ucctl_get_capabilities(capabilities).code_ !=
            status_code::ok ||
        capabilities.catalog_revision_ != 4 || capabilities.usecases_.size() != 2) {
        throw std::runtime_error("usecase capabilities are incorrect");
    }
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    using namespace vqec::vision::ai;
    vqec_vision_ai_unit_ucmtst_check_apply_publish_and_idempotency();
    vqec_vision_ai_unit_ucmtst_check_rejections_and_capabilities();
    return 0;
}
