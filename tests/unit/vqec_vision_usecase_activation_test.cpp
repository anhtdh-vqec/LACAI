#include <vqec/vision/ai/contracts/vqec_vision_usecase_activation.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;

model_catalog_entry vqec_vision_ai_unit_ucatst_make_model(const std::string& _model_id) {
    model_catalog_entry model;
    model.model_id_ = _model_id;
    model.model_version_ = "1.0";
    model.target_id_ = "target";
    model.artifact_ref_ = _model_id + "_artifact";
    model.artifact_sha256_ = std::string(64, 'a');
    model.output_manifest_ref_ = _model_id + "_outputs";
    model.decoder_contract_ = _model_id + ".decoder";
    model.preprocess_contract_ = "nv12_rgb";
    model.graph_name_ = _model_id + "_graph";
    model.tensor_width_ = 640;
    model.tensor_height_ = 640;
    model.placement_ = image_placement::centre;
    model.inference_fps_numerator_ = 30;
    model.inference_fps_denominator_ = 1;
    model.source_constraints_ = {640, 480, 4096, 2160, 10, 1};
    model.resources_ = {16 * g_mib, 8 * g_mib, 2, 16, true};
    return model;
}

model_catalog vqec_vision_ai_unit_ucatst_make_models() {
    model_catalog models;
    models.schema_version_ = model_catalog_limits::g_schema_version;
    models.revision_ = 4;
    models.catalog_id_ = "models";
    models.models_ = {
        vqec_vision_ai_unit_ucatst_make_model("person_model"),
        vqec_vision_ai_unit_ucatst_make_model("face_model"),
        vqec_vision_ai_unit_ucatst_make_model("embedding_model")};
    auto& embedding = models.models_.back();
    embedding.role_ = model_role::secondary;
    embedding.depends_on_ = {{"face_model", "1.0", "target"}};
    return models;
}

deployment_config vqec_vision_ai_unit_ucatst_make_deployment() {
    deployment_config deployment;
    deployment.schema_version_ = deployment_limits::g_schema_version;
    deployment.revision_ = 8;
    deployment.model_catalog_ref_ = "models";
    deployment.max_total_resident_bytes_ = 256 * g_mib;
    deployment.max_model_resident_bytes_ = 64 * g_mib;
    source_deployment_config source;
    source.source_id_ = "camera_front";
    source.raw_source_ref_ = "fw_camera_front";
    source.preview_output_ref_ = "detect0";
    source.profile_ = {1920, 1080, 30, 1};
    source.memory_ = {4 * g_mib, 2, 2, 32 * g_mib, 16 * g_mib};
    source.cascade_ = {2, 8, 16 * g_mib};
    source.model_ids_ = {"person_model", "face_model"};
    deployment.sources_.push_back(std::move(source));
    return deployment;
}

usecase_catalog vqec_vision_ai_unit_ucatst_make_usecases() {
    usecase_catalog usecases;
    usecases.schema_version_ = usecase_activation_limits::g_schema_version;
    usecases.revision_ = 3;
    usecases.catalog_id_ = "commercial_usecases";
    usecases.model_catalog_ref_ = "models";
    usecases.usecases_ = {
        {"person_detection", "1.0", {"person_model"}, {}},
        {"face_recognition", "1.0", {"face_model"}, {"identity"}},
        {"people_counting", "1.0", {"person_model"}, {"count"}}};
    return usecases;
}

usecase_activation_request vqec_vision_ai_unit_ucatst_make_request(
    const std::string& _usecase_id, bool _desired) {
    return {"camera_front", _usecase_id, _desired, true, true, true, true, true};
}

