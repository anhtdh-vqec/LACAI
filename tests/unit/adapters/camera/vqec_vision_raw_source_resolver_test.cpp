#include <cstdint>
#include <iostream>
#include <string>

#include "vqec_vision_raw_source_resolver.hpp"

namespace {

vqec::vision::ai::source_deployment_config
vqec_vision_ai_unit_rsrst_make_source(std::uint32_t _camera_id) {
    vqec::vision::ai::source_deployment_config source;
    source.source_id_ = "source_" + std::to_string(_camera_id);
    source.raw_source_ref_ = "fw_raw_" + std::to_string(_camera_id);
    source.camera_id_ = _camera_id;
    source.channel_id_ = 0;
    source.profile_.width_ = 1920;
    source.profile_.height_ = 1080;
    source.profile_.fps_numerator_ = 25;
    source.profile_.fps_denominator_ = 1;
    source.memory_.max_frame_allocation_bytes_ = 4U * 1024U * 1024U;
    source.memory_.max_inflight_frames_ = 2;
    source.memory_.max_tensor_bytes_ = 1024;
    source.model_ids_ = {"detector"};
    return source;
}

}  // namespace

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    const auto source_0 = vqec_vision_ai_unit_rsrst_make_source(0);
    const auto source_3 = vqec_vision_ai_unit_rsrst_make_source(3);
    raw_source_route route_0;
    raw_source_route route_3;
    check(vqec_vision_ai_camer_rsrsv_make_legacy_route(
              source_0, "/tmp/camera_ai", 1000, 23, route_0).code_ == status_code::ok);
    check(route_0.media_socket_path_ == "/tmp/camera_ai/0_third_ai.sock");
    check(vqec_vision_ai_camer_rsrsv_make_legacy_route(
              source_3, "/tmp/camera_ai", 1000, 23, route_3).code_ == status_code::ok);
    check(route_3.media_socket_path_ == "/tmp/camera_ai/0_third_ai_cam3.sock");

    static_raw_source_resolver resolver;
    check(resolver.vqec_vision_ai_camer_rsrsv_add_route(route_0).code_ == status_code::ok);
    check(resolver.vqec_vision_ai_camer_rsrsv_add_route(route_3).code_ == status_code::ok);
    check(resolver.vqec_vision_ai_camer_rsrsv_get_count() == 2);
    check(resolver.vqec_vision_ai_camer_rsrsv_add_route(route_0).code_ ==
          status_code::invalid_argument);

    raw_source_route resolved;
    check(resolver.vqec_vision_ai_camer_rsrsv_resolve_source(
              "fw_raw_3", resolved).code_ == status_code::ok);
    check(resolved.camera_id_ == 3 && resolved.channel_id_ == 0 &&
          resolved.media_socket_path_ == route_3.media_socket_path_);
    const auto preserved = resolved.media_socket_path_;
    check(resolver.vqec_vision_ai_camer_rsrsv_resolve_source(
              "missing", resolved).code_ == status_code::source_lost);
    check(resolved.media_socket_path_ == preserved);

    raw_source_cycle_identity identity{
        "ai_app:instance_1", "instance_1:start_3", "instance_1:stop_3"};
    camera_lifecycle_config lifecycle;
    check(vqec_vision_ai_camer_rsrsv_compose_lifecycle(
              source_3, route_3, identity, lifecycle).code_ == status_code::ok);
    check(lifecycle.acquire_.camera_id_ == 3 && lifecycle.acquire_.channel_id_ == 0);
    check(lifecycle.media_.socket_path_ == route_3.media_socket_path_ &&
          lifecycle.media_.producer_uid_ == 1000 && lifecycle.max_fps_ == 25);
    check(lifecycle.media_.limits_.max_width_ == 1920 &&
          lifecycle.media_.limits_.max_height_ == 1080 &&
          lifecycle.media_.limits_.max_allocation_bytes_ == 4U * 1024U * 1024U);

    auto fractional = source_3;
    fractional.profile_.fps_numerator_ = 30000;
    fractional.profile_.fps_denominator_ = 1001;
    const auto retained_path = lifecycle.media_.socket_path_;
    check(vqec_vision_ai_camer_rsrsv_compose_lifecycle(
              fractional, route_3, identity, lifecycle).code_ ==
          status_code::invalid_argument);
    check(lifecycle.media_.socket_path_ == retained_path);

    auto nonzero_channel = source_3;
    nonzero_channel.channel_id_ = 1;
    raw_source_route rejected;
    check(vqec_vision_ai_camer_rsrsv_make_legacy_route(
              nonzero_channel, "/tmp/camera_ai", 1000, 23, rejected).code_ ==
          status_code::unsupported);

    std::cout << "RAW source resolver failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
