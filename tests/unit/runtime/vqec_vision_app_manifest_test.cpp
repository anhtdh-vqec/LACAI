#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

#include "vqec_vision_app_manifest.hpp"

namespace {

using namespace vqec::vision::ai;

std::string vqec_vision_ai_unit_apmtst_fixture() {
    std::ifstream stream(VQEC_VISION_AI_APP_MANIFEST_FIXTURE, std::ios::binary);
    assert(stream);
    std::ostringstream document;
    document << stream.rdbuf();
    return document.str();
}

void vqec_vision_ai_unit_apmtst_test_valid_manifest() {
    std::istringstream stream(vqec_vision_ai_unit_apmtst_fixture());
    usecase_app_manifest manifest;
    const auto loaded = vqec_vision_ai_lifec_apmft_load(stream, manifest);
    assert(loaded.code_ == status_code::ok);
    assert(manifest.schema_version_ == app_lifecycle_limits::g_schema_version);
    assert(manifest.app_id_ == "security.fire_smoke_detection");
    assert(manifest.app_id_ == manifest.usecase_id_);
    assert(manifest.components_.size() == 3);
    assert(manifest.features_.size() == 1);
    assert(manifest.features_[0].processor_contract_ == "fire_smoke_alarm");
    assert(!manifest.uninstall_purges_data_);
}

void vqec_vision_ai_unit_apmtst_test_strict_and_preserving() {
    auto document = vqec_vision_ai_unit_apmtst_fixture();
    const auto marker = document.find("\"schema_version\": 1");
    assert(marker != std::string::npos);
    document.replace(marker, std::string("\"schema_version\": 1").size(),
        "\"schema_version\": 1, \"unknown\": true");
    std::istringstream stream(document);
    usecase_app_manifest manifest;
    manifest.app_id_ = "preserved";
    assert(vqec_vision_ai_lifec_apmft_load(stream, manifest).code_ ==
        status_code::invalid_argument);
    assert(manifest.app_id_ == "preserved");

    document = vqec_vision_ai_unit_apmtst_fixture();
    const auto digest = document.find(
        "4b74ab5cfea57042dc9dbf26f19633552e08562c3e8a93c416e3e9cde2e0b513");
    assert(digest != std::string::npos);
    document[digest] = 'z';
    std::istringstream bad_digest(document);
    assert(vqec_vision_ai_lifec_apmft_load(bad_digest, manifest).code_ ==
        status_code::invalid_argument);
    assert(manifest.app_id_ == "preserved");
}

void vqec_vision_ai_unit_apmtst_test_document_bound() {
    std::string oversized(app_lifecycle_limits::g_max_document_bytes + 1U, 'x');
    std::istringstream stream(oversized);
    usecase_app_manifest manifest;
    assert(vqec_vision_ai_lifec_apmft_load(stream, manifest).code_ ==
        status_code::resource_exhausted);
}

void vqec_vision_ai_unit_apmtst_test_runtime_snapshot_gates() {
    runtime_control_snapshot snapshot;
    snapshot.schema_version_ = 1;
    snapshot.snapshot_revision_ = 4;
    snapshot.inventory_revision_ = 2;
    snapshot.entitlement_revision_ = 3;
    snapshot.desired_revision_ = 4;
    app_runtime_association association;
    association.app_id_ = "security.fire_smoke_detection";
    association.source_id_ = "camera_front";
    association.app_version_ = "1.0.0";
    association.release_sequence_ = 1;
    association.installed_ = true;
    association.entitled_ = true;
    association.desired_ = true;
    association.supported_ = true;
    association.compatible_ = true;
    association.admitted_ = true;
    association.configuration_revision_ = 5;
    association.configuration_sha256_ =
        "07e72c1c0bdb8762f3c2771c0d46f5207b8ae9af9a04ef4a7832d2104930e935";
    association.configuration_schema_id_ = "security.fire_smoke.configuration";
    association.configuration_payload_ = {'{', '}'};
    association.output_scopes_ = {"security.fire_smoke.event"};
    association.components_.push_back({"yolo11n_fire_smoke", "1.0",
        app_component_type::model, "qcs6490_qlinux_1_8",
        "4b74ab5cfea57042dc9dbf26f19633552e08562c3e8a93c416e3e9cde2e0b513",
        3442520U,
        "436ea6a5df7eb7d8e13706639a7ebc1b3f6e6459a80703fd459ca01af982be42",
        app_model_role::primary,
        "/opt/lacai/models/app_content/4b74ab5cfea57042dc9dbf26f19633552e08562c3e8a93c416e3e9cde2e0b513"});
    association.entitlement_expires_utc_ns_ = 9000000000000000000ULL;
    snapshot.associations_.push_back(association);
    assert(vqec_vision_ai_core_applc_validate_runtime_snapshot(snapshot).code_ ==
        status_code::ok);
    assert(snapshot.associations_[0].is_effective());
    snapshot.associations_[0].entitled_ = false;
    assert(!snapshot.associations_[0].is_effective());
    snapshot.associations_.push_back(snapshot.associations_[0]);
    assert(vqec_vision_ai_core_applc_validate_runtime_snapshot(snapshot).code_ ==
        status_code::invalid_argument);
}

}  // namespace

int main() {
    vqec_vision_ai_unit_apmtst_test_valid_manifest();
    vqec_vision_ai_unit_apmtst_test_strict_and_preserving();
    vqec_vision_ai_unit_apmtst_test_document_bound();
    vqec_vision_ai_unit_apmtst_test_runtime_snapshot_gates();
    return 0;
}
