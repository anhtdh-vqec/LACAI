#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include "vqec_vision_camera_control.hpp"
#include "vqec_vision_camera_protocol.hpp"

// Independent release expectations: changing production constants must not rewrite these.
namespace protocol = vqec::vision::ai::camera_protocol;
static_assert(std::string_view(protocol::g_bus_name) == "com.vnpt.camera.Camera");
static_assert(std::string_view(protocol::g_object_path) == "/com/vnpt/camera/Camera");
static_assert(std::string_view(protocol::g_interface_name) == "com.vnpt.camera.Camera1");
static_assert(std::string_view(protocol::g_start_stream) == "StartStream");
static_assert(std::string_view(protocol::g_stop_stream) == "StopStream");
static_assert(std::string_view(protocol::g_get_status) == "GetStatus");
static_assert(std::string_view(protocol::g_stream_handle_field) == "stream_handle");
static_assert(std::string_view(protocol::g_ai_stream) == "third");
static_assert(std::string_view(protocol::g_fd_transport) == "dmabuf");
static_assert(protocol::g_code_ok == 0 && protocol::g_code_not_found == 1002);
static_assert(protocol::g_code_not_running == 1005 && protocol::g_code_permission_denied == 1009);

namespace {

class scripted_rpc final : public vqec::vision::ai::camera_rpc {
public:
    vqec::vision::ai::status transport_;
    vqec::vision::ai::camera_fields response_;
    vqec::vision::ai::camera_fields request_;
    std::string method_;
    unsigned calls_{0};

    vqec::vision::ai::status vqec_vision_ai_camer_cmrpc_call(
        const std::string& _method, const vqec::vision::ai::camera_fields& _request,
        int _timeout_ms, vqec::vision::ai::camera_fields& _response) override {
        if (_timeout_ms < 1) {
            return {vqec::vision::ai::status_code::invalid_argument, "invalid test timeout"};
        }
        ++calls_;
        method_ = _method;
        request_ = _request;
        _response = response_;
        return transport_;
    }
};

}  // namespace

