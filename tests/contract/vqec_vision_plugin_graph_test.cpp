#include <iostream>
#include <string>
#include <vector>

#include "vqec_vision_plugin_graph.hpp"

int main() {
    using vqec::vision::ai::plugin_graph;
    using vqec::vision::ai::plugin_graph_state;
    using vqec::vision::ai::status_code;
    plugin_graph graph;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    check(!graph.vqec_vision_ai_qcom_plgr_is_configured());
    check(graph.vqec_vision_ai_qcom_plgr_get_state() == plugin_graph_state::empty);
    check(graph.vqec_vision_ai_qcom_plgr_load_model().code_ == status_code::invalid_state);
    check(graph.vqec_vision_ai_qcom_plgr_poll_state().code_ == status_code::invalid_state);
    check(graph.vqec_vision_ai_qcom_plgr_unload_model().code_ == status_code::invalid_state);
    check(graph.vqec_vision_ai_qcom_plgr_bind_source({}).code_ == status_code::invalid_state);
    check(graph.vqec_vision_ai_qcom_plgr_start_stream({}, 1024).code_ ==
          status_code::invalid_state);
    check(graph.vqec_vision_ai_qcom_plgr_request_drain().code_ == status_code::invalid_state);
    auto retention = std::make_shared<vqec::vision::ai::graph_retention>();
    check(retention->vqec_vision_ai_qcom_plgr_get_retained_count() == 0);
    check(graph.vqec_vision_ai_qcom_plgr_arm_submission(1, 1, 1000, retention).code_ ==
          status_code::invalid_state);
    check(graph.vqec_vision_ai_qcom_plgr_poll_jobs(0).code_ == status_code::invalid_state);
    check(graph.vqec_vision_ai_qcom_plgr_get_outstanding() == 0);
    check(graph.vqec_vision_ai_qcom_plgr_get_pending_ticket().token_.job_id_ == 0);
    vqec::vision::ai::submission_ticket ticket;
    check(graph.vqec_vision_ai_qcom_plgr_submit_frame({}, -1, {}, 0, ticket).code_ ==
          status_code::invalid_state);
    check(ticket.token_.job_id_ == 0);
    check(retention->vqec_vision_ai_qcom_plgr_restore_graph(4, graph, retention).code_ ==
          status_code::invalid_argument);
    vqec::vision::ai::tensor_result output;
    output.pipeline_pts_ns_ = 777;
    check(graph.vqec_vision_ai_qcom_plgr_poll_result(0, output).code_ ==
          status_code::invalid_state);
    check(graph.vqec_vision_ai_qcom_plgr_poll_output(123, output).code_ ==
          status_code::invalid_state);
    check(output.pipeline_pts_ns_ == 777);
    check(graph.vqec_vision_ai_qcom_plgr_get_warning_count() == 0);
    check(graph.vqec_vision_ai_qcom_plgr_get_last_error().message_.empty());
    std::vector<vqec::vision::ai::plugin_factory_capability> capabilities;
    check(graph.vqec_vision_ai_qcom_plgr_probe_factories({}, capabilities).code_ ==
          status_code::ok);
    check(capabilities.empty());
    check(graph.vqec_vision_ai_qcom_plgr_probe_factories(
              {"vqec_vision_ai_factory_that_does_not_exist"}, capabilities).code_ ==
          status_code::ok);
    check(capabilities.size() == 1U && !capabilities.front().available_);
    check(graph.vqec_vision_ai_qcom_plgr_probe_factories(
              {std::string(129U, 'x')}, capabilities).code_ == status_code::invalid_argument);
    std::vector<vqec::vision::ai::plugin_property_capability> properties;
    check(graph.vqec_vision_ai_qcom_plgr_probe_properties(
              "vqec_vision_ai_factory_that_does_not_exist", {"model"}, properties).code_ ==
          status_code::missing_plugin);
    check(properties.empty());
    check(graph.vqec_vision_ai_qcom_plgr_probe_properties(
              "qtimlqnn", {std::string(129U, 'x')}, properties).code_ ==
          status_code::invalid_argument);
    vqec::vision::ai::inference_plan invalid;
    check(graph.vqec_vision_ai_qcom_plgr_configure_graph(invalid).code_ != status_code::ok);
    check(graph.vqec_vision_ai_qcom_plgr_get_state() == plugin_graph_state::empty);
    std::cout << "graph state guard failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
