#include "vqec_vision_app_manager_dbus.hpp"

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <new>
#include <sstream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "vqec_vision_runtime_control_snapshot.hpp"

namespace vqec::vision::ai {
namespace {

constexpr guint g_request_name_do_not_queue = 4U;
constexpr guint g_request_name_primary_owner = 1U;
constexpr guint g_request_name_already_owner = 4U;
constexpr std::size_t g_max_callbacks_ceiling = 64;
constexpr gsize g_max_request_wire_bytes = 128U * 1024U;
constexpr std::size_t g_max_signature_bytes = 64U * 1024U;

struct dbus_binding {
    app_manager_port* port_{nullptr};
    std::string trusted_sender_;
};

constexpr char g_introspection_xml[] =
    "<node><interface name='com.vqec.AiVision.AppManager1'>"
    "<method name='Install'><arg type='h' direction='in'/><arg type='h' direction='in'/><arg type='h' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='t' direction='out'/></method>"
    "<method name='ApplyConfiguration'><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='h' direction='in'/><arg type='s' direction='in'/><arg type='t' direction='out'/></method>"
    "<method name='SetDesired'><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='b' direction='in'/><arg type='t' direction='out'/></method>"
    "<method name='Uninstall'><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='t' direction='out'/></method>"
    "<method name='GetSnapshot'><arg type='s' direction='out'/></method>"
    "</interface></node>";

struct error_owner {
    GError* value_{nullptr};
    ~error_owner() noexcept { if (value_ != nullptr) { g_error_free(value_); } }
};

struct node_owner {
    GDBusNodeInfo* value_{nullptr};
    ~node_owner() noexcept { if (value_ != nullptr) { g_dbus_node_info_unref(value_); } }
};

void vqec_vision_ai_fwctl_amdbs_return_error(
    GDBusMethodInvocation* _invocation, const status& _result) {
    g_dbus_method_invocation_return_error(_invocation, G_DBUS_ERROR,
        G_DBUS_ERROR_FAILED, "%s", _result.message_.c_str());
}

status vqec_vision_ai_fwctl_amdbs_read_fd(GDBusMethodInvocation* _invocation,
    gint32 _handle, std::size_t _max_bytes, std::vector<std::uint8_t>& _payload) {
    GDBusMessage* message = g_dbus_method_invocation_get_message(_invocation);
    GUnixFDList* list = message == nullptr ? nullptr :
        g_dbus_message_get_unix_fd_list(message);
    if (list == nullptr || _handle < 0) {
        return {status_code::invalid_argument, "request has no Unix descriptor list"};
    }
    error_owner error;
    const int fd = g_unix_fd_list_get(list, _handle, &error.value_);
    if (fd < 0) {
        return {status_code::invalid_argument, "request Unix descriptor is invalid"};
    }
    std::vector<std::uint8_t> candidate;
    try {
        candidate.reserve(std::min<std::size_t>(_max_bytes, 65536U));
        std::array<std::uint8_t, 16384> block{};
        for (;;) {
            const auto count = ::read(fd, block.data(), block.size());
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                (void)::close(fd);
                return {status_code::io_error, "request Unix descriptor read failed"};
            }
            if (count == 0) {
                break;
            }
            const auto bytes = static_cast<std::size_t>(count);
            if (bytes > _max_bytes - candidate.size()) {
                (void)::close(fd);
                return {status_code::resource_exhausted,
                    "request Unix descriptor exceeds byte limit"};
            }
            candidate.insert(candidate.end(), block.begin(), block.begin() + count);
        }
    } catch (const std::bad_alloc&) {
        (void)::close(fd);
        return {status_code::resource_exhausted,
            "request Unix descriptor allocation failed"};
    }
    (void)::close(fd);
    if (candidate.empty()) {
        return {status_code::invalid_argument, "request Unix descriptor is empty"};
    }
    _payload = std::move(candidate);
    return {};
}

void vqec_vision_ai_fwctl_amdbs_return_revision(
    GDBusMethodInvocation* _invocation, const status& _result,
    const runtime_control_snapshot& _snapshot) {
    if (_result.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, _result);
        return;
    }
    g_dbus_method_invocation_return_value(_invocation,
        g_variant_new("(t)", _snapshot.snapshot_revision_));
}

