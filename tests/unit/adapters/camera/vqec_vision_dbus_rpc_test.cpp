#include <gio/gio.h>

#include <iostream>

#include "vqec_vision_camera_protocol.hpp"
#include "vqec_vision_dbus_rpc.hpp"

int main() {
    using namespace vqec::vision::ai;

    GTestDBus* bus = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(bus);
    dbus_rpc rpc;
    const auto opened = rpc.vqec_vision_ai_camer_dbrpc_open(true);
    camera_fields response;
    const camera_fields request{
        {camera_protocol::g_camera_id_field, "0"},
        {camera_protocol::g_channel_id_field, "0"},
        {camera_protocol::g_stream_id_field, camera_protocol::g_ai_stream},
        {camera_protocol::g_transport_field, camera_protocol::g_fd_transport},
        {camera_protocol::g_consumer_id_field, "test_runtime"},
        {camera_protocol::g_request_id_field, "test_runtime:start"}};
    const auto called = opened.code_ == status_code::ok ?
        rpc.vqec_vision_ai_camer_cmrpc_call(
            camera_protocol::g_start_stream, request, 1000, response) : opened;
    g_test_dbus_down(bus);
    g_object_unref(bus);
    if (called.code_ != status_code::source_lost) {
        std::cerr << "ownerless Camera D-Bus mapped to "
                  << static_cast<int>(called.code_) << ": " << called.message_ << '\n';
        return 1;
    }
    return 0;
}
