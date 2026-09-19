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
    std::string trusted_backend_bus_name_;
    std::string trusted_runtime_bus_name_;
    int rpc_timeout_ms_{0};
};

constexpr char g_introspection_xml[] =
    "<node><interface name='com.vqec.AiVision.AppManager1'>"
    "<method name='Install'><arg type='h' direction='in'/><arg type='h' direction='in'/><arg type='h' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='t' direction='out'/></method>"
    "<method name='ApplyConfiguration'><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='h' direction='in'/><arg type='s' direction='in'/><arg type='t' direction='out'/></method>"
    "<method name='ApplyEntitlement'><arg type='h' direction='in'/><arg type='h' direction='in'/><arg type='s' direction='in'/><arg type='t' direction='out'/></method>"
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

struct fd_owner {
    int value_{-1};
    ~fd_owner() noexcept { if (value_ >= 0) { (void)::close(value_); } }
};

status vqec_vision_ai_fwctl_amdbs_make_payload_fd(
    const std::vector<std::uint8_t>& _payload, fd_owner& _fd) {
    if (_payload.empty() || _payload.size() > app_lifecycle_limits::g_max_document_bytes) {
        return {status_code::invalid_argument, "invalid App Manager client payload"};
    }
    error_owner error;
    gchar* temporary_path = nullptr;
    _fd.value_ = g_file_open_tmp("vqec-app-manager-XXXXXX", &temporary_path,
        &error.value_);
    if (_fd.value_ < 0) {
        return {status_code::io_error, "cannot create App Manager client payload"};
    }
    if (temporary_path != nullptr) {
        (void)::unlink(temporary_path);
        g_free(temporary_path);
    }
    std::size_t offset = 0;
    while (offset < _payload.size()) {
        const auto count = ::write(_fd.value_, _payload.data() + offset,
            _payload.size() - offset);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return {status_code::io_error, "App Manager client payload write failed"};
        }
        offset += static_cast<std::size_t>(count);
    }
    if (::lseek(_fd.value_, 0, SEEK_SET) != 0) {
        return {status_code::io_error, "App Manager client payload rewind failed"};
    }
    return {};
}

status vqec_vision_ai_fwctl_amdbs_take_revision(
    GVariant* _reply, std::uint64_t& _snapshot_revision) {
    if (_reply == nullptr) {
        return {status_code::io_error, "App Manager mutation call failed"};
    }
    guint64 revision = 0;
    g_variant_get(_reply, "(t)", &revision);
    g_variant_unref(_reply);
    if (revision == 0) {
        return {status_code::protocol_error,
            "App Manager returned an invalid snapshot revision"};
    }
    _snapshot_revision = revision;
    return {};
}

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

void vqec_vision_ai_fwctl_amdbs_entitlement(app_manager_port& _port,
    GVariant* _parameters, GDBusMethodInvocation* _invocation) {
    gint32 grant_handle = -1;
    gint32 signature_handle = -1;
    const gchar* grant_sha256 = nullptr;
    g_variant_get(_parameters, "(hh&s)", &grant_handle, &signature_handle,
        &grant_sha256);
    app_entitlement_candidate candidate;
    auto current = vqec_vision_ai_fwctl_amdbs_read_fd(_invocation,
        grant_handle, app_lifecycle_limits::g_max_document_bytes,
        candidate.grant_payload_);
    if (current.code_ == status_code::ok) {
        current = vqec_vision_ai_fwctl_amdbs_read_fd(_invocation,
            signature_handle, g_max_signature_bytes, candidate.signature_payload_);
    }
    if (current.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, current);
        return;
    }
    candidate.grant_sha256_ = grant_sha256 == nullptr ? "" : grant_sha256;
    runtime_control_snapshot snapshot;
    const auto applied = _port.vqec_vision_ai_ports_apmgr_apply_entitlement(
        candidate, snapshot);
    vqec_vision_ai_fwctl_amdbs_return_revision(_invocation, applied, snapshot);
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