void vqec_vision_ai_fwctl_amdbs_install(app_manager_port& _port,
    GVariant* _parameters, GDBusMethodInvocation* _invocation) {
    gint32 manifest_handle = -1;
    gint32 configuration_handle = -1;
    gint32 signature_handle = -1;
    const gchar* manifest_sha256 = nullptr;
    const gchar* configuration_sha256 = nullptr;
    guint64 expected_revision = 0;
    g_variant_get(_parameters, "(hhh&s&st)", &manifest_handle,
        &configuration_handle, &signature_handle, &manifest_sha256,
        &configuration_sha256, &expected_revision);
    app_package_candidate candidate;
    auto current = vqec_vision_ai_fwctl_amdbs_read_fd(_invocation,
        manifest_handle, app_lifecycle_limits::g_max_document_bytes,
        candidate.manifest_payload_);
    if (current.code_ == status_code::ok) {
        current = vqec_vision_ai_fwctl_amdbs_read_fd(_invocation,
            configuration_handle, app_lifecycle_limits::g_max_document_bytes,
            candidate.configuration_payload_);
    }
    if (current.code_ == status_code::ok) {
        current = vqec_vision_ai_fwctl_amdbs_read_fd(_invocation,
            signature_handle, g_max_signature_bytes, candidate.signature_payload_);
    }
    if (current.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, current);
        return;
    }
    candidate.manifest_sha256_ = manifest_sha256 == nullptr ? "" : manifest_sha256;
    candidate.configuration_sha256_ = configuration_sha256 == nullptr ?
        "" : configuration_sha256;
    runtime_control_snapshot snapshot;
    const auto installed = _port.vqec_vision_ai_ports_apmgr_install(
        candidate, expected_revision, snapshot);
    vqec_vision_ai_fwctl_amdbs_return_revision(_invocation, installed, snapshot);
}

void vqec_vision_ai_fwctl_amdbs_configuration(app_manager_port& _port,
    GVariant* _parameters, GDBusMethodInvocation* _invocation) {
    const gchar* app_id = nullptr;
    guint64 expected_revision = 0;
    gint32 configuration_handle = -1;
    const gchar* sha256 = nullptr;
    g_variant_get(_parameters, "(&sth&s)", &app_id, &expected_revision,
        &configuration_handle, &sha256);
    std::vector<std::uint8_t> payload;
    const auto read = vqec_vision_ai_fwctl_amdbs_read_fd(_invocation,
        configuration_handle, app_lifecycle_limits::g_max_document_bytes, payload);
    if (read.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, read);
        return;
    }
    runtime_control_snapshot snapshot;
    const auto updated = _port.vqec_vision_ai_ports_apmgr_update_configuration(
        app_id == nullptr ? "" : app_id, expected_revision, payload,
        sha256 == nullptr ? "" : sha256, snapshot);
    vqec_vision_ai_fwctl_amdbs_return_revision(_invocation, updated, snapshot);
}

void vqec_vision_ai_fwctl_amdbs_desired(app_manager_port& _port,
    GVariant* _parameters, GDBusMethodInvocation* _invocation) {
    const gchar* app_id = nullptr;
    const gchar* source_id = nullptr;
    guint64 expected_revision = 0;
    gboolean desired = FALSE;
    g_variant_get(_parameters, "(&s&stb)", &app_id, &source_id,
        &expected_revision, &desired);
    app_desired_update update{app_id == nullptr ? "" : app_id,
        source_id == nullptr ? "" : source_id, expected_revision,
        desired != FALSE};
    runtime_control_snapshot snapshot;
    const auto updated = _port.vqec_vision_ai_ports_apmgr_set_desired(update, snapshot);
    vqec_vision_ai_fwctl_amdbs_return_revision(_invocation, updated, snapshot);
}

