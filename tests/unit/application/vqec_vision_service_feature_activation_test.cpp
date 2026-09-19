#include <cassert>

#include "vqec_vision_service_feature_activation.hpp"
#include "vqec_vision_service_fixture.hpp"

int main() {
    using namespace vqec::vision::ai;

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
