#include <cassert>
#include <utility>

#include "vqec_vision_service_feature_activation.hpp"
#include "vqec_vision_service_fixture.hpp"

namespace {

using namespace vqec::vision::ai;

void vqec_vision_ai_unit_sfatst_test_runtime_model_projection() {
    model_catalog catalog;
    model_catalog_entry model;
    model.model_id_ = "yolo11n_fire_smoke";
    model.model_version_ = "1.0";
    model.target_id_ = "qcs6490";
    model.artifact_ref_ = "original.so";
    model.artifact_sha256_ =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    catalog.models_.push_back(model);
    model_package_registry registry;
    registry.schema_version_ = model_package_registry_limits::g_schema_version;
    registry.bindings_.push_back({"yolo11n_fire_smoke", "1.0", "qcs6490",
        "original.so", "/opt/lacai/models/yolo11n_fire_smoke/package",
        "/opt/lacai/models/yolo11n_fire_smoke/original.so"});
    runtime_control_snapshot runtime;
    app_runtime_association association;
    association.installed_ = true;
    association.entitled_ = true;
    association.desired_ = true;
    association.supported_ = true;
    association.compatible_ = true;
    association.admitted_ = true;
    association.components_.push_back({"yolo11n_fire_smoke", "1.0",
        app_component_type::model, "qcs6490_qlinux_1_8",
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        1024U,
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
        app_model_role::primary,
        "/opt/lacai/models/app_content/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"});
    runtime.associations_.push_back(association);
    assert(vqec_vision_ai_appl_svstr_apply_runtime_models(
               runtime, catalog, registry)
               .code_ == status_code::ok);
    assert(catalog.models_[0].artifact_sha256_ ==
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    assert(registry.bindings_[0].model_library_ ==
        "/opt/lacai/models/app_content/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

    auto conflicting = association;
    conflicting.components_[0].artifact_sha256_ =
        "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";
    runtime.associations_.push_back(std::move(conflicting));
    assert(vqec_vision_ai_appl_svstr_apply_runtime_models(
               runtime, catalog, registry)
               .code_ == status_code::invalid_state);
}

}  // namespace

int main() {
    using namespace vqec::vision::ai;

    vqec_vision_ai_unit_sfatst_test_runtime_model_projection();

    service_startup_resolution startup;
    parsed_arguments args;
    deployment_config deployment;
    deployment.revision_ = 1;
    source_deployment_config source;
    source.source_id_ = "camera_front";
    deployment.sources_.push_back(source);
    model_catalog models;
    models.revision_ = 1;
    feature_catalog features;
    feature_processor_registry registry;
    output_gate gate;
    service_feature_activation owner;

    const auto configured = owner.vqec_vision_ai_appl_svfac_configure(
        startup, args, deployment, models, features, registry,
        "fixture.attribute", gate, 1);
    assert(configured.code_ == status_code::ok);
    assert(owner.vqec_vision_ai_appl_svfac_get_wiring() == nullptr);
    assert(gate.vqec_vision_ai_core_otgat_get_revision() ==
        service_harness::g_policy_revision);

    output_authorization preview;
    preview.policy_revision_ = service_harness::g_policy_revision;
    preview.source_id_ = "camera_front";
    preview.feature_id_ = "preview";
    preview.attributes_.push_back("overlay");
    assert(gate.vqec_vision_ai_core_otgat_authorize(preview, 1).code_ ==
        status_code::ok);
    assert(owner.vqec_vision_ai_appl_svfac_configure(
               startup, args, deployment, models, features, registry,
               "fixture.attribute", gate, 1)
               .code_ == status_code::invalid_state);
    return 0;
}