void vqec_vision_ai_fwctl_amdbs_uninstall(app_manager_port& _port,
    GVariant* _parameters, GDBusMethodInvocation* _invocation) {
    const gchar* app_id = nullptr;
    guint64 expected_revision = 0;
    g_variant_get(_parameters, "(&st)", &app_id, &expected_revision);
    runtime_control_snapshot snapshot;
    const auto removed = _port.vqec_vision_ai_ports_apmgr_uninstall(
        app_id == nullptr ? "" : app_id, expected_revision, snapshot);
    vqec_vision_ai_fwctl_amdbs_return_revision(_invocation, removed, snapshot);
}

void vqec_vision_ai_fwctl_amdbs_snapshot(app_manager_port& _port,
    GDBusMethodInvocation* _invocation) {
    runtime_control_snapshot snapshot;
    const auto queried = _port.vqec_vision_ai_ports_apmgr_get_snapshot(snapshot);
    if (queried.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, queried);
        return;
    }
    std::ostringstream stream;
    const auto encoded = vqec_vision_ai_lifec_rcsnp_write(snapshot, stream);
    if (encoded.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, encoded);
        return;
    }
    g_dbus_method_invocation_return_value(_invocation,
        g_variant_new("(s)", stream.str().c_str()));
}

void vqec_vision_ai_fwctl_amdbs_method_call(GDBusConnection*,
    const gchar* _sender, const gchar*, const gchar*, const gchar* _method_name,
    GVariant* _parameters, GDBusMethodInvocation* _invocation,
    gpointer _user_data) {
    auto* binding = static_cast<dbus_binding*>(_user_data);
    if (binding == nullptr || _sender == nullptr ||
        binding->trusted_sender_ != _sender) {
        g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
            G_DBUS_ERROR_ACCESS_DENIED,
            "app manager caller is not the configured backend peer");
        return;
    }
    if (g_variant_get_size(_parameters) > g_max_request_wire_bytes) {
        g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
            G_DBUS_ERROR_LIMITS_EXCEEDED,
            "app manager request exceeds wire byte limit");
        return;
    }
    if (binding->port_ == nullptr) {
        g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED, "app manager port unavailable");
        return;
    }
    try {
        if (g_strcmp0(_method_name, app_manager_dbus_protocol::g_install_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_install(
                *binding->port_, _parameters, _invocation);
        } else if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_configuration_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_configuration(
                *binding->port_, _parameters, _invocation);
        } else if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_desired_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_desired(
                *binding->port_, _parameters, _invocation);
        } else if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_uninstall_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_uninstall(
                *binding->port_, _parameters, _invocation);
        } else if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_snapshot_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_snapshot(*binding->port_, _invocation);
        } else {
            g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
                G_DBUS_ERROR_UNKNOWN_METHOD, "unknown app manager method");
        }
    } catch (...) {
        g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED, "app manager operation failed; query snapshot");
    }
}

const GDBusInterfaceVTable g_vtable = {
    vqec_vision_ai_fwctl_amdbs_method_call, nullptr, nullptr,
    {nullptr, nullptr, nullptr, nullptr}};

}  // namespace

struct app_manager_dbus_client::implementation {
    GDBusConnection* connection_{nullptr};
    ~implementation() noexcept {
        if (connection_ != nullptr) {
            g_dbus_connection_close(connection_, nullptr, nullptr, nullptr);
            g_object_unref(connection_);
        }
    }
};

app_manager_dbus_client::app_manager_dbus_client()
    : implementation_(std::make_unique<implementation>()) {}
