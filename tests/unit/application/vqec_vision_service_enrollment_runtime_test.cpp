#include <cassert>

#include "vqec_vision_production_platform.hpp"
#include "vqec_vision_recognition_session.hpp"
#include "vqec_vision_service_enrollment_runtime.hpp"
#include "vqec_vision_service_options.hpp"

using namespace vqec::vision::ai;

int main() {
    parsed_arguments arguments;
    recognition_session recognition;
    production_platform platform;
    source_deployment_config source;
    source.source_id_ = "source.enrollment";
    source.camera_id_ = 1;
    source.channel_id_ = 1;
    source.profile_.width_ = 640;
    source.profile_.height_ = 480;
    model_catalog catalog;
    model_catalog_entry embedding_model;
    embedding_model.model_id_ = "face.embedding";

    service_enrollment_runtime runtime;
    assert(runtime.vqec_vision_ai_appl_svenr_get_port() == nullptr);
    assert(runtime.vqec_vision_ai_appl_svenr_configure(arguments, recognition,
        platform, 0, source, catalog, embedding_model, 7).code_ == status_code::ok);
    assert(runtime.vqec_vision_ai_appl_svenr_get_port() != nullptr);
    assert(runtime.vqec_vision_ai_appl_svenr_poll(8).code_ == status_code::pending);
    assert(runtime.vqec_vision_ai_appl_svenr_stop().code_ == status_code::ok);
    assert(runtime.vqec_vision_ai_appl_svenr_configure(arguments, recognition,
        platform, 0, source, catalog, embedding_model, 9).code_ ==
        status_code::invalid_state);
    return 0;
}
