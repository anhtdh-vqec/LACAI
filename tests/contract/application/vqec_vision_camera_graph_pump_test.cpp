#include <iostream>
#include <memory>

#include "vqec_vision_camera_graph_pump.hpp"
#include "vqec_vision_inference_graph.hpp"
#include "vqec_vision_source_lifecycle.hpp"

int main() {
    using namespace vqec::vision::ai;
    source_lifecycle source({}, {});
    plugin_graph graph;
    auto domain = std::make_shared<graph_retention>();
    qualcomm_inference_graph graph_port(graph, domain);
    tensor_result result;
    result.pipeline_pts_ns_ = 777;
    camera_pump_report report;
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    camera_graph_pump invalid(source, graph_port, 0, 1000);
    report.has_result_ = true;
    check(invalid.vqec_vision_ai_appl_cgpmp_pump_step(0, result, report).code_ ==
          status_code::invalid_argument);
    check(!report.has_result_ && !report.has_submission_ && result.pipeline_pts_ns_ == 777);
    camera_graph_pump unstarted(source, graph_port, 1, 1000);
    check(unstarted.vqec_vision_ai_appl_cgpmp_pump_step(0, result, report).code_ ==
          status_code::invalid_state);
    check(source.vqec_vision_ai_camer_srclc_get_state() == camera_source_state::idle);
    check(graph.vqec_vision_ai_qcom_plgr_get_state() == plugin_graph_state::empty);
    unstarted.vqec_vision_ai_appl_cgpmp_begin_stop();
    unstarted.vqec_vision_ai_appl_cgpmp_begin_stop();
    check(unstarted.vqec_vision_ai_appl_cgpmp_pump_step(0, result, report).code_ ==
          status_code::pending);
    check(source.vqec_vision_ai_camer_srclc_get_outstanding() == 0);
    check(domain->vqec_vision_ai_qcom_plgr_get_retained_count() == 0);
    check(result.pipeline_pts_ns_ == 777);
    std::cout << "camera graph pump guard failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