app_manager_dbus_client::~app_manager_dbus_client() noexcept = default;

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_fetch_snapshot(
    const app_manager_dbus_client_config& _config,
    runtime_control_snapshot& _snapshot) {
    if (implementation_->connection_ != nullptr) {
        return {status_code::invalid_state,
            "app manager DBus client already fetched a snapshot"};
    }
    if (!g_dbus_is_name(_config.service_bus_name_.c_str()) ||
        !g_variant_is_object_path(_config.object_path_.c_str()) ||
        _config.rpc_timeout_ms_ <= 0) {
        return {status_code::invalid_argument,
            "invalid app manager DBus client configuration"};
    }
    error_owner error;
    gchar* address = g_dbus_address_get_for_bus_sync(
        _config.use_session_bus_ ? G_BUS_TYPE_SESSION : G_BUS_TYPE_SYSTEM,
        nullptr, &error.value_);
    if (address == nullptr) {
        return {status_code::io_error,
            "cannot resolve app manager DBus client address"};
    }
    implementation_->connection_ = g_dbus_connection_new_for_address_sync(address,
        static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
            G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION), nullptr, nullptr,
        &error.value_);
    g_free(address);
    if (implementation_->connection_ == nullptr) {
        return {status_code::io_error, "cannot connect to app manager DBus"};
    }
    GVariant* owner_reply = g_dbus_connection_call_sync(implementation_->connection_,
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "GetNameOwner", g_variant_new("(s)", _config.service_bus_name_.c_str()),
        G_VARIANT_TYPE("(s)"), G_DBUS_CALL_FLAGS_NONE, _config.rpc_timeout_ms_,
        nullptr, &error.value_);
    if (owner_reply == nullptr) {
        return {status_code::source_lost, "app manager DBus name has no owner"};
    }
    const gchar* owner = nullptr;
    g_variant_get(owner_reply, "(&s)", &owner);
    const std::string unique_owner = owner == nullptr ? "" : owner;
    g_variant_unref(owner_reply);
    if (unique_owner.empty()) {
        return {status_code::protocol_error, "app manager DBus owner is empty"};
    }
    GVariant* reply = g_dbus_connection_call_sync(implementation_->connection_,
        unique_owner.c_str(), _config.object_path_.c_str(),
        app_manager_dbus_protocol::g_interface_name,
        app_manager_dbus_protocol::g_snapshot_method, nullptr,
        G_VARIANT_TYPE("(s)"), G_DBUS_CALL_FLAGS_NONE, _config.rpc_timeout_ms_,
        nullptr, &error.value_);
    if (reply == nullptr) {
        return {status_code::io_error, "app manager GetSnapshot failed"};
    }
    const gchar* document = nullptr;
    g_variant_get(reply, "(&s)", &document);
    const std::string payload = document == nullptr ? "" : document;
    g_variant_unref(reply);
    if (payload.empty() || payload.size() > app_lifecycle_limits::g_max_document_bytes) {
        return {status_code::protocol_error,
            "app manager snapshot exceeds client wire bound"};
    }
    std::istringstream stream(payload);
    return vqec_vision_ai_lifec_rcsnp_load(stream, _snapshot);
}

struct app_manager_dbus_server::implementation : dbus_binding {
    GDBusConnection* connection_{nullptr};
    guint registration_id_{0};
    GDBusNodeInfo* node_{nullptr};
    GMainContext* context_{nullptr};
    std::size_t max_callbacks_per_poll_{0};
    ~implementation() noexcept {
        if (connection_ != nullptr && registration_id_ != 0) {
            g_dbus_connection_unregister_object(connection_, registration_id_);
        }
        if (node_ != nullptr) {
            g_dbus_node_info_unref(node_);
        }
        if (connection_ != nullptr) {
            g_dbus_connection_close(connection_, nullptr, nullptr, nullptr);
            g_object_unref(connection_);
        }
        if (context_ != nullptr) {
            g_main_context_unref(context_);
        }
    }
};

app_manager_dbus_server::app_manager_dbus_server()
    : implementation_(std::make_unique<implementation>()) {}
app_manager_dbus_server::~app_manager_dbus_server() noexcept = default;

