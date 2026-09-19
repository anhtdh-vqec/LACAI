#include <cassert>

#include "vqec_vision_service_output_runtime.hpp"

namespace {

using namespace vqec::vision::ai;

class test_event_sink final : public feature_event_sink_port {
public:
    status vqec_vision_ai_ports_fesnk_deliver_event(
        const feature_event&) override {
        ++delivered_;
        return {};
    }

    std::size_t delivered_{0};
};

void vqec_vision_ai_unit_svotst_test_fallback_lifecycle() {
    parsed_arguments args;
    deployment_config deployment;
    test_event_sink fallback;
    service_output_runtime runtime;
    assert(runtime.vqec_vision_ai_appl_svout_start(args, deployment, fallback).code_ ==
        status_code::ok);
    assert(&runtime.vqec_vision_ai_appl_svout_get_sink() == &fallback);
    assert(runtime.vqec_vision_ai_appl_svout_get_metadata() == nullptr);
    assert(!runtime.vqec_vision_ai_appl_svout_is_metadata_required());
    assert(runtime.vqec_vision_ai_appl_svout_start(args, deployment, fallback).code_ ==
        status_code::invalid_state);
    assert(runtime.vqec_vision_ai_appl_svout_start_delivery().code_ == status_code::ok);
    assert(runtime.vqec_vision_ai_appl_svout_start_delivery().code_ ==
        status_code::invalid_state);
    service_output_runtime_report report;
    assert(runtime.vqec_vision_ai_appl_svout_stop(true, report).code_ == status_code::ok);
    assert(!report.has_metadata_ && !report.has_evidence_);
    assert(runtime.vqec_vision_ai_appl_svout_start(args, deployment, fallback).code_ ==
        status_code::ok);
}

void vqec_vision_ai_unit_svotst_test_partial_evidence_fails_closed() {
    parsed_arguments args;
    args.evidence_socket_path = "/tmp/evidence.sock";
    deployment_config deployment;
    test_event_sink fallback;
    service_output_runtime runtime;
    const auto started =
        runtime.vqec_vision_ai_appl_svout_start(args, deployment, fallback);
    assert(started.code_ == status_code::invalid_argument);
}

}  // namespace

int main() {
    vqec_vision_ai_unit_svotst_test_fallback_lifecycle();
    vqec_vision_ai_unit_svotst_test_partial_evidence_fails_closed();
    return 0;
}
