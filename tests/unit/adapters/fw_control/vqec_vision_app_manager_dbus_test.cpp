#include "vqec_vision_app_manager_dbus.hpp"

#include <gio/gio.h>

#include <chrono>
#include <future>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "vqec_vision_runtime_control_snapshot.hpp"

namespace vqec::vision::ai {
namespace {

constexpr char g_peer_name[] = "com.vqec.AppManagerTestPeer";
constexpr char g_service_name[] = "com.vqec.AppManagerTestService";
constexpr char g_object_path[] = "/com/vqec/AiVision/AppManagerTest";
constexpr int g_rpc_timeout_ms = 5000;
constexpr std::size_t g_callbacks_per_poll = 8;
constexpr std::size_t g_max_poll_iterations = 10000;
constexpr auto g_poll_interval = std::chrono::milliseconds(1);
constexpr guint g_request_name_do_not_queue = 4U;

class fake_app_manager final : public app_manager_port {
public:
    status vqec_vision_ai_ports_apmgr_install(const app_package_candidate&,
        std::uint64_t, runtime_control_snapshot&) override {
        return {status_code::unsupported, "install not exercised by this wire test"};
    }
    status vqec_vision_ai_ports_apmgr_update_configuration(const std::string&,
        std::uint64_t, const std::vector<std::uint8_t>&, const std::string&,
        runtime_control_snapshot&) override {
        return {status_code::unsupported, "configuration not exercised by this wire test"};
    }
    status vqec_vision_ai_ports_apmgr_set_desired(const app_desired_update& _update,
        runtime_control_snapshot& _snapshot) override {
        if (_update.app_id_ != "security.fire_smoke_detection" ||
            _update.source_id_ != "camera_front" ||
            _update.expected_desired_revision_ != 4 || !_update.desired_) {
            return {status_code::invalid_argument, "unexpected desired update"};
        }
        desired_called_ = true;
        vqec_vision_ai_unit_amdtst_snapshot(_snapshot);
        _snapshot.snapshot_revision_ = 5;
        _snapshot.desired_revision_ = 5;
        _snapshot.associations_[0].desired_ = true;
        return {};
    }
    status vqec_vision_ai_ports_apmgr_uninstall(const std::string&,
        std::uint64_t, runtime_control_snapshot&) override {
        return {status_code::unsupported, "uninstall not exercised by this wire test"};
    }
    status vqec_vision_ai_ports_apmgr_get_snapshot(
        runtime_control_snapshot& _snapshot) const override {
        vqec_vision_ai_unit_amdtst_snapshot(_snapshot);
        return {};
    }

    static void vqec_vision_ai_unit_amdtst_snapshot(
        runtime_control_snapshot& _snapshot) {
        _snapshot = {};
        _snapshot.schema_version_ = 1;
        _snapshot.snapshot_revision_ = 4;
        _snapshot.inventory_revision_ = 2;
        _snapshot.entitlement_revision_ = 3;
        _snapshot.desired_revision_ = 4;
        app_runtime_association association;
        association.app_id_ = "security.fire_smoke_detection";
        association.source_id_ = "camera_front";
        association.installed_ = true;
        association.entitled_ = true;
        association.supported_ = true;
        association.compatible_ = true;
        association.admitted_ = true;
        association.configuration_revision_ = 1;
        association.configuration_sha256_ =
            "07e72c1c0bdb8762f3c2771c0d46f5207b8ae9af9a04ef4a7832d2104930e935";
        association.configuration_schema_id_ = "security.fire_smoke.configuration";
        association.configuration_payload_ = {'{', '}'};
        association.output_scopes_ = {"security.fire_smoke.event"};
        association.entitlement_expires_utc_ns_ = 9000000000000000000ULL;
        _snapshot.associations_.push_back(std::move(association));
    }