status app_manager_dbus_server::vqec_vision_ai_fwctl_amdbs_open(
    app_manager_port& _port, const app_manager_dbus_config& _config) {
    if (implementation_->connection_ != nullptr) {
        return {status_code::invalid_state, "app manager DBus is already open"};
    }
    if (!g_dbus_is_name(_config.service_bus_name_.c_str()) ||
        !g_variant_is_object_path(_config.object_path_.c_str()) ||
        !g_dbus_is_name(_config.trusted_peer_bus_name_.c_str()) ||
        _config.rpc_timeout_ms_ <= 0 || _config.max_callbacks_per_poll_ == 0 ||
        _config.max_callbacks_per_poll_ > g_max_callbacks_ceiling) {
        return {status_code::invalid_argument,
            "invalid app manager DBus configuration"};
    }
    error_owner error;
    gchar* address = g_dbus_address_get_for_bus_sync(
        _config.use_session_bus_ ? G_BUS_TYPE_SESSION : G_BUS_TYPE_SYSTEM,
        nullptr, &error.value_);
    if (address == nullptr) {
        return {status_code::io_error, "cannot resolve app manager DBus address"};
    }
    implementation_->connection_ = g_dbus_connection_new_for_address_sync(address,
        static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
            G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION), nullptr, nullptr,
        &error.value_);
    g_free(address);
    if (implementation_->connection_ == nullptr) {
        return {status_code::io_error, "cannot connect to app manager DBus"};
    }
    g_dbus_connection_set_exit_on_close(implementation_->connection_, FALSE);
    GVariant* peer_reply = g_dbus_connection_call_sync(implementation_->connection_,
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "GetNameOwner", g_variant_new("(s)", _config.trusted_peer_bus_name_.c_str()),
        G_VARIANT_TYPE("(s)"), G_DBUS_CALL_FLAGS_NONE, _config.rpc_timeout_ms_,
        nullptr, &error.value_);
    if (peer_reply == nullptr) {
        return {status_code::unauthorized,
            "configured backend peer has no DBus owner"};
    }
    const gchar* sender = nullptr;
    g_variant_get(peer_reply, "(&s)", &sender);
    implementation_->trusted_sender_ = sender;
    g_variant_unref(peer_reply);
    implementation_->context_ = g_main_context_new();
    implementation_->max_callbacks_per_poll_ = _config.max_callbacks_per_poll_;
    implementation_->port_ = &_port;
    node_owner node{g_dbus_node_info_new_for_xml(g_introspection_xml, &error.value_)};
    if (node.value_ == nullptr || node.value_->interfaces == nullptr ||
        node.value_->interfaces[0] == nullptr) {
        return {status_code::protocol_error,
            "invalid app manager DBus introspection"};
    }
    implementation_->node_ = node.value_;
    node.value_ = nullptr;
    g_main_context_push_thread_default(implementation_->context_);
    implementation_->registration_id_ = g_dbus_connection_register_object(
        implementation_->connection_, _config.object_path_.c_str(),
        implementation_->node_->interfaces[0], &g_vtable,
        static_cast<dbus_binding*>(implementation_.get()), nullptr, &error.value_);
    g_main_context_pop_thread_default(implementation_->context_);
    if (implementation_->registration_id_ == 0) {
        return {status_code::io_error,
            "cannot register app manager DBus object"};
    }
    GVariant* reply = g_dbus_connection_call_sync(implementation_->connection_,
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "RequestName", g_variant_new("(su)", _config.service_bus_name_.c_str(),
            g_request_name_do_not_queue), G_VARIANT_TYPE("(u)"),
        G_DBUS_CALL_FLAGS_NONE, _config.rpc_timeout_ms_, nullptr, &error.value_);
    if (reply == nullptr) {
        return {status_code::io_error, "cannot acquire app manager DBus name"};
    }
    guint32 reply_code = 0;
    g_variant_get(reply, "(u)", &reply_code);
    g_variant_unref(reply);
    if (reply_code != g_request_name_primary_owner &&
        reply_code != g_request_name_already_owner) {
        return {status_code::invalid_state,
            "app manager DBus name is already owned"};
    }
    return {};
}

void app_manager_dbus_server::vqec_vision_ai_fwctl_amdbs_poll() noexcept {
    if (implementation_->connection_ == nullptr) {
        return;
    }
    for (std::size_t index = 0;
         index < implementation_->max_callbacks_per_poll_ &&
         g_main_context_pending(implementation_->context_); ++index) {
        g_main_context_iteration(implementation_->context_, FALSE);
    }
}

}  // namespace vqec::vision::ai
