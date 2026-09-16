#include "vqec_vision_usecase_control_dbus.hpp"

#include <gio/gio.h>

#include <chrono>
#include <cstdint>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>

namespace vqec::vision::ai {
namespace {

constexpr char g_peer_name[] = "com.vqec.UsecaseControlTestPeer";
constexpr char g_service_name[] = "com.vqec.UsecaseControlTestService";
constexpr char g_object_path[] = "/com/vqec/AiVision/UsecaseControlTest";
constexpr int g_rpc_timeout_ms = 5000;
constexpr std::size_t g_callbacks_per_poll = 8;
constexpr std::size_t g_max_poll_iterations = 10000;
constexpr auto g_poll_interval = std::chrono::milliseconds(1);
constexpr guint g_request_name_do_not_queue = 4U;

class fake_usecase_control final : public usecase_control_port {
public:
    status vqec_vision_ai_ports_ucctl_apply_desired_plan(
        const usecase_desired_plan& _plan, usecase_apply_receipt& _receipt) override {
        if (_plan.request_id_ != "dbus_request" ||
            _plan.expected_control_revision_ != 7 || _plan.entries_.size() != 1 ||
            _plan.entries_[0].source_id_ != "camera_front" ||
            _plan.entries_[0].usecase_id_ != "face_recognition" ||
            !_plan.entries_[0].desired_enabled_) {
            return {status_code::invalid_argument, "unexpected desired plan"};
        }
        apply_called_ = true;
        _receipt = {true, 8, usecase_apply_state::reconciling, status_code::ok};
        return {};
    }

    status vqec_vision_ai_ports_ucctl_get_status(
        usecase_control_status& _status) const override {
        _status.control_revision_ = apply_called_ ? 8 : 7;
        _status.entitlement_revision_ = 3;
        _status.runtime_generation_ = 5;
        _status.entries_ = {{"camera_front", "face_recognition", true, true, true,
            true, true, true, true, false, usecase_runtime_state::loading,
            "runtime_reconciling"}};
        return {};
    }

    status vqec_vision_ai_ports_ucctl_get_capabilities(
        usecase_capability_snapshot& _capabilities) const override {
        _capabilities.catalog_revision_ = 11;
        _capabilities.usecases_ = {{"person_detection", "1.0"},
            {"face_recognition", "1.0"}};
        return {};
    }

