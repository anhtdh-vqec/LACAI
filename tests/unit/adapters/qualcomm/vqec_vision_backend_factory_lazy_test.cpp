#include <iostream>
#include <memory>

#include "vqec_vision_backend_factory.hpp"

namespace {

bool vqec_vision_ai_unit_bflzt_check(bool _condition, const char* _message) {
    if (!_condition) {
        std::cerr << _message << '\n';
    }
    return _condition;
}

}  // namespace

int main() {
    using namespace vqec::vision::ai;
    resolved_model_paths paths;
    paths.model_id_ = "fixture";
    paths.model_path_ = "/not-loaded/fixture-model.so";
    paths.backend_path_ = "/not-loaded/libQnnHtp.so";
    paths.system_path_ = "/not-loaded/libQnnSystem.so";
    inference_execution_policy policy;
    std::unique_ptr<qnn_backend_bundle> bundle;
    const auto created = vqec_vision_ai_qcom_bfact_create(paths, policy, bundle);
    if (!vqec_vision_ai_unit_bflzt_check(
            created.code_ == status_code::ok && bundle != nullptr,
            "backend factory rejected a structurally valid deferred recipe")) {
        return 1;
    }
    auto* engine = bundle->vqec_vision_ai_qcom_bfact_get_engine();
    auto* graph = bundle->vqec_vision_ai_qcom_bfact_get_graph();
    const bool valid =
        vqec_vision_ai_unit_bflzt_check(engine != nullptr && graph != nullptr,
            "backend factory did not create its neutral graph owner") &&
        vqec_vision_ai_unit_bflzt_check(
            engine->vqec_vision_ai_qcom_qneng_is_configured() &&
                !engine->vqec_vision_ai_qcom_qneng_is_open(),
            "backend factory touched QNN/HTP before a frame") &&
        vqec_vision_ai_unit_bflzt_check(
            graph->vqec_vision_ai_ports_infgr_validate_activation().code_ ==
                status_code::ok,
            "configured deferred graph failed neutral activation validation");
    return valid ? 0 : 1;
}