void vqec_vision_ai_unit_ucatst_check_model_filtering() {
    const auto models = vqec_vision_ai_unit_ucatst_make_models();
    const auto base = vqec_vision_ai_unit_ucatst_make_deployment();
    const auto usecases = vqec_vision_ai_unit_ucatst_make_usecases();
    std::vector<usecase_activation_request> requests = {
        vqec_vision_ai_unit_ucatst_make_request("person_detection", true),
        vqec_vision_ai_unit_ucatst_make_request("face_recognition", false),
        vqec_vision_ai_unit_ucatst_make_request("people_counting", false)};
    usecase_activation_snapshot snapshot;
    deployment_config effective;
    auto result = vqec_vision_ai_core_ucact_compose_effective_deployment(
        base, models, usecases, requests, snapshot, effective);
    if (result.code_ != status_code::ok || effective.sources_.size() != 1 ||
        effective.sources_[0].model_ids_ != std::vector<std::string>{"person_model"} ||
        effective.sources_[0].cascade_.max_bytes_ != 0) {
        throw std::runtime_error("person-only plan retained face/cascade resources");
    }

    requests[0].desired_enabled_ = false;
    requests[1].desired_enabled_ = true;
    result = vqec_vision_ai_core_ucact_compose_effective_deployment(
        base, models, usecases, requests, snapshot, effective);
    if (result.code_ != status_code::ok ||
        effective.sources_[0].model_ids_ != std::vector<std::string>{"face_model"} ||
        effective.sources_[0].cascade_.max_bytes_ == 0) {
        throw std::runtime_error("FR plan did not retain cascade resources");
    }

    requests[0].desired_enabled_ = true;
    result = vqec_vision_ai_core_ucact_compose_effective_deployment(
        base, models, usecases, requests, snapshot, effective);
    if (result.code_ != status_code::ok || effective.sources_[0].model_ids_.size() != 2) {
        throw std::runtime_error("combined person and FR plan was not composed");
    }
}

void vqec_vision_ai_unit_ucatst_check_denial_idle_and_shared_root() {
    const auto models = vqec_vision_ai_unit_ucatst_make_models();
    const auto base = vqec_vision_ai_unit_ucatst_make_deployment();
    const auto usecases = vqec_vision_ai_unit_ucatst_make_usecases();
    std::vector<usecase_activation_request> requests = {
        vqec_vision_ai_unit_ucatst_make_request("person_detection", false),
        vqec_vision_ai_unit_ucatst_make_request("face_recognition", true),
        vqec_vision_ai_unit_ucatst_make_request("people_counting", false)};
    requests[1].entitlement_granted_ = false;
    usecase_activation_snapshot snapshot;
    deployment_config effective;
    auto result = vqec_vision_ai_core_ucact_compose_effective_deployment(
        base, models, usecases, requests, snapshot, effective);
    if (result.code_ != status_code::ok || !effective.sources_.empty() ||
        snapshot.records_[1].state_ != usecase_effective_state::denied) {
        throw std::runtime_error("denied usecase loaded a model");
    }

    requests[2].desired_enabled_ = true;
    result = vqec_vision_ai_core_ucact_compose_effective_deployment(
        base, models, usecases, requests, snapshot, effective);
    if (result.code_ != status_code::ok || effective.sources_.size() != 1 ||
        effective.sources_[0].model_ids_ != std::vector<std::string>{"person_model"}) {
        throw std::runtime_error("shared root was released while still referenced");
    }
}

void vqec_vision_ai_unit_ucatst_check_transactional_validation() {
    const auto models = vqec_vision_ai_unit_ucatst_make_models();
    const auto base = vqec_vision_ai_unit_ucatst_make_deployment();
    const auto usecases = vqec_vision_ai_unit_ucatst_make_usecases();
    std::vector<usecase_activation_request> requests = {
        vqec_vision_ai_unit_ucatst_make_request("person_detection", true),
        vqec_vision_ai_unit_ucatst_make_request("face_recognition", true)};
    usecase_activation_snapshot snapshot;
    snapshot.deployment_revision_ = 99;
    deployment_config effective;
    effective.revision_ = 77;
    const auto result = vqec_vision_ai_core_ucact_compose_effective_deployment(
        base, models, usecases, requests, snapshot, effective);
    if (result.code_ != status_code::invalid_argument ||
        snapshot.deployment_revision_ != 99 || effective.revision_ != 77) {
        throw std::runtime_error("invalid plan changed transactional output");
    }
}

