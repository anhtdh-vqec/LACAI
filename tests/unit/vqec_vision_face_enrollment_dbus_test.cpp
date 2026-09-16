#include "vqec_vision_face_enrollment_dbus.hpp"

#include <gio/gio.h>

#include <chrono>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>

namespace vqec::vision::ai {
namespace {

constexpr char g_peer_name[] = "com.vqec.FaceEnrollmentTestPeer";
constexpr int g_rpc_timeout_ms = 5000;
constexpr std::size_t g_callbacks_per_poll = 8;
constexpr std::size_t g_max_poll_iterations = 10000;
constexpr auto g_poll_interval = std::chrono::milliseconds(1);
constexpr guint g_request_name_do_not_queue = 4U;

class fake_face_enrollment final : public face_enrollment_port {
public:
    status vqec_vision_ai_ports_fenrl_begin(
        const face_enrollment_begin_request&, face_enrollment_status&) override {
        return {status_code::unsupported, "not used by gallery-status test"};
    }
    status vqec_vision_ai_ports_fenrl_cancel(
        const std::string&, face_enrollment_status&) override {
        return {status_code::unsupported, "not used by gallery-status test"};
    }
    status vqec_vision_ai_ports_fenrl_remove_subject(
        const std::string&, std::uint64_t, std::uint64_t&) override {
        return {status_code::unsupported, "not used by gallery-status test"};
    }
    status vqec_vision_ai_ports_fenrl_get_status(
        const std::string&, face_enrollment_status&) const override {
        return {status_code::unsupported, "not used by gallery-status test"};
    }
    status vqec_vision_ai_ports_fenrl_get_gallery_status(
        face_gallery_status& _status) const override {
        _status = {17, 2, 5, true, false};
        return {};
    }
    status vqec_vision_ai_ports_fenrl_fail(
        const std::string&, status_code, face_enrollment_status&) override {
        return {status_code::unsupported, "not used by gallery-status test"};
    }
    status vqec_vision_ai_ports_fenrl_accept_embedding(
        const embedding_result&, face_enrollment_status&) override {
        return {status_code::unsupported, "not used by gallery-status test"};
    }
    status vqec_vision_ai_ports_fenrl_accept_batch(
        const std::string&, const std::vector<embedding_result>&, std::size_t,
        face_enrollment_status&) override {
        return {status_code::unsupported, "not used by gallery-status test"};
    }
};

void vqec_vision_ai_unit_fdbst_request_name(GDBusConnection* _connection) {
    GError* error = nullptr;
    GVariant* reply = g_dbus_connection_call_sync(_connection, "org.freedesktop.DBus",
        "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName",
        g_variant_new("(su)", g_peer_name, g_request_name_do_not_queue),
        G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, g_rpc_timeout_ms, nullptr, &error);
    if (reply == nullptr) {
        const std::string message = error == nullptr ?
            "cannot own enrollment test peer name" : error->message;
        if (error != nullptr) g_error_free(error);
        throw std::runtime_error(message);
    }
    g_variant_unref(reply);
}

GVariant* vqec_vision_ai_unit_fdbst_wait_call(
    face_enrollment_dbus_server& _server, std::future<GVariant*>& _future) {
    for (std::size_t iteration = 0; iteration < g_max_poll_iterations; ++iteration) {
        _server.vqec_vision_ai_fwctl_fedbs_poll();
        if (_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            return _future.get();
        }
        std::this_thread::sleep_for(g_poll_interval);
    }
    throw std::runtime_error("timed out waiting for enrollment DBus reply");
}

void vqec_vision_ai_unit_fdbst_check_gallery_status() {
    GTestDBus* bus = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(bus);
    GError* error = nullptr;
    GDBusConnection* peer = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (peer == nullptr) {
        const std::string message = error == nullptr ? "cannot open test bus" : error->message;
        if (error != nullptr) g_error_free(error);
        g_test_dbus_down(bus);
        g_object_unref(bus);
        throw std::runtime_error(message);
    }
    vqec_vision_ai_unit_fdbst_request_name(peer);
    {
        fake_face_enrollment port;
        face_enrollment_dbus_server server;
        const face_enrollment_dbus_config config{
            g_peer_name, g_rpc_timeout_ms, g_callbacks_per_poll, true};
        const auto opened = server.vqec_vision_ai_fwctl_fedbs_open(port, config);
        if (opened.code_ != status_code::ok) {
            throw std::runtime_error("cannot open enrollment DBus server");
        }
        auto future = std::async(std::launch::async, [_peer = peer]() {
            GError* call_error = nullptr;
            GVariant* reply = g_dbus_connection_call_sync(_peer,
                face_enrollment_dbus_protocol::g_bus_name,
                face_enrollment_dbus_protocol::g_object_path,
                face_enrollment_dbus_protocol::g_interface_name,
                face_enrollment_dbus_protocol::g_gallery_status_method, nullptr,
                G_VARIANT_TYPE("(tuubb)"), G_DBUS_CALL_FLAGS_NONE,
                g_rpc_timeout_ms, nullptr, &call_error);
            if (reply == nullptr) {
                const std::string message = call_error == nullptr ?
                    "gallery status call failed" : call_error->message;
                if (call_error != nullptr) g_error_free(call_error);
                throw std::runtime_error(message);
            }
            return reply;
        });
        GVariant* reply = vqec_vision_ai_unit_fdbst_wait_call(server, future);
        guint64 revision = 0;
        guint subjects = 0;
        guint templates = 0;
        gboolean available = FALSE;
        gboolean faulted = TRUE;
        g_variant_get(reply, "(tuubb)", &revision, &subjects, &templates,
            &available, &faulted);
        g_variant_unref(reply);
        if (revision != 17 || subjects != 2 || templates != 5 ||
            available == FALSE || faulted != FALSE) {
            throw std::runtime_error("GetGalleryStatus wire reply is incorrect");
        }
    }
    g_object_unref(peer);
    g_test_dbus_down(bus);
    g_object_unref(bus);
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    vqec::vision::ai::vqec_vision_ai_unit_fdbst_check_gallery_status();
    return 0;
}