bool vqec_vision_ai_fwctl_amdbs_is_named_sender(GDBusConnection* _connection,
    const std::string& _trusted_bus_name, int _rpc_timeout_ms,
    const char* _sender) {
    if (_connection == nullptr || _sender == nullptr ||
        _trusted_bus_name.empty()) {
        return false;
    }
    error_owner error;
    GVariant* reply = g_dbus_connection_call_sync(_connection,
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "GetNameOwner", g_variant_new("(s)",
            _trusted_bus_name.c_str()), G_VARIANT_TYPE("(s)"),
        G_DBUS_CALL_FLAGS_NONE, _rpc_timeout_ms, nullptr, &error.value_);
    if (reply == nullptr) {
        return false;
    }
    const gchar* owner = nullptr;
    g_variant_get(reply, "(&s)", &owner);
    const bool matches = owner != nullptr && g_strcmp0(owner, _sender) == 0;
    g_variant_unref(reply);
    return matches;
}

void vqec_vision_ai_fwctl_amdbs_method_call(GDBusConnection* _connection,
    const gchar* _sender, const gchar*, const gchar*, const gchar* _method_name,
    GVariant* _parameters, GDBusMethodInvocation* _invocation,
    gpointer _user_data) {
    auto* binding = static_cast<dbus_binding*>(_user_data);
    const bool is_snapshot = g_strcmp0(
        _method_name, app_manager_dbus_protocol::g_snapshot_method) == 0;
    const bool is_backend = binding != nullptr &&
        vqec_vision_ai_fwctl_amdbs_is_named_sender(_connection,
            binding->trusted_backend_bus_name_, binding->rpc_timeout_ms_, _sender);
    const bool is_runtime = binding != nullptr && is_snapshot &&
        vqec_vision_ai_fwctl_amdbs_is_named_sender(_connection,
            binding->trusted_runtime_bus_name_, binding->rpc_timeout_ms_, _sender);
    if (!is_backend && !is_runtime) {
        g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
            G_DBUS_ERROR_ACCESS_DENIED,
            "app manager caller is not authorized for this method");
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
                       app_manager_dbus_protocol::g_entitlement_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_entitlement(
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
    bool owns_name_{false};
    [[nodiscard]] status vqec_vision_ai_fwctl_amdbs_prepare(
        const app_manager_dbus_client_config& _config,
        std::string& _unique_owner);
    ~implementation() noexcept {
        if (connection_ != nullptr) {
            g_dbus_connection_close(connection_, nullptr, nullptr, nullptr);
            g_object_unref(connection_);
        }
    }
};

status app_manager_dbus_client::implementation::vqec_vision_ai_fwctl_amdbs_prepare(
    const app_manager_dbus_client_config& _config,
    std::string& _unique_owner) {
    if (!g_dbus_is_name(_config.service_bus_name_.c_str()) ||
        !g_dbus_is_name(_config.client_bus_name_.c_str()) ||
        !g_variant_is_object_path(_config.object_path_.c_str()) ||
        _config.rpc_timeout_ms_ <= 0) {
        return {status_code::invalid_argument,
            "invalid app manager DBus client configuration"};
    }
    error_owner error;
    if (connection_ == nullptr) {
        gchar* address = g_dbus_address_get_for_bus_sync(
            _config.use_session_bus_ ? G_BUS_TYPE_SESSION : G_BUS_TYPE_SYSTEM,
            nullptr, &error.value_);
        if (address == nullptr) {
            return {status_code::io_error,
                "cannot resolve app manager DBus client address"};
        }
        connection_ = g_dbus_connection_new_for_address_sync(address,
            static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION), nullptr, nullptr,
            &error.value_);
        g_free(address);
        if (connection_ == nullptr) {
            return {status_code::io_error, "cannot connect to app manager DBus"};
        }
    }
    if (!owns_name_) {
        GVariant* name_reply = g_dbus_connection_call_sync(connection_,
            "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
            "RequestName", g_variant_new("(su)", _config.client_bus_name_.c_str(),
                g_request_name_do_not_queue), G_VARIANT_TYPE("(u)"),
            G_DBUS_CALL_FLAGS_NONE, _config.rpc_timeout_ms_, nullptr, &error.value_);
        if (name_reply == nullptr) {
            return {status_code::source_lost,
                "app manager client cannot request its trusted bus name"};
        }
        guint32 result = 0;
        g_variant_get(name_reply, "(u)", &result);
        g_variant_unref(name_reply);
        if (result != g_request_name_primary_owner &&
            result != g_request_name_already_owner) {
            return {status_code::unauthorized,
                "app manager client trusted bus name is already owned"};
        }
        owns_name_ = true;
    }
    GVariant* owner_reply = g_dbus_connection_call_sync(connection_,
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "GetNameOwner", g_variant_new("(s)", _config.service_bus_name_.c_str()),
        G_VARIANT_TYPE("(s)"), G_DBUS_CALL_FLAGS_NONE, _config.rpc_timeout_ms_,
        nullptr, &error.value_);
    if (owner_reply == nullptr) {
        return {status_code::source_lost, "app manager DBus name has no owner"};
    }
    const gchar* owner = nullptr;
    g_variant_get(owner_reply, "(&s)", &owner);
    _unique_owner = owner == nullptr ? "" : owner;
    g_variant_unref(owner_reply);
    if (_unique_owner.empty()) {
        return {status_code::protocol_error, "app manager DBus owner is empty"};
    }
    return {};
}