void vqec_vision_ai_unit_ucatst_check_feature_authority_projection() {
    auto usecases = vqec_vision_ai_unit_ucatst_make_usecases();
    usecases.usecases_[0].feature_ids_ = {"shared_feature"};
    usecases.usecases_[2].feature_ids_ = {"shared_feature"};

    usecase_activation_snapshot snapshot;
    snapshot.usecase_catalog_revision_ = usecases.revision_;
    usecase_activation_record denied;
    denied.source_id_ = "camera_front";
    denied.usecase_id_ = "person_detection";
    denied.state_ = usecase_effective_state::denied;
    denied.desired_enabled_ = true;
    denied.entitlement_granted_ = false;
    denied.resource_admitted_ = true;
    usecase_activation_record disabled;
    disabled.source_id_ = "camera_front";
    disabled.usecase_id_ = "people_counting";
    disabled.state_ = usecase_effective_state::disabled;
    disabled.desired_enabled_ = false;
    disabled.entitlement_granted_ = true;
    disabled.resource_admitted_ = true;
    snapshot.records_ = {denied, disabled};

    feature_authority_projection projection;
    const auto projected = vqec_vision_ai_core_ucact_project_feature_authority(
        usecases, snapshot, "camera_front", "shared_feature", projection);
    if (projected.code_ != status_code::ok || !projection.has_association_ ||
        !projection.desired_enabled_ || projection.entitlement_granted_ ||
        projection.resource_admitted_) {
        throw std::runtime_error("feature authority combined gates across usecases");
    }

    snapshot.records_[1].desired_enabled_ = true;
    snapshot.records_[1].state_ = usecase_effective_state::ready;
    const auto ready = vqec_vision_ai_core_ucact_project_feature_authority(
        usecases, snapshot, "camera_front", "shared_feature", projection);
    if (ready.code_ != status_code::ok || !projection.desired_enabled_ ||
        !projection.entitlement_granted_ || !projection.resource_admitted_) {
        throw std::runtime_error("ready usecase did not authorize its shared feature");
    }

    auto stale = snapshot;
    stale.usecase_catalog_revision_ += 1;
    const auto preserved = projection;
    if (vqec_vision_ai_core_ucact_project_feature_authority(
            usecases, stale, "camera_front", "shared_feature", projection).code_ !=
            status_code::invalid_argument ||
        projection.resource_admitted_ != preserved.resource_admitted_) {
        throw std::runtime_error("stale authority projection changed output");
    }
}

