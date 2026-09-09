#include <iostream>
#include <memory>

#include <unistd.h>

#include "vqec_vision_camera_session.hpp"
#include "vqec_vision_inference_graph.hpp"
#include "vqec_vision_source_lifecycle.hpp"

namespace {

using namespace vqec::vision::ai;

class test_rpc final : public camera_rpc {
public:
    status vqec_vision_ai_camer_cmrpc_call(const std::string& _method,
        const camera_fields& _request, int _timeout_ms, camera_fields& _response) override {
        (void)_timeout_ms;
        _response.clear();
        if (_method == "StartStream") {
            ++starts_;
            if (starts_ == 1) {
                first_start_ = _request;
                return {status_code::timeout, "uncertain acquisition"};
            }
            has_same_start_ = first_start_ == _request;
            _response = {{"code", "0"}, {"stream_handle", "fixture-handle"},
                         {"codec", "RAW"}, {"width", "1920"}, {"height", "1080"},
                         {"fps", "25"}};
            return {};
        }
        if (_method == "StopStream") {
            ++stops_;
            if (stops_ == 1) {
                first_stop_ = _request;
                return {status_code::timeout, "uncertain release"};
            }
            has_same_stop_ = first_stop_ == _request;
            _response = {{"code", "0"}};
            return {};
        }
        return {status_code::unsupported, "unexpected fixture RPC"};
    }
    unsigned starts_{0};
    unsigned stops_{0};
    bool has_same_start_{false};
    bool has_same_stop_{false};
    camera_fields first_start_;
    camera_fields first_stop_;
};

camera_session_config vqec_vision_ai_ctest_cstst_make_config() {
    camera_session_config config;
    config.plan_.source_width_ = 1920;
    config.plan_.source_height_ = 1080;
    config.plan_.fps_numerator_ = 25;
    config.plan_.fps_denominator_ = 1;
    config.plan_.tensor_width_ = 640;
    config.plan_.tensor_height_ = 640;
    config.plan_.placement_ = image_placement::centre;
    config.plan_.model_path_ = "/fixture/model.bin";
    config.plan_.backend_path_ = "/fixture/backend.so";
    config.plan_.system_path_ = "/fixture/system.so";
    config.plan_.input_queue_bytes_ = 8U * 1024U * 1024U;
    config.plan_.output_queue_buffers_ = 2;
    config.binding_.width_ = 1920;
    config.binding_.height_ = 1080;
    config.binding_.fps_numerator_ = 25;
    config.binding_.fps_denominator_ = 1;
    config.binding_.memory_kind_ = source_memory_kind::dmabuf;
    config.binding_.layout_ = source_memory_layout::linear_nv12;
    config.binding_.sync_mode_ = source_sync_mode::implicit_ready;
    config.binding_.color_profile_ = source_color_profile::bt709_limited;
    config.binding_.chroma_site_ = source_chroma_site::mpeg2;
    config.binding_.fw_memory_contract_ = "fixture:fw";
    config.binding_.backend_memory_contract_ = "fixture:backend";
    config.binding_.preprocess_contract_ = "fixture:golden";
    config.outputs_ = {{"fixture", {256}}};
    config.max_output_bytes_ = 1024;
    config.cycle_id_ = 1;
    config.stop_timeout_ns_ = 50;
    return config;
}

}  // namespace

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    auto rpc = std::make_shared<test_rpc>();
    {
        auto config = vqec_vision_ai_ctest_cstst_make_config();
        model_outputs manifest;
        manifest.model_id_ = "fixture";
        manifest.model_version_ = "1.0";
        manifest.artifact_sha256_ = std::string(64, 'a');
        manifest.decoder_contract_ = "fixture.raw.v1";
        manifest.outputs_ = {{"scores", {1}}};
        manifest.max_output_bytes_ = 4;
        model_output_selection selection{"fixture", "1.0", std::string(64, 'a'), "wrong"};
        check(vqec_vision_ai_appl_camsn_bind_model_outputs(manifest, selection, config).code_ ==
              status_code::invalid_argument);
        check(config.outputs_[0].name_ == "fixture" && config.max_output_bytes_ == 1024);
        selection.decoder_contract_ = "fixture.raw.v1";
        check(vqec_vision_ai_appl_camsn_bind_model_outputs(manifest, selection, config).code_ ==
              status_code::ok);
        check(config.outputs_[0].name_ == "scores" && config.max_output_bytes_ == 4);
        check(config.plan_.model_path_ == "/fixture/model.bin");
        manifest.outputs_[0].dimensions_ = {0};
        check(vqec_vision_ai_appl_camsn_bind_model_outputs(manifest, selection, config).code_ ==
              status_code::invalid_argument);
        check(config.outputs_[0].dimensions_ == std::vector<std::uint32_t>{1});
    }
    camera_lifecycle_config camera;
    camera.acquire_.consumer_id_ = "fixture-ai";
    camera.acquire_.request_id_ = "fixture-start";
    camera.stop_request_id_ = "fixture-stop";
    camera.media_.socket_path_ = "/tmp/vqec-vision-fixture.sock";
    camera.media_.producer_uid_ = static_cast<std::uint32_t>(::getuid());
    camera.media_.limits_.nv12_format_value_ = 23;  // No packet is decoded by this fixture.
    source_lifecycle source(rpc, camera);
    plugin_graph graph;
    auto domain = std::make_shared<graph_retention>();
    qualcomm_inference_graph graph_port(graph, domain);
    tensor_result result;
    result.pipeline_pts_ns_ = 777;
    camera_pump_report report;
    {
        camera_session invalid(source, graph_port, {});
        check(invalid.vqec_vision_ai_appl_camsn_step(0, result, report).code_ ==
              status_code::invalid_argument);
        check(rpc->starts_ == 0 && rpc->stops_ == 0);
        check(invalid.vqec_vision_ai_appl_camsn_request_stop(0).code_ == status_code::ok);
        check(invalid.vqec_vision_ai_appl_camsn_get_state() == camera_session_state::stopped);
    }
    {
        auto malformed = vqec_vision_ai_ctest_cstst_make_config();
        malformed.outputs_ = {{"duplicate", {1}}, {"duplicate", {1}}};
        camera_session rejected(source, graph_port, malformed);
        check(rejected.vqec_vision_ai_appl_camsn_step(0, result, report).code_ ==
              status_code::invalid_argument);
        check(rejected.vqec_vision_ai_appl_camsn_get_state() == camera_session_state::idle);
        check(source.vqec_vision_ai_camer_srclc_get_state() == camera_source_state::idle);
        check(rpc->starts_ == 0 && rpc->stops_ == 0);
    }
    {
        plugin_graph unsafe_graph;
        qualcomm_inference_graph unsafe_port(unsafe_graph, {});
        camera_session rejected(source, unsafe_port,
            vqec_vision_ai_ctest_cstst_make_config());
        check(rejected.vqec_vision_ai_appl_camsn_step(0, result, report).code_ ==
              status_code::invalid_argument);
        check(source.vqec_vision_ai_camer_srclc_get_state() == camera_source_state::idle);
        check(rpc->starts_ == 0 && rpc->stops_ == 0);
        check(rejected.vqec_vision_ai_appl_camsn_request_stop(0).code_ == status_code::ok);
    }
    camera_session session(source, graph_port, vqec_vision_ai_ctest_cstst_make_config());
    const auto initial = session.vqec_vision_ai_appl_camsn_get_snapshot();
    check(initial.session_state_ == camera_session_state::idle);
    check(initial.source_state_ == raw_source_state::idle);
    check(initial.graph_state_ == inference_graph_state::empty);
    check(initial.graph_jobs_ == 0 && initial.source_readers_ == 0);
    check(initial.first_error_code_ == status_code::ok && !initial.is_recovery_required_);
    check(rpc->starts_ == 0 && rpc->stops_ == 0);
    check(session.vqec_vision_ai_appl_camsn_step(0, result, report).code_ == status_code::pending);
    check(session.vqec_vision_ai_appl_camsn_step(1, result, report).code_ == status_code::pending);
    check(rpc->starts_ == 1 && rpc->stops_ == 0);
    check(session.vqec_vision_ai_appl_camsn_get_last_error().message_ == "uncertain acquisition");
    check(session.vqec_vision_ai_appl_camsn_request_stop(2).code_ == status_code::ok);
    check(session.vqec_vision_ai_appl_camsn_step(3, result, report).code_ == status_code::pending);
    check(session.vqec_vision_ai_appl_camsn_get_state() == camera_session_state::releasing_camera);
    check(rpc->stops_ == 0);
    check(session.vqec_vision_ai_appl_camsn_step(4, result, report).code_ == status_code::pending);
    check(rpc->starts_ == 2 && rpc->has_same_start_ && rpc->stops_ == 0);
    check(session.vqec_vision_ai_appl_camsn_step(52, result, report).code_ == status_code::timeout);
    check(session.vqec_vision_ai_appl_camsn_is_recovery_required());
    const auto expired = session.vqec_vision_ai_appl_camsn_get_snapshot();
    check(expired.session_state_ == camera_session_state::releasing_camera);
    check(expired.source_state_ == raw_source_state::draining);
    check(expired.graph_jobs_ == 0 && expired.source_readers_ == 0);
    check(expired.is_recovery_required_ && expired.first_error_code_ == status_code::timeout);
    check(rpc->starts_ == 2 && rpc->stops_ == 1);  // Snapshot does not retry RPC.
    check(session.vqec_vision_ai_appl_camsn_request_stop(53).code_ == status_code::ok);
    check(session.vqec_vision_ai_appl_camsn_is_recovery_required());
    check(session.vqec_vision_ai_appl_camsn_step(52, result, report).code_ ==
          status_code::invalid_argument);
    check(rpc->stops_ == 1);
    check(session.vqec_vision_ai_appl_camsn_step(54, result, report).code_ == status_code::ok);
    check(session.vqec_vision_ai_appl_camsn_get_state() == camera_session_state::stopped);
    check(source.vqec_vision_ai_camer_srclc_get_state() == camera_source_state::stopped);
    check(!session.vqec_vision_ai_appl_camsn_is_recovery_required());
    check(rpc->stops_ == 2 && rpc->has_same_stop_);
    const auto complete = session.vqec_vision_ai_appl_camsn_get_snapshot();
    check(complete.session_state_ == camera_session_state::stopped);
    check(complete.source_state_ == raw_source_state::stopped);
    check(!complete.is_recovery_required_ && complete.first_error_code_ == status_code::timeout);
    check(session.vqec_vision_ai_appl_camsn_get_last_error().message_ == "uncertain acquisition");
    check(session.vqec_vision_ai_appl_camsn_get_last_error().code_ == status_code::timeout);
    check(graph.vqec_vision_ai_qcom_plgr_get_state() == plugin_graph_state::empty);
    check(result.pipeline_pts_ns_ == 777 && !report.has_result_ && !report.has_submission_);
    check(session.vqec_vision_ai_appl_camsn_step(55, result, report).code_ == status_code::ok);
    check(rpc->starts_ == 2 && rpc->stops_ == 2);
    {
        auto startup_rpc = std::make_shared<test_rpc>();
        source_lifecycle startup_source(startup_rpc, camera);
        plugin_graph startup_graph;
        qualcomm_inference_graph startup_graph_port(startup_graph, domain);
        auto config = vqec_vision_ai_ctest_cstst_make_config();
        config.startup_timeout_ns_ = 10;
        camera_session startup(startup_source, startup_graph_port, config);
        check(startup.vqec_vision_ai_appl_camsn_step(0, result, report).code_ ==
              status_code::pending);
        check(startup.vqec_vision_ai_appl_camsn_step(1, result, report).code_ ==
              status_code::pending);
        check(startup.vqec_vision_ai_appl_camsn_step(10, result, report).code_ ==
              status_code::timeout);
        check(startup.vqec_vision_ai_appl_camsn_get_last_error().message_ ==
              "uncertain acquisition");
        check(startup.vqec_vision_ai_appl_camsn_get_state() ==
              camera_session_state::draining_graph);
        check(startup_rpc->starts_ == 1 && startup_rpc->stops_ == 0);
        check(startup.vqec_vision_ai_appl_camsn_step(11, result, report).code_ ==
              status_code::pending);
        check(startup.vqec_vision_ai_appl_camsn_step(12, result, report).code_ ==
              status_code::pending);
        check(startup.vqec_vision_ai_appl_camsn_step(13, result, report).code_ ==
              status_code::timeout);
        check(startup.vqec_vision_ai_appl_camsn_step(14, result, report).code_ == status_code::ok);
        check(startup.vqec_vision_ai_appl_camsn_get_state() == camera_session_state::stopped);
        check(startup_rpc->has_same_start_ && startup_rpc->has_same_stop_);
    }
    std::cout << "camera session failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