    bool apply_called_{false};
};

GVariant* vqec_vision_ai_unit_ucdtst_wait_call(
    usecase_control_dbus_server& _server, std::future<GVariant*>& _future) {
    for (std::size_t iteration = 0; iteration < g_max_poll_iterations; ++iteration) {
        _server.vqec_vision_ai_fwctl_ucdbs_poll();
        if (_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            return _future.get();
        }
        std::this_thread::sleep_for(g_poll_interval);
    }
    throw std::runtime_error("timed out waiting for usecase DBus reply");
}

std::future<GVariant*> vqec_vision_ai_unit_ucdtst_call(
    GDBusConnection* _connection, const char* _method, GVariant* _parameters,
    const GVariantType* _reply_type) {
    if (_parameters != nullptr) {
        g_variant_ref_sink(_parameters);
    }
    return std::async(std::launch::async,
        [_connection, _method, _parameters, _reply_type]() {
            GError* error = nullptr;
            GVariant* reply = g_dbus_connection_call_sync(_connection, g_service_name,
                g_object_path, usecase_control_dbus_protocol::g_interface_name, _method,
                _parameters, _reply_type, G_DBUS_CALL_FLAGS_NONE, g_rpc_timeout_ms,
                nullptr, &error);
            if (_parameters != nullptr) {
                g_variant_unref(_parameters);
            }
            if (reply == nullptr) {
                const std::string message = error == nullptr ?
                    "usecase DBus call failed" : error->message;
                if (error != nullptr) {
                    g_error_free(error);
                }
                throw std::runtime_error(message);
            }
            return reply;
        });
}

void vqec_vision_ai_unit_ucdtst_request_name(GDBusConnection* _connection) {
    GError* error = nullptr;
    GVariant* reply = g_dbus_connection_call_sync(_connection, "org.freedesktop.DBus",
        "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName",
        g_variant_new("(su)", g_peer_name, g_request_name_do_not_queue),
        G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, g_rpc_timeout_ms, nullptr, &error);
    if (reply == nullptr) {
        const std::string message = error == nullptr ? "cannot own peer name" : error->message;
        if (error != nullptr) {
            g_error_free(error);
        }
        throw std::runtime_error(message);
    }
    g_variant_unref(reply);
}

void vqec_vision_ai_unit_ucdtst_check_wire_contract() {
    GTestDBus* bus = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(bus);
    GError* error = nullptr;
    GDBusConnection* peer = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (peer == nullptr) {
        const std::string message = error == nullptr ? "cannot open test bus" : error->message;
        if (error != nullptr) {
            g_error_free(error);
        }
        g_test_dbus_down(bus);
        g_object_unref(bus);
        throw std::runtime_error(message);
    }
    vqec_vision_ai_unit_ucdtst_request_name(peer);
    {
        fake_usecase_control port;
        usecase_control_dbus_server server;
        const usecase_control_dbus_config config{g_service_name, g_object_path,
            g_peer_name, g_rpc_timeout_ms, g_callbacks_per_poll, true};
        const auto opened = server.vqec_vision_ai_fwctl_ucdbs_open(port, config);
        if (opened.code_ != status_code::ok) {
            throw std::runtime_error("cannot open usecase control DBus server");
        }

        GVariantBuilder desired;
        g_variant_builder_init(&desired, G_VARIANT_TYPE("a(ssb)"));
        g_variant_builder_add(&desired, "(ssb)", "camera_front", "face_recognition", TRUE);
        auto apply_future = vqec_vision_ai_unit_ucdtst_call(peer,
            usecase_control_dbus_protocol::g_apply_method,
            g_variant_new("(st@a(ssb))", "dbus_request", static_cast<guint64>(7),
                g_variant_builder_end(&desired)),
            G_VARIANT_TYPE("(btsu)"));
        GVariant* apply_reply = vqec_vision_ai_unit_ucdtst_wait_call(server, apply_future);
        gboolean accepted = FALSE;
        guint64 revision = 0;
        const gchar* state = nullptr;
        guint reason = 0;
        g_variant_get(apply_reply, "(bt&su)", &accepted, &revision, &state, &reason);
        if (accepted == FALSE || revision != 8 || g_strcmp0(state, "reconciling") != 0 ||
            reason != static_cast<guint>(status_code::ok) || !port.apply_called_) {
            g_variant_unref(apply_reply);
            throw std::runtime_error("ApplyDesiredPlan wire reply is incorrect");
        }
        g_variant_unref(apply_reply);

        auto status_future = vqec_vision_ai_unit_ucdtst_call(peer,
            usecase_control_dbus_protocol::g_status_method, nullptr,
            G_VARIANT_TYPE("(ttta(ssbbbbbbbbss))"));
        GVariant* status_reply = vqec_vision_ai_unit_ucdtst_wait_call(server, status_future);
        if (g_variant_n_children(status_reply) != 4) {
            g_variant_unref(status_reply);
            throw std::runtime_error("GetUsecaseStatus wire reply is incorrect");
        }
        g_variant_unref(status_reply);

        auto capability_future = vqec_vision_ai_unit_ucdtst_call(peer,
            usecase_control_dbus_protocol::g_capabilities_method, nullptr,
            G_VARIANT_TYPE("(ta(ss))"));
        GVariant* capability_reply =
            vqec_vision_ai_unit_ucdtst_wait_call(server, capability_future);
        guint64 catalog_revision = 0;
        GVariant* capabilities = nullptr;
        g_variant_get(capability_reply, "(t@a(ss))", &catalog_revision, &capabilities);
        if (catalog_revision != 11 || g_variant_n_children(capabilities) != 2) {
            g_variant_unref(capabilities);
            g_variant_unref(capability_reply);
            throw std::runtime_error("GetCapabilities wire reply is incorrect");
        }
        g_variant_unref(capabilities);
        g_variant_unref(capability_reply);
    }
    g_object_unref(peer);
    g_test_dbus_down(bus);
    g_object_unref(bus);
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    vqec::vision::ai::vqec_vision_ai_unit_ucdtst_check_wire_contract();
    return 0;
}