    bool desired_called_{false};
};

void vqec_vision_ai_unit_amdtst_request_name(GDBusConnection* _connection) {
    GError* error = nullptr;
    GVariant* reply = g_dbus_connection_call_sync(_connection, "org.freedesktop.DBus",
        "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName",
        g_variant_new("(su)", g_peer_name, g_request_name_do_not_queue),
        G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, g_rpc_timeout_ms,
        nullptr, &error);
    if (reply == nullptr) {
        const std::string message = error == nullptr ?
            "cannot own app manager test peer name" : error->message;
        if (error != nullptr) g_error_free(error);
        throw std::runtime_error(message);
    }
    g_variant_unref(reply);
}

std::future<GVariant*> vqec_vision_ai_unit_amdtst_call(
    GDBusConnection* _connection, const char* _method, GVariant* _parameters,
    const GVariantType* _reply_type) {
    if (_parameters != nullptr) g_variant_ref_sink(_parameters);
    return std::async(std::launch::async,
        [_connection, _method, _parameters, _reply_type]() {
            GError* error = nullptr;
            GVariant* reply = g_dbus_connection_call_sync(_connection, g_service_name,
                g_object_path, app_manager_dbus_protocol::g_interface_name, _method,
                _parameters, _reply_type, G_DBUS_CALL_FLAGS_NONE, g_rpc_timeout_ms,
                nullptr, &error);
            if (_parameters != nullptr) g_variant_unref(_parameters);
            if (reply == nullptr) {
                const std::string message = error == nullptr ?
                    "app manager DBus call failed" : error->message;
                if (error != nullptr) g_error_free(error);
                throw std::runtime_error(message);
            }
            return reply;
        });
}

GVariant* vqec_vision_ai_unit_amdtst_wait(app_manager_dbus_server& _server,
    std::future<GVariant*>& _future) {
    for (std::size_t iteration = 0; iteration < g_max_poll_iterations; ++iteration) {
        _server.vqec_vision_ai_fwctl_amdbs_poll();
        if (_future.wait_for(std::chrono::milliseconds(0)) ==
            std::future_status::ready) {
            return _future.get();
        }
        std::this_thread::sleep_for(g_poll_interval);
    }
    throw std::runtime_error("timed out waiting for app manager DBus reply");
}

void vqec_vision_ai_unit_amdtst_check_wire() {
    GTestDBus* bus = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(bus);
    GError* error = nullptr;
    GDBusConnection* peer = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (peer == nullptr) {
        const std::string message = error == nullptr ?
            "cannot open app manager test bus" : error->message;
        if (error != nullptr) g_error_free(error);
        g_test_dbus_down(bus);
        g_object_unref(bus);
        throw std::runtime_error(message);
    }
    vqec_vision_ai_unit_amdtst_request_name(peer);
    {
        fake_app_manager port;
        app_manager_dbus_server server;
        const app_manager_dbus_config config{g_service_name, g_object_path,
            g_peer_name, g_rpc_timeout_ms, g_callbacks_per_poll, true};
        const auto opened = server.vqec_vision_ai_fwctl_amdbs_open(port, config);
        if (opened.code_ != status_code::ok) {
            throw std::runtime_error(opened.message_);
        }

        auto snapshot_future = vqec_vision_ai_unit_amdtst_call(peer,
            app_manager_dbus_protocol::g_snapshot_method, nullptr,
            G_VARIANT_TYPE("(s)"));
        GVariant* snapshot_reply =
            vqec_vision_ai_unit_amdtst_wait(server, snapshot_future);
        const gchar* document = nullptr;
        g_variant_get(snapshot_reply, "(&s)", &document);
        std::istringstream stream(document == nullptr ? "" : document);
        runtime_control_snapshot snapshot;
        if (vqec_vision_ai_lifec_rcsnp_load(stream, snapshot).code_ != status_code::ok ||
            snapshot.snapshot_revision_ != 4) {
            g_variant_unref(snapshot_reply);
            throw std::runtime_error("GetSnapshot wire payload is invalid");
        }
        g_variant_unref(snapshot_reply);

        auto desired_future = vqec_vision_ai_unit_amdtst_call(peer,
            app_manager_dbus_protocol::g_desired_method,
            g_variant_new("(sstb)", "security.fire_smoke_detection", "camera_front",
                static_cast<guint64>(4), TRUE), G_VARIANT_TYPE("(t)"));
        GVariant* desired_reply =
            vqec_vision_ai_unit_amdtst_wait(server, desired_future);
        guint64 revision = 0;
        g_variant_get(desired_reply, "(t)", &revision);
        g_variant_unref(desired_reply);
        if (revision != 5 || !port.desired_called_) {
            throw std::runtime_error("SetDesired wire reply is invalid");
        }
    }
    g_object_unref(peer);
    g_test_dbus_down(bus);
    g_object_unref(bus);
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    vqec::vision::ai::vqec_vision_ai_unit_amdtst_check_wire();
    return 0;
}