int main() {
    using vqec::vision::ai::camera_acquire_request;
    using vqec::vision::ai::camera_control;
    using vqec::vision::ai::camera_lease_state;
    using vqec::vision::ai::status_code;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    auto rpc = std::make_shared<scripted_rpc>();
    camera_control control(rpc);
    camera_acquire_request request{0, "ai_app:instance_1:cam0", "instance_1:start_1"};
    rpc->transport_ = {status_code::timeout, "injected lost response"};
    check(control.vqec_vision_ai_camer_cctrl_start(request, 10).code_ == status_code::timeout);
    check(control.vqec_vision_ai_camer_cctrl_get_state() == camera_lease_state::start_pending);
    const auto original = rpc->request_;
    check(original.at("transport") == "dmabuf" && original.at("stream_id") == "third" &&
          original.at("channel_id") == "0" && original.at("camera_id") == "0");
    auto changed = request;
    changed.consumer_id_ = "another_instance";
    check(control.vqec_vision_ai_camer_cctrl_start(changed, 10).code_ ==
          status_code::invalid_state);
    check(control.vqec_vision_ai_camer_cctrl_stop("stop_1", 10).code_ ==
          status_code::invalid_state);
    rpc->transport_ = {};
    rpc->response_ = {{"code", "2004"}};
    check(control.vqec_vision_ai_camer_cctrl_start(request, 10).code_ ==
          status_code::resource_exhausted);
    check(control.vqec_vision_ai_camer_cctrl_get_state() == camera_lease_state::start_pending);
    check(rpc->request_ == original);
    rpc->response_ = {{"code", "9001"}};
    check(control.vqec_vision_ai_camer_cctrl_start(request, 10).code_ == status_code::protocol_error);
    check(control.vqec_vision_ai_camer_cctrl_get_state() == camera_lease_state::start_pending);
    check(rpc->request_ == original);
    rpc->response_ = {{"code", "0"}, {"stream_handle", "opaque_lease"}, {"codec", "RAW"},
                      {"width", "1920"}, {"height", "1080"}, {"fps", "25"}};
    check(control.vqec_vision_ai_camer_cctrl_start(request, 10).code_ == status_code::ok);
    check(rpc->request_ == original && rpc->method_ == "StartStream");
    check(control.vqec_vision_ai_camer_cctrl_get_profile().width_ == 1920 &&
          control.vqec_vision_ai_camer_cctrl_get_profile().is_valid_);
    const auto calls = rpc->calls_;
    check(control.vqec_vision_ai_camer_cctrl_start(request, 10).code_ == status_code::ok);
    check(rpc->calls_ == calls);
    check(control.vqec_vision_ai_camer_cctrl_stop(request.request_id_, 10).code_ ==
          status_code::invalid_argument);
    rpc->transport_ = {status_code::timeout, "stop response lost"};
    check(control.vqec_vision_ai_camer_cctrl_stop("stop_1", 10).code_ == status_code::timeout);
    check(control.vqec_vision_ai_camer_cctrl_get_state() == camera_lease_state::stop_pending);
    check(control.vqec_vision_ai_camer_cctrl_get_handle() == "opaque_lease");
    const auto stop_request = rpc->request_;
    check(stop_request.at("consumer_id") == request.consumer_id_ &&
          stop_request.at("stream_handle") == "opaque_lease" && rpc->method_ == "StopStream");
    check(control.vqec_vision_ai_camer_cctrl_start(request, 10).code_ ==
          status_code::invalid_state);
    check(control.vqec_vision_ai_camer_cctrl_stop("different_stop", 10).code_ ==
          status_code::invalid_argument);
    rpc->transport_ = {};
    rpc->response_ = {{"code", "1002"}};
    check(control.vqec_vision_ai_camer_cctrl_stop("stop_1", 10).code_ == status_code::ok);
    check(rpc->request_ == stop_request);
    check(control.vqec_vision_ai_camer_cctrl_get_state() == camera_lease_state::idle &&
          control.vqec_vision_ai_camer_cctrl_get_handle().empty());

    // An acquired handle survives malformed profile data for explicit cleanup.
    for (const std::string width : {"1920junk", "-1", "4294967296", "0", "1919"}) {
        request.request_id_ = "profile_test:" + width;
        const auto stop_id = "stop_profile:" + width;
        rpc->response_ = {{"code", "0"}, {"stream_handle", "cleanup_handle"}, {"codec", "RAW"},
                          {"width", width}, {"height", "1080"}, {"fps", "25"}};
        check(control.vqec_vision_ai_camer_cctrl_start(request, 10).code_ ==
              status_code::protocol_error);
        check(control.vqec_vision_ai_camer_cctrl_get_state() == camera_lease_state::acquired &&
              control.vqec_vision_ai_camer_cctrl_get_handle() == "cleanup_handle" &&
              !control.vqec_vision_ai_camer_cctrl_get_profile().is_valid_);
        rpc->response_ = {{"code", "1009"}};
        check(control.vqec_vision_ai_camer_cctrl_stop(stop_id, 10).code_ ==
              status_code::unauthorized);
        rpc->response_ = {{"code", "0"}};
        check(control.vqec_vision_ai_camer_cctrl_stop(stop_id, 10).code_ == status_code::ok);
    }
    request.request_id_ = "4k_test:start";
    rpc->response_ = {{"code", "0"}, {"stream_handle", "4k_handle"}, {"codec", "RAW"},
                      {"width", "3840"}, {"height", "2160"}, {"fps", "25"}};
    check(control.vqec_vision_ai_camer_cctrl_start(request, 10).code_ == status_code::ok);
    check(control.vqec_vision_ai_camer_cctrl_get_profile().width_ == 3840);
    rpc->response_ = {{"code", "0"}};
    check(control.vqec_vision_ai_camer_cctrl_stop("4k_test:stop", 10).code_ == status_code::ok);
    request.request_id_ = "invalid_code:start";
    rpc->response_ = {{"code", "0junk"}};
    check(control.vqec_vision_ai_camer_cctrl_start(request, 10).code_ ==
          status_code::protocol_error);
    check(control.vqec_vision_ai_camer_cctrl_get_state() == camera_lease_state::start_pending);
    std::cout << "camera control failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
