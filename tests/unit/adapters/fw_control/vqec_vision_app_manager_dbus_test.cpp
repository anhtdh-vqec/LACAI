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

#include <fcntl.h>
#include <unistd.h>

#include "vqec_vision_runtime_control_snapshot.hpp"

namespace vqec::vision::ai {
namespace {

constexpr char g_backend_name[] = "com.vqec.AppManagerTestBackend";
constexpr char g_runtime_name[] = "com.vqec.AppManagerTestRuntime";
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
    status vqec_vision_ai_ports_apmgr_update(const app_package_candidate& _candidate,
        std::uint64_t _expected_revision,
        runtime_control_snapshot& _snapshot) override {
        if (_expected_revision != 2 ||
            _candidate.manifest_payload_ != std::vector<std::uint8_t>{'{', '}'} ||
            _candidate.configuration_payload_ !=
                std::vector<std::uint8_t>{'{', '}'} ||
            _candidate.signature_payload_ != std::vector<std::uint8_t>{'s'} ||
            _candidate.components_.size() != 1 ||
            _candidate.components_[0].descriptor_ < 0) {
            return {status_code::invalid_argument, "unexpected update candidate"};
        }
        update_called_ = true;
        vqec_vision_ai_unit_amdtst_snapshot(_snapshot);
        _snapshot.snapshot_revision_ = 5;
        return {};
    }
    status vqec_vision_ai_ports_apmgr_rollback(const std::string& _app_id,
        std::uint64_t _expected_revision,
        runtime_control_snapshot& _snapshot) override {
        if (_app_id != "security.fire_smoke_detection" || _expected_revision != 3) {
            return {status_code::invalid_argument, "unexpected rollback request"};
        }
        rollback_called_ = true;
        vqec_vision_ai_unit_amdtst_snapshot(_snapshot);
        _snapshot.snapshot_revision_ = 5;
        return {};
    }
    status vqec_vision_ai_ports_apmgr_update_configuration(const std::string& _app_id,
        std::uint64_t _expected_revision,
        const std::vector<std::uint8_t>& _configuration,
        const std::string& _configuration_sha256,
        runtime_control_snapshot& _snapshot) override {
        if (_app_id != "security.fire_smoke_detection" || _expected_revision != 1 ||
            _configuration != std::vector<std::uint8_t>{'{', '}'} ||
            _configuration_sha256 !=
                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb") {
            return {status_code::invalid_argument, "unexpected configuration update"};
        }
        configuration_called_ = true;
        vqec_vision_ai_unit_amdtst_snapshot(_snapshot);
        _snapshot.snapshot_revision_ = 5;
        _snapshot.associations_[0].configuration_revision_ = 2;
        _snapshot.associations_[0].configuration_sha256_ = _configuration_sha256;
        return {};
    }
    status vqec_vision_ai_ports_apmgr_apply_entitlement(
        const app_entitlement_candidate& _candidate,
        runtime_control_snapshot& _snapshot) override {
        if (_candidate.grant_payload_ != std::vector<std::uint8_t>{'{', '}'} ||
            _candidate.signature_payload_ != std::vector<std::uint8_t>{'s'} ||
            _candidate.grant_sha256_ !=
                "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") {
            return {status_code::invalid_argument, "unexpected entitlement candidate"};
        }
        entitlement_called_ = true;
        vqec_vision_ai_unit_amdtst_snapshot(_snapshot);
        _snapshot.snapshot_revision_ = 5;
        return {};
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
    status vqec_vision_ai_ports_apmgr_list_applications(
        const std::string& _source_id,
        std::vector<app_catalog_status>& _applications) const override {
        if (_source_id != "camera_front") {
            return {status_code::invalid_argument, "unexpected catalog source"};
        }
        _applications = {
            {{"S01", "security.restricted_area_smoking",
                 "Restricted Area Smoking", "1.0.0", true},
                _source_id, false, false, false, false, false,
                app_install_state::not_installed, "unsupported", 4},
            {{"S04", "security.fire_smoke_detection",
                 "Fire and Smoke Detection", "1.0.0", true},
                _source_id, true, true, true, false, false,
                app_install_state::installed_disabled, "installed_disabled", 4}};
        return {};
    }
    status vqec_vision_ai_ports_apmgr_submit_install(
        const app_operation_request&, const app_package_candidate&,
        std::uint64_t, app_operation_record&) override {
        return {status_code::unsupported, "submit install not exercised"};
    }
    status vqec_vision_ai_ports_apmgr_submit_update(
        const app_operation_request& _request,
        const app_package_candidate& _candidate,
        std::uint64_t _expected_revision,
        app_operation_record& _operation) override {
        runtime_control_snapshot snapshot;
        const auto updated = vqec_vision_ai_ports_apmgr_update(
            _candidate, _expected_revision, snapshot);
        if (updated.code_ != status_code::ok ||
            _request.idempotency_key_ != "update_wire_001" ||
            _request.payload_sha256_ !=
                "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc" ||
            _request.app_id_ != "security.fire_smoke_detection" ||
            _request.kind_ != app_operation_kind::update) {
            return {status_code::invalid_argument,
                "unexpected update operation submission"};
        }
        _operation = {_request.idempotency_key_, _request.idempotency_key_,
            _request.payload_sha256_, _request.app_id_, _request.kind_,
            app_operation_state::committed, status_code::ok, {},
            snapshot.snapshot_revision_};
        operation_ = _operation;
        submit_update_called_ = true;
        return {};
    }
    status vqec_vision_ai_ports_apmgr_submit_rollback(
        const app_operation_request&, std::uint64_t,
        app_operation_record&) override {
        return {status_code::unsupported, "submit rollback not exercised"};
    }
    status vqec_vision_ai_ports_apmgr_get_operation(
        const std::string& _operation_id,
        app_operation_record& _operation) const override {
        if (_operation_id != operation_.operation_id_) {
            return {status_code::source_lost, "operation is unavailable"};
        }
        _operation = operation_;
        return {};
    }
    status vqec_vision_ai_ports_apmgr_cancel_operation(
        const std::string& _operation_id,
        app_operation_record& _operation) override {
        if (_operation_id != "cancel_wire_001") {
            return {status_code::invalid_state, "operation is already terminal"};
        }
        _operation = {"cancel_wire_001", "cancel_wire_001",
            "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
            "security.fire_smoke_detection", app_operation_kind::install,
            app_operation_state::cancelled, status_code::invalid_state,
            "operation cancelled before execution", 0};
        cancel_called_ = true;
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
        association.app_version_ = "1.0.0";
        association.release_sequence_ = 1;
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
        association.components_.push_back({"yolo11n_fire_smoke", "1.0",
            app_component_type::model, "qcs6490_qlinux_1_8",
            "4b74ab5cfea57042dc9dbf26f19633552e08562c3e8a93c416e3e9cde2e0b513",
            3442520U,
            "436ea6a5df7eb7d8e13706639a7ebc1b3f6e6459a80703fd459ca01af982be42",
            app_model_role::primary,
            "/opt/lacai/models/app_content/4b74ab5cfea57042dc9dbf26f19633552e08562c3e8a93c416e3e9cde2e0b513"});
        association.entitlement_expires_utc_ns_ = 9000000000000000000ULL;
        _snapshot.associations_.push_back(std::move(association));
    }

    bool desired_called_{false};
    bool entitlement_called_{false};
    bool configuration_called_{false};
    bool update_called_{false};
    bool rollback_called_{false};
    bool submit_update_called_{false};
    bool cancel_called_{false};
    app_operation_record operation_;
};

void vqec_vision_ai_unit_amdtst_request_name(
    GDBusConnection* _connection, const char* _name) {
    GError* error = nullptr;
    GVariant* reply = g_dbus_connection_call_sync(_connection, "org.freedesktop.DBus",
        "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName",
        g_variant_new("(su)", _name, g_request_name_do_not_queue),
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

void vqec_vision_ai_unit_amdtst_release_name(
    GDBusConnection* _connection, const char* _name) {
    GError* error = nullptr;
    GVariant* reply = g_dbus_connection_call_sync(_connection, "org.freedesktop.DBus",
        "/org/freedesktop/DBus", "org.freedesktop.DBus", "ReleaseName",
        g_variant_new("(s)", _name), G_VARIANT_TYPE("(u)"),
        G_DBUS_CALL_FLAGS_NONE, g_rpc_timeout_ms, nullptr, &error);
    if (reply == nullptr) {
        const std::string message = error == nullptr ?
            "cannot release app manager test peer name" : error->message;
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
    {
        fake_app_manager port;
        app_manager_dbus_server server;
        const app_manager_dbus_config config{g_service_name, g_object_path,
            g_backend_name, g_runtime_name, g_rpc_timeout_ms,
            g_callbacks_per_poll, true};
        const auto opened = server.vqec_vision_ai_fwctl_amdbs_open(port, config);
        if (opened.code_ != status_code::ok) {
            throw std::runtime_error(opened.message_);
        }
        // App Manager must start before the backend and bind the backend's current
        // unique owner at call time so a backend restart does not require daemon restart.
        vqec_vision_ai_unit_amdtst_request_name(peer, g_backend_name);

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

        vqec_vision_ai_unit_amdtst_release_name(peer, g_backend_name);
        app_manager_dbus_client client;
        const app_manager_dbus_client_config client_config{
            g_service_name, g_backend_name, g_object_path, g_rpc_timeout_ms, true};
        auto repeated_fetch = std::async(std::launch::async,
            [&client, &client_config]() {
                runtime_control_snapshot first;
                runtime_control_snapshot second;
                const auto first_status =
                    client.vqec_vision_ai_fwctl_amdbs_fetch_snapshot(
                        client_config, first);
                const auto second_status =
                    client.vqec_vision_ai_fwctl_amdbs_fetch_snapshot(
                        client_config, second);
                return first_status.code_ == status_code::ok &&
                    second_status.code_ == status_code::ok &&
                    first.snapshot_revision_ == second.snapshot_revision_;
            });
        for (std::size_t iteration = 0;
             iteration < g_max_poll_iterations &&
             repeated_fetch.wait_for(std::chrono::milliseconds(0)) !=
                 std::future_status::ready;
             ++iteration) {
            server.vqec_vision_ai_fwctl_amdbs_poll();
            std::this_thread::sleep_for(g_poll_interval);
        }
        if (repeated_fetch.wait_for(std::chrono::milliseconds(0)) !=
                std::future_status::ready ||
            !repeated_fetch.get()) {
            throw std::runtime_error("App Manager client cannot refresh a snapshot");
        }
        auto list_call = std::async(std::launch::async,
            [&client, &client_config]() {
                std::vector<app_catalog_status> applications;
                const auto listed =
                    client.vqec_vision_ai_fwctl_amdbs_list_applications(
                        client_config, "camera_front", applications);
                return listed.code_ == status_code::ok &&
                    applications.size() == 2U &&
                    !applications[0].supported_ &&
                    applications[1].installed_ &&
                    applications[1].state_ ==
                        app_install_state::installed_disabled;
            });
        for (std::size_t iteration = 0;
             iteration < g_max_poll_iterations &&
             list_call.wait_for(std::chrono::milliseconds(0)) !=
                 std::future_status::ready;
             ++iteration) {
            server.vqec_vision_ai_fwctl_amdbs_poll();
            std::this_thread::sleep_for(g_poll_interval);
        }
        if (list_call.wait_for(std::chrono::milliseconds(0)) !=
                std::future_status::ready ||
            !list_call.get()) {
            throw std::runtime_error("App Manager catalog wire is invalid");
        }
        const std::vector<std::uint8_t> configuration{'{', '}'};
        auto configuration_call = std::async(std::launch::async,
            [&client, &client_config, &configuration]() {
                std::uint64_t revision = 0;
                const auto applied =
                    client.vqec_vision_ai_fwctl_amdbs_apply_configuration(
                        client_config, "security.fire_smoke_detection", 1,
                        configuration,
                        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                        revision);
                return applied.code_ == status_code::ok && revision == 5;
            });
        for (std::size_t iteration = 0;
             iteration < g_max_poll_iterations &&
             configuration_call.wait_for(std::chrono::milliseconds(0)) !=
                 std::future_status::ready;
             ++iteration) {
            server.vqec_vision_ai_fwctl_amdbs_poll();
            std::this_thread::sleep_for(g_poll_interval);
        }
        if (configuration_call.wait_for(std::chrono::milliseconds(0)) !=
                std::future_status::ready ||
            !configuration_call.get() || !port.configuration_called_) {
            throw std::runtime_error("App Manager configuration FD wire is invalid");
        }
        app_entitlement_candidate entitlement;
        entitlement.grant_payload_ = {'{', '}'};
        entitlement.signature_payload_ = {'s'};
        entitlement.grant_sha256_ =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        auto entitlement_call = std::async(std::launch::async,
            [&client, &client_config, &entitlement]() {
                std::uint64_t revision = 0;
                const auto applied =
                    client.vqec_vision_ai_fwctl_amdbs_apply_entitlement(
                        client_config, entitlement, revision);
                return applied.code_ == status_code::ok && revision == 5;
            });
        for (std::size_t iteration = 0;
             iteration < g_max_poll_iterations &&
             entitlement_call.wait_for(std::chrono::milliseconds(0)) !=
                 std::future_status::ready;
             ++iteration) {
            server.vqec_vision_ai_fwctl_amdbs_poll();
            std::this_thread::sleep_for(g_poll_interval);
        }
        if (entitlement_call.wait_for(std::chrono::milliseconds(0)) !=
                std::future_status::ready ||
            !entitlement_call.get() || !port.entitlement_called_) {
            throw std::runtime_error("App Manager entitlement FD wire is invalid");
        }
        app_package_candidate update;
        update.manifest_payload_ = {'{', '}'};
        update.configuration_payload_ = {'{', '}'};
        update.signature_payload_ = {'s'};
        update.manifest_sha256_ =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        update.configuration_sha256_ =
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
        const int component_descriptor = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (component_descriptor < 0) {
            throw std::runtime_error("cannot open component descriptor fixture");
        }
        update.components_.push_back({component_descriptor});
        auto update_call = std::async(std::launch::async,
            [&client, &client_config, &update]() {
                std::uint64_t revision = 0;
                const auto applied = client.vqec_vision_ai_fwctl_amdbs_update(
                    client_config, update, 2, revision);
                return applied.code_ == status_code::ok && revision == 5;
            });
        for (std::size_t iteration = 0;
             iteration < g_max_poll_iterations &&
             update_call.wait_for(std::chrono::milliseconds(0)) !=
                 std::future_status::ready;
             ++iteration) {
            server.vqec_vision_ai_fwctl_amdbs_poll();
            std::this_thread::sleep_for(g_poll_interval);
        }
        const bool update_passed =
            update_call.wait_for(std::chrono::milliseconds(0)) ==
                std::future_status::ready && update_call.get();
        (void)::close(component_descriptor);
        if (!update_passed || !port.update_called_) {
            throw std::runtime_error("App Manager update FD wire is invalid");
        }
        const int submitted_component_descriptor =
            ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (submitted_component_descriptor < 0) {
            throw std::runtime_error("cannot open submitted component fixture");
        }
        update.components_[0].descriptor_ = submitted_component_descriptor;
        const app_operation_request operation_request{
            "update_wire_001",
            "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
            "security.fire_smoke_detection", app_operation_kind::update};
        auto submit_update_call = std::async(std::launch::async,
            [&client, &client_config, &operation_request, &update]() {
                std::string operation_id;
                const auto submitted =
                    client.vqec_vision_ai_fwctl_amdbs_submit_update(
                        client_config, operation_request, update, 2,
                        operation_id);
                return submitted.code_ == status_code::ok &&
                    operation_id == "update_wire_001";
            });
        for (std::size_t iteration = 0;
             iteration < g_max_poll_iterations &&
             submit_update_call.wait_for(std::chrono::milliseconds(0)) !=
                 std::future_status::ready;
             ++iteration) {
            server.vqec_vision_ai_fwctl_amdbs_poll();
            std::this_thread::sleep_for(g_poll_interval);
        }
        const bool submit_update_passed =
            submit_update_call.wait_for(std::chrono::milliseconds(0)) ==
                std::future_status::ready && submit_update_call.get();
        (void)::close(submitted_component_descriptor);
        if (!submit_update_passed || !port.submit_update_called_) {
            throw std::runtime_error(
                "App Manager submitted update wire is invalid");
        }
        auto operation_call = std::async(std::launch::async,
            [&client, &client_config]() {
                app_operation_record operation;
                const auto queried =
                    client.vqec_vision_ai_fwctl_amdbs_get_operation(
                        client_config, "update_wire_001", operation);
                return queried.code_ == status_code::ok &&
                    operation.operation_id_ == "update_wire_001" &&
                    operation.kind_ == app_operation_kind::update &&
                    operation.state_ == app_operation_state::committed &&
                    operation.snapshot_revision_ == 5;
            });
        for (std::size_t iteration = 0;
             iteration < g_max_poll_iterations &&
             operation_call.wait_for(std::chrono::milliseconds(0)) !=
                 std::future_status::ready;
             ++iteration) {
            server.vqec_vision_ai_fwctl_amdbs_poll();
            std::this_thread::sleep_for(g_poll_interval);
        }
        if (operation_call.wait_for(std::chrono::milliseconds(0)) !=
                std::future_status::ready || !operation_call.get()) {
            throw std::runtime_error("App Manager operation query wire is invalid");
        }
        auto cancel_call = std::async(std::launch::async,
            [&client, &client_config]() {
                app_operation_record operation;
                const auto cancelled =
                    client.vqec_vision_ai_fwctl_amdbs_cancel_operation(
                        client_config, "cancel_wire_001", operation);
                return cancelled.code_ == status_code::ok &&
                    operation.state_ == app_operation_state::cancelled;
            });
        for (std::size_t iteration = 0;
             iteration < g_max_poll_iterations &&
             cancel_call.wait_for(std::chrono::milliseconds(0)) !=
                 std::future_status::ready;
             ++iteration) {
            server.vqec_vision_ai_fwctl_amdbs_poll();
            std::this_thread::sleep_for(g_poll_interval);
        }
        if (cancel_call.wait_for(std::chrono::milliseconds(0)) !=
                std::future_status::ready || !cancel_call.get() ||
            !port.cancel_called_) {
            throw std::runtime_error(
                "App Manager operation cancel wire is invalid");
        }
        auto rollback_call = std::async(std::launch::async,
            [&client, &client_config]() {
                std::uint64_t revision = 0;
                const auto restored = client.vqec_vision_ai_fwctl_amdbs_rollback(
                    client_config, "security.fire_smoke_detection", 3, revision);
                return restored.code_ == status_code::ok && revision == 5;
            });
        for (std::size_t iteration = 0;
             iteration < g_max_poll_iterations &&
             rollback_call.wait_for(std::chrono::milliseconds(0)) !=
                 std::future_status::ready;
             ++iteration) {
            server.vqec_vision_ai_fwctl_amdbs_poll();
            std::this_thread::sleep_for(g_poll_interval);
        }
        if (rollback_call.wait_for(std::chrono::milliseconds(0)) !=
                std::future_status::ready || !rollback_call.get() ||
            !port.rollback_called_) {
            throw std::runtime_error("App Manager rollback wire is invalid");
        }
        app_manager_dbus_client runtime_client;
        const app_manager_dbus_client_config runtime_config{
            g_service_name, g_runtime_name, g_object_path, g_rpc_timeout_ms, true};
        auto runtime_fetch = std::async(std::launch::async,
            [&runtime_client, &runtime_config]() {
                runtime_control_snapshot snapshot;
                return runtime_client.vqec_vision_ai_fwctl_amdbs_fetch_snapshot(
                           runtime_config, snapshot).code_ == status_code::ok &&
                    snapshot.snapshot_revision_ == 4;
            });
        for (std::size_t iteration = 0;
             iteration < g_max_poll_iterations &&
             runtime_fetch.wait_for(std::chrono::milliseconds(0)) !=
                 std::future_status::ready;
             ++iteration) {
            server.vqec_vision_ai_fwctl_amdbs_poll();
            std::this_thread::sleep_for(g_poll_interval);
        }
        if (runtime_fetch.wait_for(std::chrono::milliseconds(0)) !=
                std::future_status::ready || !runtime_fetch.get()) {
            throw std::runtime_error("runtime peer cannot read App Manager snapshot");
        }
        auto unauthorized_mutation = std::async(std::launch::async,
            [&runtime_client, &runtime_config]() {
                std::uint64_t revision = 0;
                const app_desired_update update{
                    "security.fire_smoke_detection", "camera_front", 4, true};
                return runtime_client.vqec_vision_ai_fwctl_amdbs_set_desired(
                           runtime_config, update, revision).code_ != status_code::ok;
            });
        for (std::size_t iteration = 0;
             iteration < g_max_poll_iterations &&
             unauthorized_mutation.wait_for(std::chrono::milliseconds(0)) !=
                 std::future_status::ready;
             ++iteration) {
            server.vqec_vision_ai_fwctl_amdbs_poll();
            std::this_thread::sleep_for(g_poll_interval);
        }
        if (unauthorized_mutation.wait_for(std::chrono::milliseconds(0)) !=
                std::future_status::ready || !unauthorized_mutation.get()) {
            throw std::runtime_error("runtime peer can mutate App Manager state");
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
