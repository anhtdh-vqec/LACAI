#include <cassert>

#include "vqec_vision_service_platform.hpp"

int main() {
    using namespace vqec::vision::ai;

    parsed_arguments args;
    deployment_config deployment;
    deployment.revision_ = 1;
    source_deployment_config source;
    source.source_id_ = "camera_front";
    source.profile_.width_ = 1280;
    source.profile_.height_ = 720;
    source.profile_.fps_numerator_ = 30;
    source.profile_.fps_denominator_ = 1;
    deployment.sources_.push_back(source);
    model_catalog models;
    models.revision_ = 1;
    feature_catalog features;
    model_package_registry packages;
    service_platform owner;

    const auto prepared = owner.vqec_vision_ai_appl_svplt_prepare(
        args, deployment, models, features, packages, false, false);
    assert(prepared.code_ == status_code::ok);
    assert(owner.vqec_vision_ai_appl_svplt_get_activation().source_count_ == 1);
    assert(owner.vqec_vision_ai_appl_svplt_get_activation().sources_[0].source_ != nullptr);
    assert(!owner.vqec_vision_ai_appl_svplt_get_tracker_contract().empty());
    assert(!owner.vqec_vision_ai_appl_svplt_get_attribute_schema().empty());
    assert(owner.vqec_vision_ai_appl_svplt_prepare(
               args, deployment, models, features, packages, false, false)
               .code_ == status_code::invalid_state);

    service_platform missing_source;
    deployment_config empty_deployment;
    assert(missing_source.vqec_vision_ai_appl_svplt_prepare(
               args, empty_deployment, models, features, packages, false, false)
               .code_ == status_code::invalid_state);
    return 0;
}