app_manager_dbus_client::app_manager_dbus_client()
    : implementation_(std::make_unique<implementation>()) {}
app_manager_dbus_client::~app_manager_dbus_client() noexcept = default;

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_fetch_snapshot(
    const app_manager_dbus_client_config& _config,
    runtime_control_snapshot& _snapshot) {
    std::string unique_owner;
    auto prepared = implementation_->vqec_vision_ai_fwctl_amdbs_prepare(
        _config, unique_owner);
    if (prepared.code_ != status_code::ok) {
        return prepared;
    }
    error_owner error;
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

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_install(
    const app_manager_dbus_client_config& _config,
    const app_package_candidate& _candidate,
    std::uint64_t _expected_inventory_revision,
    std::uint64_t& _snapshot_revision) {
    if (_expected_inventory_revision == 0) {
        return {status_code::invalid_argument, "invalid expected inventory revision"};
    }
    std::string unique_owner;
    auto current = implementation_->vqec_vision_ai_fwctl_amdbs_prepare(
        _config, unique_owner);
    if (current.code_ != status_code::ok) {
        return current;
    }
    fd_owner manifest;
    fd_owner configuration;
    fd_owner signature;
    current = vqec_vision_ai_fwctl_amdbs_make_payload_fd(
        _candidate.manifest_payload_, manifest);
    if (current.code_ == status_code::ok) {
        current = vqec_vision_ai_fwctl_amdbs_make_payload_fd(
            _candidate.configuration_payload_, configuration);
    }
    if (current.code_ == status_code::ok) {
        current = vqec_vision_ai_fwctl_amdbs_make_payload_fd(
            _candidate.signature_payload_, signature);
    }
    if (current.code_ != status_code::ok) {
        return current;
    }
    error_owner error;
    GUnixFDList* descriptors = g_unix_fd_list_new();
    const int manifest_handle = g_unix_fd_list_append(
        descriptors, manifest.value_, &error.value_);
    const int configuration_handle = manifest_handle < 0 ? -1 :
        g_unix_fd_list_append(descriptors, configuration.value_, &error.value_);
    const int signature_handle = configuration_handle < 0 ? -1 :
        g_unix_fd_list_append(descriptors, signature.value_, &error.value_);
    if (signature_handle < 0) {
        g_object_unref(descriptors);
        return {status_code::io_error,
            "cannot attach App Manager package descriptors"};
    }
    GVariant* reply = g_dbus_connection_call_with_unix_fd_list_sync(
        implementation_->connection_, unique_owner.c_str(), _config.object_path_.c_str(),
        app_manager_dbus_protocol::g_interface_name,
        app_manager_dbus_protocol::g_install_method,
        g_variant_new("(hhhsst)", manifest_handle, configuration_handle,
            signature_handle, _candidate.manifest_sha256_.c_str(),
            _candidate.configuration_sha256_.c_str(),
            static_cast<guint64>(_expected_inventory_revision)),
        G_VARIANT_TYPE("(t)"), G_DBUS_CALL_FLAGS_NONE, _config.rpc_timeout_ms_,
        descriptors, nullptr, nullptr, &error.value_);
    g_object_unref(descriptors);
    return vqec_vision_ai_fwctl_amdbs_take_revision(reply, _snapshot_revision);
}

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_apply_entitlement(
    const app_manager_dbus_client_config& _config,
    const app_entitlement_candidate& _candidate,
    std::uint64_t& _snapshot_revision) {
    std::string unique_owner;
    auto current = implementation_->vqec_vision_ai_fwctl_amdbs_prepare(
        _config, unique_owner);
    if (current.code_ != status_code::ok) {
        return current;
    }
    fd_owner grant;
    fd_owner signature;
    current = vqec_vision_ai_fwctl_amdbs_make_payload_fd(
        _candidate.grant_payload_, grant);
    if (current.code_ == status_code::ok) {
        current = vqec_vision_ai_fwctl_amdbs_make_payload_fd(
            _candidate.signature_payload_, signature);
    }
    if (current.code_ != status_code::ok) {
        return current;
    }
    error_owner error;
    GUnixFDList* descriptors = g_unix_fd_list_new();
    const int grant_handle = g_unix_fd_list_append(
        descriptors, grant.value_, &error.value_);
    const int signature_handle = grant_handle < 0 ? -1 :
        g_unix_fd_list_append(descriptors, signature.value_, &error.value_);
    if (signature_handle < 0) {
        g_object_unref(descriptors);
        return {status_code::io_error,
            "cannot attach App Manager entitlement descriptors"};
    }
    GVariant* reply = g_dbus_connection_call_with_unix_fd_list_sync(
        implementation_->connection_, unique_owner.c_str(), _config.object_path_.c_str(),
        app_manager_dbus_protocol::g_interface_name,
        app_manager_dbus_protocol::g_entitlement_method,
        g_variant_new("(hhs)", grant_handle, signature_handle,
            _candidate.grant_sha256_.c_str()), G_VARIANT_TYPE("(t)"),
        G_DBUS_CALL_FLAGS_NONE, _config.rpc_timeout_ms_, descriptors, nullptr,
        nullptr, &error.value_);
    g_object_unref(descriptors);
    return vqec_vision_ai_fwctl_amdbs_take_revision(reply, _snapshot_revision);
}

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_set_desired(
    const app_manager_dbus_client_config& _config,
    const app_desired_update& _update,
    std::uint64_t& _snapshot_revision) {
    std::string unique_owner;
    auto current = implementation_->vqec_vision_ai_fwctl_amdbs_prepare(
        _config, unique_owner);
    if (current.code_ != status_code::ok) {
        return current;
    }
    error_owner error;
    GVariant* reply = g_dbus_connection_call_sync(implementation_->connection_,
        unique_owner.c_str(), _config.object_path_.c_str(),
        app_manager_dbus_protocol::g_interface_name,
        app_manager_dbus_protocol::g_desired_method,
        g_variant_new("(sstb)", _update.app_id_.c_str(), _update.source_id_.c_str(),
            static_cast<guint64>(_update.expected_desired_revision_),
            _update.desired_ ? TRUE : FALSE), G_VARIANT_TYPE("(t)"),
        G_DBUS_CALL_FLAGS_NONE, _config.rpc_timeout_ms_, nullptr, &error.value_);
    return vqec_vision_ai_fwctl_amdbs_take_revision(reply, _snapshot_revision);
}

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_uninstall(
    const app_manager_dbus_client_config& _config,
    const std::string& _app_id,
    std::uint64_t _expected_inventory_revision,
    std::uint64_t& _snapshot_revision) {
    std::string unique_owner;
    auto current = implementation_->vqec_vision_ai_fwctl_amdbs_prepare(
        _config, unique_owner);
    if (current.code_ != status_code::ok) {
        return current;
    }
    error_owner error;
    GVariant* reply = g_dbus_connection_call_sync(implementation_->connection_,
        unique_owner.c_str(), _config.object_path_.c_str(),
        app_manager_dbus_protocol::g_interface_name,
        app_manager_dbus_protocol::g_uninstall_method,
        g_variant_new("(st)", _app_id.c_str(),
            static_cast<guint64>(_expected_inventory_revision)),
        G_VARIANT_TYPE("(t)"), G_DBUS_CALL_FLAGS_NONE,
        _config.rpc_timeout_ms_, nullptr, &error.value_);
    return vqec_vision_ai_fwctl_amdbs_take_revision(reply, _snapshot_revision);
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
        !g_dbus_is_name(_config.trusted_backend_bus_name_.c_str()) ||
        !g_dbus_is_name(_config.trusted_runtime_bus_name_.c_str()) ||
        _config.trusted_backend_bus_name_ == _config.trusted_runtime_bus_name_ ||
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
    implementation_->trusted_backend_bus_name_ = _config.trusted_backend_bus_name_;
    implementation_->trusted_runtime_bus_name_ = _config.trusted_runtime_bus_name_;
    implementation_->rpc_timeout_ms_ = _config.rpc_timeout_ms_;
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