void vqec_vision_ai_unit_ucatst_check_feature_association_projection() {
    auto usecases = vqec_vision_ai_unit_ucatst_make_usecases();
    usecases.usecases_[0].feature_ids_ = {"person_feature"};
    usecases.usecases_[2].feature_ids_ = {"person_feature"};

    const auto deployment = vqec_vision_ai_unit_ucatst_make_deployment();
    usecase_activation_snapshot snapshot;
    snapshot.usecase_catalog_revision_ = usecases.revision_;
    snapshot.deployment_revision_ = deployment.revision_;
    snapshot.model_catalog_revision_ = 4;
    snapshot.policy_revision_ = 10;
    snapshot.config_revision_ = 20;

    // Usecase 0 (person_detection): ready
    usecase_activation_record rec_person;
    rec_person.source_id_ = "camera_front";
    rec_person.usecase_id_ = "person_detection";
    rec_person.state_ = usecase_effective_state::ready;
    rec_person.reason_code_ = status_code::ok;
    rec_person.desired_enabled_ = true;
    rec_person.installed_ = true;
    rec_person.entitlement_granted_ = true;
    rec_person.supported_ = true;
    rec_person.compatible_ = true;
    rec_person.resource_admitted_ = true;

    // Usecase 2 (people_counting): disabled
    usecase_activation_record rec_count;
    rec_count.source_id_ = "camera_front";
    rec_count.usecase_id_ = "people_counting";
    rec_count.state_ = usecase_effective_state::disabled;
    rec_count.reason_code_ = status_code::ok;
    rec_count.desired_enabled_ = false;
    rec_count.installed_ = true;
    rec_count.entitlement_granted_ = false;
    rec_count.supported_ = true;
    rec_count.compatible_ = true;
    rec_count.resource_admitted_ = false;

    snapshot.records_ = {rec_person, rec_count};

    feature_scoped_association_record association;
    auto projected = vqec_vision_ai_core_ucact_project_feature_association(
        usecases, snapshot, deployment, "camera_front", "person_feature", association);
    if (projected.code_ != status_code::ok || !association.is_ready() ||
        association.usecase_id_ != "person_detection" ||
        association.feature_id_ != "person_feature" ||
        association.model_slot_ != 0 ||
        association.policy_revision_ != 10 ||
        association.config_revision_ != 20 ||
        association.deployment_revision_ != deployment.revision_ ||
        association.usecase_catalog_revision_ != usecases.revision_) {
        throw std::runtime_error("scoped association projection failed to project ready usecase");
    }

    // Policy Revoke test: revoke entitlement on person_detection
    snapshot.records_[0].entitlement_granted_ = false;
    snapshot.records_[0].state_ = usecase_effective_state::denied;
    snapshot.records_[0].reason_code_ = status_code::unauthorized;
    projected = vqec_vision_ai_core_ucact_project_feature_association(
        usecases, snapshot, deployment, "camera_front", "person_feature", association);
    if (projected.code_ != status_code::ok || association.is_ready() ||
        association.state_ != usecase_effective_state::denied ||
        association.reason_code_ != status_code::unauthorized ||
        association.entitlement_granted_) {
        throw std::runtime_error("revoked policy did not deny scoped association");
    }

    // Negative cross-grant test:
    // Usecase A has desired=true, but entitled=false. Usecase B has desired=false, but entitled=true.
    snapshot.records_[0].desired_enabled_ = true;
    snapshot.records_[0].entitlement_granted_ = false;
    snapshot.records_[0].state_ = usecase_effective_state::denied;
    snapshot.records_[1].desired_enabled_ = false;
    snapshot.records_[1].entitlement_granted_ = true;
    snapshot.records_[1].state_ = usecase_effective_state::disabled;
    projected = vqec_vision_ai_core_ucact_project_feature_association(
        usecases, snapshot, deployment, "camera_front", "person_feature", association);
    if (projected.code_ != status_code::ok || association.is_ready()) {
        throw std::runtime_error("cross-grant combining allowed unverified ready association");
    }

    // All-off test:
    snapshot.records_[0].desired_enabled_ = false;
    snapshot.records_[0].state_ = usecase_effective_state::disabled;
    snapshot.records_[1].desired_enabled_ = false;
    snapshot.records_[1].state_ = usecase_effective_state::disabled;
    projected = vqec_vision_ai_core_ucact_project_feature_association(
        usecases, snapshot, deployment, "camera_front", "person_feature", association);
    if (projected.code_ != status_code::ok || association.is_ready() ||
        association.state_ != usecase_effective_state::disabled) {
        throw std::runtime_error("all-off did not yield disabled scoped association");
    }
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    try {
        vqec::vision::ai::vqec_vision_ai_unit_ucatst_check_model_filtering();
        vqec::vision::ai::vqec_vision_ai_unit_ucatst_check_denial_idle_and_shared_root();
        vqec::vision::ai::vqec_vision_ai_unit_ucatst_check_transactional_validation();
        vqec::vision::ai::vqec_vision_ai_unit_ucatst_check_feature_authority_projection();
        vqec::vision::ai::vqec_vision_ai_unit_ucatst_check_feature_association_projection();
    } catch (const std::exception& error) {
        return error.what() == nullptr ? 2 : 1;
    }
    return 0;
}
