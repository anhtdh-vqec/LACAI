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
#include <string_view>
#include <unistd.h>
#include <utility>
#include <vector>

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"
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
    GDBusConnection* authority_connection_{nullptr};
    std::string trusted_backend_bus_name_;
    std::string trusted_runtime_bus_name_;
    int rpc_timeout_ms_{0};
};

constexpr char g_introspection_xml[] =
    "<node><interface name='com.vqec.AiVision.AppManager1'>"
    "<method name='SubmitInstall'><arg type='h' direction='in'/><arg type='h' direction='in'/><arg type='h' direction='in'/><arg type='ah' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='out'/></method>"
    "<method name='SubmitUpdate'><arg type='h' direction='in'/><arg type='h' direction='in'/><arg type='h' direction='in'/><arg type='ah' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='out'/></method>"
    "<method name='SubmitRollback'><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='out'/></method>"
    "<method name='SubmitConfiguration'><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='h' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='out'/></method>"
    "<method name='SubmitEntitlement'><arg type='h' direction='in'/><arg type='h' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='out'/></method>"
    "<method name='SubmitDesired'><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='b' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='out'/></method>"
    "<method name='SubmitUninstall'><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='out'/></method>"
    "<method name='GetOperation'><arg type='s' direction='in'/><arg type='s' direction='out'/><arg type='s' direction='out'/><arg type='s' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='t' direction='out'/><arg type='s' direction='out'/></method>"
    "<method name='CancelOperation'><arg type='s' direction='in'/><arg type='s' direction='out'/><arg type='s' direction='out'/><arg type='s' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='t' direction='out'/><arg type='s' direction='out'/></method>"
    "<method name='GetSnapshot'><arg type='s' direction='out'/></method>"
    "<method name='ListApplications'><arg type='s' direction='in'/><arg type='a(ssssbbbbbbust)' direction='out'/></method>"
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

struct fd_vector_owner {
    std::vector<int> values_;
    ~fd_vector_owner() noexcept {
        for (const int fd : values_) {
            if (fd >= 0) {
                (void)::close(fd);
            }
        }
    }
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

status vqec_vision_ai_fwctl_amdbs_take_operation_id(
    GVariant* _reply, const GError* _error, std::string& _operation_id) {
    if (_reply == nullptr) {
        return {status_code::io_error,
            _error == nullptr || _error->message == nullptr ?
                "App Manager operation submission failed" : _error->message};
    }
    const gchar* operation_id = nullptr;
    g_variant_get(_reply, "(&s)", &operation_id);
    const std::string candidate = operation_id == nullptr ? "" : operation_id;
    g_variant_unref(_reply);
    if (!vqec_vision_ai_cntr_ident_is_valid(
            candidate, app_lifecycle_limits::g_max_identifier_bytes)) {
        return {status_code::protocol_error,
            "App Manager returned an invalid operation identifier"};
    }
    _operation_id = candidate;
    return {};
}

status vqec_vision_ai_fwctl_amdbs_take_operation(
    GVariant* _reply, const GError* _error, app_operation_record& _operation) {
    if (_reply == nullptr) {
        return {status_code::io_error,
            _error == nullptr || _error->message == nullptr ?
                "App Manager operation query failed" : _error->message};
    }
    const gchar* operation_id = nullptr;
    const gchar* app_id = nullptr;
    const gchar* payload_sha256 = nullptr;
    const gchar* result_message = nullptr;
    guint32 kind = 0;
    guint32 state = 0;
    guint32 result_code = 0;
    guint64 snapshot_revision = 0;
    g_variant_get(_reply, "(&s&s&suuut&s)", &operation_id, &app_id,
        &payload_sha256, &kind, &state, &result_code, &snapshot_revision,
        &result_message);
    app_operation_record candidate;
    candidate.operation_id_ = operation_id == nullptr ? "" : operation_id;
    candidate.idempotency_key_ = candidate.operation_id_;
    candidate.app_id_ = app_id == nullptr ? "" : app_id;
    candidate.payload_sha256_ = payload_sha256 == nullptr ? "" : payload_sha256;
    candidate.result_message_ = result_message == nullptr ? "" : result_message;
    candidate.snapshot_revision_ = snapshot_revision;
    g_variant_unref(_reply);
    if (!vqec_vision_ai_cntr_ident_is_valid(candidate.operation_id_,
            app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(candidate.app_id_,
            app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_sha256_hex(candidate.payload_sha256_) ||
        kind > static_cast<guint32>(app_operation_kind::uninstall) ||
        state > static_cast<guint32>(app_operation_state::recovery_required) ||
        result_code > static_cast<guint32>(status_code::pending) ||
        candidate.result_message_.size() >
            app_lifecycle_limits::g_max_operation_message_bytes) {
        return {status_code::protocol_error,
            "App Manager returned an invalid operation record"};
    }
    candidate.kind_ = static_cast<app_operation_kind>(kind);
    candidate.state_ = static_cast<app_operation_state>(state);
    candidate.result_code_ = static_cast<status_code>(result_code);
    _operation = std::move(candidate);
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

void vqec_vision_ai_fwctl_amdbs_return_operation(
    GDBusMethodInvocation* _invocation, const status& _result,
    const app_operation_record& _operation) {
    if (_result.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, _result);
        return;
    }
    g_dbus_method_invocation_return_value(_invocation,
        g_variant_new("(sssuuuts)", _operation.operation_id_.c_str(),
            _operation.app_id_.c_str(), _operation.payload_sha256_.c_str(),
            static_cast<guint32>(_operation.kind_),
            static_cast<guint32>(_operation.state_),
            static_cast<guint32>(_operation.result_code_),
            static_cast<guint64>(_operation.snapshot_revision_),
            _operation.result_message_.c_str()));
}

void vqec_vision_ai_fwctl_amdbs_commit_package(app_manager_port& _port,
    GVariant* _parameters, GDBusMethodInvocation* _invocation,
    bool _is_update) {
    gint32 manifest_handle = -1;
    gint32 configuration_handle = -1;
    gint32 signature_handle = -1;
    GVariant* component_handles = nullptr;
    const gchar* manifest_sha256 = nullptr;
    const gchar* configuration_sha256 = nullptr;
    guint64 expected_revision = 0;
    const gchar* idempotency_key = nullptr;
    const gchar* payload_sha256 = nullptr;
    const gchar* app_id = nullptr;
    g_variant_get(_parameters, "(hhh@ah&s&st&s&s&s)", &manifest_handle,
        &configuration_handle, &signature_handle, &component_handles,
        &manifest_sha256, &configuration_sha256, &expected_revision,
        &idempotency_key, &payload_sha256, &app_id);
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
    fd_vector_owner component_fds;
    const auto component_count = component_handles == nullptr ? 0U :
        g_variant_n_children(component_handles);
    if (current.code_ == status_code::ok &&
        component_count > app_lifecycle_limits::g_max_components) {
        current = {status_code::resource_exhausted,
            "package component descriptor count exceeds limit"};
    }
    if (current.code_ == status_code::ok) {
        GDBusMessage* message = g_dbus_method_invocation_get_message(_invocation);
        GUnixFDList* list = message == nullptr ? nullptr :
            g_dbus_message_get_unix_fd_list(message);
        if (list == nullptr && component_count != 0) {
            current = {status_code::invalid_argument,
                "package component descriptors are unavailable"};
        } else {
            component_fds.values_.reserve(component_count);
            candidate.components_.reserve(component_count);
            for (gsize index = 0; index < component_count; ++index) {
                gint32 handle = -1;
                g_variant_get_child(component_handles, index, "h", &handle);
                error_owner error;
                const int fd = g_unix_fd_list_get(list, handle, &error.value_);
                if (fd < 0) {
                    current = {status_code::invalid_argument,
                        "package component descriptor is invalid"};
                    break;
                }
                component_fds.values_.push_back(fd);
                candidate.components_.push_back({fd});
            }
        }
    }
    if (component_handles != nullptr) {
        g_variant_unref(component_handles);
    }
    if (current.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, current);
        return;
    }
    candidate.manifest_sha256_ = manifest_sha256 == nullptr ? "" : manifest_sha256;
    candidate.configuration_sha256_ = configuration_sha256 == nullptr ?
        "" : configuration_sha256;
    app_operation_request request;
    request.idempotency_key_ = idempotency_key == nullptr ? "" : idempotency_key;
    request.payload_sha256_ = payload_sha256 == nullptr ? "" : payload_sha256;
    request.app_id_ = app_id == nullptr ? "" : app_id;
    request.kind_ = _is_update ? app_operation_kind::update :
        app_operation_kind::install;
    app_operation_record operation;
    const auto submitted = _is_update ?
        _port.vqec_vision_ai_ports_apmgr_submit_update(
            request, candidate, expected_revision, operation) :
        _port.vqec_vision_ai_ports_apmgr_submit_install(
            request, candidate, expected_revision, operation);
    if (submitted.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, submitted);
        return;
    }
    g_dbus_method_invocation_return_value(_invocation,
        g_variant_new("(s)", operation.operation_id_.c_str()));
}

void vqec_vision_ai_fwctl_amdbs_configuration(app_manager_port& _port,
    GVariant* _parameters, GDBusMethodInvocation* _invocation) {
    const gchar* app_id = nullptr;
    guint64 expected_revision = 0;
    gint32 configuration_handle = -1;
    const gchar* sha256 = nullptr;
    const gchar* idempotency_key = nullptr;
    const gchar* payload_sha256 = nullptr;
    g_variant_get(_parameters, "(&sth&s&s&s)", &app_id, &expected_revision,
        &configuration_handle, &sha256, &idempotency_key, &payload_sha256);
    std::vector<std::uint8_t> payload;
    const auto read = vqec_vision_ai_fwctl_amdbs_read_fd(_invocation,
        configuration_handle, app_lifecycle_limits::g_max_document_bytes, payload);
    if (read.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, read);
        return;
    }
    app_operation_request request;
    request.idempotency_key_ = idempotency_key == nullptr ? "" : idempotency_key;
    request.payload_sha256_ = payload_sha256 == nullptr ? "" : payload_sha256;
    request.app_id_ = app_id == nullptr ? "" : app_id;
    request.kind_ = app_operation_kind::configure;
    app_operation_record operation;
    const auto submitted = _port.vqec_vision_ai_ports_apmgr_submit_configuration(
        request, expected_revision, payload, sha256 == nullptr ? "" : sha256,
        operation);
    if (submitted.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, submitted);
        return;
    }
    g_dbus_method_invocation_return_value(_invocation,
        g_variant_new("(s)", operation.operation_id_.c_str()));
}

void vqec_vision_ai_fwctl_amdbs_entitlement(app_manager_port& _port,
    GVariant* _parameters, GDBusMethodInvocation* _invocation) {
    gint32 grant_handle = -1;
    gint32 signature_handle = -1;
    const gchar* grant_sha256 = nullptr;
    const gchar* app_id = nullptr;
    const gchar* idempotency_key = nullptr;
    const gchar* payload_sha256 = nullptr;
    g_variant_get(_parameters, "(hh&s&s&s&s)", &grant_handle, &signature_handle,
        &grant_sha256, &app_id, &idempotency_key, &payload_sha256);
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
    app_operation_request request;
    request.idempotency_key_ = idempotency_key == nullptr ? "" : idempotency_key;
    request.payload_sha256_ = payload_sha256 == nullptr ? "" : payload_sha256;
    request.app_id_ = app_id == nullptr ? "" : app_id;
    request.kind_ = app_operation_kind::entitlement;
    app_operation_record operation;
    const auto submitted = _port.vqec_vision_ai_ports_apmgr_submit_entitlement(
        request, candidate, operation);
    if (submitted.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, submitted);
        return;
    }
    g_dbus_method_invocation_return_value(_invocation,
        g_variant_new("(s)", operation.operation_id_.c_str()));
}

void vqec_vision_ai_fwctl_amdbs_desired(app_manager_port& _port,
    GVariant* _parameters, GDBusMethodInvocation* _invocation) {
    const gchar* app_id = nullptr;
    const gchar* source_id = nullptr;
    guint64 expected_revision = 0;
    gboolean desired = FALSE;
    const gchar* idempotency_key = nullptr;
    const gchar* payload_sha256 = nullptr;
    g_variant_get(_parameters, "(&s&stb&s&s)", &app_id, &source_id,
        &expected_revision, &desired, &idempotency_key, &payload_sha256);
    app_desired_update update{app_id == nullptr ? "" : app_id,
        source_id == nullptr ? "" : source_id, expected_revision,
        desired != FALSE};
    app_operation_request request;
    request.idempotency_key_ = idempotency_key == nullptr ? "" : idempotency_key;
    request.payload_sha256_ = payload_sha256 == nullptr ? "" : payload_sha256;
    request.app_id_ = app_id == nullptr ? "" : app_id;
    request.kind_ = app_operation_kind::desired;
    app_operation_record operation;
    const auto submitted = _port.vqec_vision_ai_ports_apmgr_submit_desired(
        request, update, operation);
    if (submitted.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, submitted);
        return;
    }
    g_dbus_method_invocation_return_value(_invocation,
        g_variant_new("(s)", operation.operation_id_.c_str()));
}

void vqec_vision_ai_fwctl_amdbs_uninstall(app_manager_port& _port,
    GVariant* _parameters, GDBusMethodInvocation* _invocation) {
    const gchar* app_id = nullptr;
    guint64 expected_revision = 0;
    const gchar* idempotency_key = nullptr;
    const gchar* payload_sha256 = nullptr;
    g_variant_get(_parameters, "(&st&s&s)", &app_id, &expected_revision,
        &idempotency_key, &payload_sha256);
    app_operation_request request;
    request.idempotency_key_ = idempotency_key == nullptr ? "" : idempotency_key;
    request.payload_sha256_ = payload_sha256 == nullptr ? "" : payload_sha256;
    request.app_id_ = app_id == nullptr ? "" : app_id;
    request.kind_ = app_operation_kind::uninstall;
    app_operation_record operation;
    const auto submitted = _port.vqec_vision_ai_ports_apmgr_submit_uninstall(
        request, expected_revision, operation);
    if (submitted.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, submitted);
        return;
    }
    g_dbus_method_invocation_return_value(_invocation,
        g_variant_new("(s)", operation.operation_id_.c_str()));
}

void vqec_vision_ai_fwctl_amdbs_submit_rollback(app_manager_port& _port,
    GVariant* _parameters, GDBusMethodInvocation* _invocation) {
    const gchar* app_id = nullptr;
    guint64 expected_revision = 0;
    const gchar* idempotency_key = nullptr;
    const gchar* payload_sha256 = nullptr;
    g_variant_get(_parameters, "(&st&s&s)", &app_id, &expected_revision,
        &idempotency_key, &payload_sha256);
    app_operation_request request;
    request.idempotency_key_ = idempotency_key == nullptr ? "" : idempotency_key;
    request.payload_sha256_ = payload_sha256 == nullptr ? "" : payload_sha256;
    request.app_id_ = app_id == nullptr ? "" : app_id;
    request.kind_ = app_operation_kind::rollback;
    app_operation_record operation;
    const auto submitted = _port.vqec_vision_ai_ports_apmgr_submit_rollback(
        request, expected_revision, operation);
    if (submitted.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, submitted);
        return;
    }
    g_dbus_method_invocation_return_value(_invocation,
        g_variant_new("(s)", operation.operation_id_.c_str()));
}

void vqec_vision_ai_fwctl_amdbs_operation(app_manager_port& _port,
    GVariant* _parameters, GDBusMethodInvocation* _invocation,
    bool _cancel) {
    const gchar* operation_id = nullptr;
    g_variant_get(_parameters, "(&s)", &operation_id);
    app_operation_record operation;
    const auto result = _cancel ?
        _port.vqec_vision_ai_ports_apmgr_cancel_operation(
            operation_id == nullptr ? "" : operation_id, operation) :
        _port.vqec_vision_ai_ports_apmgr_get_operation(
            operation_id == nullptr ? "" : operation_id, operation);
    vqec_vision_ai_fwctl_amdbs_return_operation(_invocation, result, operation);
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

void vqec_vision_ai_fwctl_amdbs_list_applications(
    app_manager_port& _port, GVariant* _parameters,
    GDBusMethodInvocation* _invocation) {
    const gchar* source_id = nullptr;
    g_variant_get(_parameters, "(&s)", &source_id);
    std::vector<app_catalog_status> applications;
    const auto queried = _port.vqec_vision_ai_ports_apmgr_list_applications(
        source_id == nullptr ? "" : source_id, applications);
    if (queried.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_amdbs_return_error(_invocation, queried);
        return;
    }
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ssssbbbbbbust)"));
    for (const auto& item : applications) {
        g_variant_builder_add(&builder, "(ssssbbbbbbust)",
            item.catalog_.catalog_code_.c_str(), item.catalog_.app_id_.c_str(),
            item.catalog_.display_name_.c_str(), item.catalog_.app_version_.c_str(),
            static_cast<gboolean>(item.catalog_.published_),
            static_cast<gboolean>(item.supported_),
            static_cast<gboolean>(item.installed_),
            static_cast<gboolean>(item.entitled_),
            static_cast<gboolean>(item.desired_),
            static_cast<gboolean>(item.effective_),
            static_cast<guint32>(item.state_), item.reason_code_.c_str(),
            static_cast<guint64>(item.snapshot_revision_));
    }
    g_dbus_method_invocation_return_value(_invocation,
        g_variant_new("(@a(ssssbbbbbbust))", g_variant_builder_end(&builder)));
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

void vqec_vision_ai_fwctl_amdbs_method_call(GDBusConnection*,
    const gchar* _sender, const gchar*, const gchar*, const gchar* _method_name,
    GVariant* _parameters, GDBusMethodInvocation* _invocation,
    gpointer _user_data) {
    auto* binding = static_cast<dbus_binding*>(_user_data);
    const bool is_snapshot = g_strcmp0(
        _method_name, app_manager_dbus_protocol::g_snapshot_method) == 0;
    // Snapshot polling is the runtime's only method. Resolve that expected owner
    // first so an offline backend cannot delay runtime startup or reconnection.
    const bool is_runtime = binding != nullptr && is_snapshot &&
        vqec_vision_ai_fwctl_amdbs_is_named_sender(binding->authority_connection_,
            binding->trusted_runtime_bus_name_, binding->rpc_timeout_ms_, _sender);
    const bool is_backend = binding != nullptr && !is_runtime &&
        vqec_vision_ai_fwctl_amdbs_is_named_sender(binding->authority_connection_,
            binding->trusted_backend_bus_name_, binding->rpc_timeout_ms_, _sender);
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
        if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_submit_install_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_commit_package(
                *binding->port_, _parameters, _invocation, false);
        } else if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_submit_update_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_commit_package(
                *binding->port_, _parameters, _invocation, true);
        } else if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_submit_rollback_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_submit_rollback(
                *binding->port_, _parameters, _invocation);
        } else if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_get_operation_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_operation(
                *binding->port_, _parameters, _invocation, false);
        } else if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_cancel_operation_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_operation(
                *binding->port_, _parameters, _invocation, true);
        } else if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_submit_configuration_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_configuration(
                *binding->port_, _parameters, _invocation);
        } else if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_submit_entitlement_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_entitlement(
                *binding->port_, _parameters, _invocation);
        } else if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_submit_desired_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_desired(
                *binding->port_, _parameters, _invocation);
        } else if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_submit_uninstall_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_uninstall(
                *binding->port_, _parameters, _invocation);
        } else if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_snapshot_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_snapshot(*binding->port_, _invocation);
        } else if (g_strcmp0(_method_name,
                       app_manager_dbus_protocol::g_list_applications_method) == 0) {
            vqec_vision_ai_fwctl_amdbs_list_applications(
                *binding->port_, _parameters, _invocation);
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

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_list_applications(
    const app_manager_dbus_client_config& _config,
    const std::string& _source_id,
    std::vector<app_catalog_status>& _applications) {
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _source_id, app_lifecycle_limits::g_max_identifier_bytes)) {
        return {status_code::invalid_argument,
            "invalid App Manager catalog source identity"};
    }
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
        app_manager_dbus_protocol::g_list_applications_method,
        g_variant_new("(s)", _source_id.c_str()),
        G_VARIANT_TYPE("(a(ssssbbbbbbust))"), G_DBUS_CALL_FLAGS_NONE,
        _config.rpc_timeout_ms_, nullptr, &error.value_);
    if (reply == nullptr) {
        return {status_code::io_error, "App Manager ListApplications failed"};
    }
    GVariant* entries = nullptr;
    g_variant_get(reply, "(@a(ssssbbbbbbust))", &entries);
    g_variant_unref(reply);
    if (entries == nullptr ||
        g_variant_n_children(entries) > app_lifecycle_limits::g_max_applications) {
        if (entries != nullptr) {
            g_variant_unref(entries);
        }
        return {status_code::protocol_error,
            "App Manager catalog exceeds application limit"};
    }
    std::vector<app_catalog_status> applications;
    usecase_app_catalog catalog;
    catalog.schema_version_ = app_lifecycle_limits::g_schema_version;
    catalog.catalog_id_ = "dbus.app_catalog";
    catalog.revision_ = 1;
    try {
        applications.reserve(g_variant_n_children(entries));
        catalog.applications_.reserve(g_variant_n_children(entries));
        GVariantIter iterator;
        g_variant_iter_init(&iterator, entries);
        const gchar* catalog_code = nullptr;
        const gchar* app_id = nullptr;
        const gchar* display_name = nullptr;
        const gchar* app_version = nullptr;
        gboolean published = FALSE;
        gboolean supported = FALSE;
        gboolean installed = FALSE;
        gboolean entitled = FALSE;
        gboolean desired = FALSE;
        gboolean effective = FALSE;
        guint32 state = 0;
        const gchar* reason_code = nullptr;
        guint64 snapshot_revision = 0;
        while (g_variant_iter_next(&iterator, "(&s&s&s&sbbbbbbu&st)",
                   &catalog_code, &app_id, &display_name, &app_version,
                   &published, &supported, &installed, &entitled, &desired,
                   &effective, &state, &reason_code, &snapshot_revision)) {
            app_catalog_status item;
            item.catalog_ = {catalog_code == nullptr ? "" : catalog_code,
                app_id == nullptr ? "" : app_id,
                display_name == nullptr ? "" : display_name,
                app_version == nullptr ? "" : app_version, published != FALSE};
            item.source_id_ = _source_id;
            item.supported_ = supported != FALSE;
            item.installed_ = installed != FALSE;
            item.entitled_ = entitled != FALSE;
            item.desired_ = desired != FALSE;
            item.effective_ = effective != FALSE;
            item.reason_code_ = reason_code == nullptr ? "" : reason_code;
            item.snapshot_revision_ = snapshot_revision;
            if (state > static_cast<guint32>(app_install_state::faulted) ||
                !vqec_vision_ai_cntr_ident_is_valid(item.reason_code_,
                    app_lifecycle_limits::g_max_identifier_bytes) ||
                item.snapshot_revision_ == 0) {
                g_variant_unref(entries);
                return {status_code::protocol_error,
                    "App Manager returned invalid catalog status"};
            }
            item.state_ = static_cast<app_install_state>(state);
            catalog.applications_.push_back(item.catalog_);
            applications.push_back(std::move(item));
        }
    } catch (const std::bad_alloc&) {
        g_variant_unref(entries);
        return {status_code::resource_exhausted,
            "cannot allocate App Manager catalog response"};
    }
    g_variant_unref(entries);
    current = vqec_vision_ai_core_applc_validate_catalog(catalog);
    if (current.code_ != status_code::ok) {
        return {status_code::protocol_error, current.message_};
    }
    _applications = std::move(applications);
    return {};
}

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_submit_install(
    const app_manager_dbus_client_config& _config,
    const app_operation_request& _operation_request,
    const app_package_candidate& _candidate,
    std::uint64_t _expected_inventory_revision,
    std::string& _operation_id) {
    return vqec_vision_ai_fwctl_amdbs_commit_package(_config, _candidate,
        _expected_inventory_revision,
        app_manager_dbus_protocol::g_submit_install_method,
        _operation_request, _operation_id);
}

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_submit_update(
    const app_manager_dbus_client_config& _config,
    const app_operation_request& _operation_request,
    const app_package_candidate& _candidate,
    std::uint64_t _expected_inventory_revision,
    std::string& _operation_id) {
    return vqec_vision_ai_fwctl_amdbs_commit_package(_config, _candidate,
        _expected_inventory_revision,
        app_manager_dbus_protocol::g_submit_update_method,
        _operation_request, _operation_id);
}

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_commit_package(
    const app_manager_dbus_client_config& _config,
    const app_package_candidate& _candidate,
    std::uint64_t _expected_inventory_revision,
    const char* _method_name,
    const app_operation_request& _operation_request,
    std::string& _operation_id) {
    const std::string_view method = _method_name == nullptr ? "" : _method_name;
    const bool is_submit = method == app_manager_dbus_protocol::g_submit_install_method ||
        method == app_manager_dbus_protocol::g_submit_update_method;
    const app_operation_kind expected_kind =
        method == app_manager_dbus_protocol::g_submit_update_method ?
            app_operation_kind::update : app_operation_kind::install;
    if (!is_submit ||
        _expected_inventory_revision == 0 ||
        _candidate.components_.size() > app_lifecycle_limits::g_max_components ||
        _operation_request.kind_ != expected_kind ||
            !vqec_vision_ai_cntr_ident_is_valid(
                _operation_request.idempotency_key_,
                app_lifecycle_limits::g_max_identifier_bytes) ||
            !vqec_vision_ai_cntr_ident_is_sha256_hex(
                _operation_request.payload_sha256_) ||
            !vqec_vision_ai_cntr_ident_is_valid(_operation_request.app_id_,
                app_lifecycle_limits::g_max_identifier_bytes)) {
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
    GVariantBuilder component_builder;
    g_variant_builder_init(&component_builder, G_VARIANT_TYPE("ah"));
    bool components_attached = signature_handle >= 0;
    for (const auto& component : _candidate.components_) {
        if (!components_attached || component.descriptor_ < 0) {
            components_attached = false;
            break;
        }
        const int handle = g_unix_fd_list_append(
            descriptors, component.descriptor_, &error.value_);
        if (handle < 0) {
            components_attached = false;
            break;
        }
        g_variant_builder_add(&component_builder, "h", handle);
    }
    if (!components_attached) {
        g_object_unref(descriptors);
        return {status_code::io_error,
            "cannot attach App Manager package descriptors"};
    }
    GVariant* parameters = g_variant_new("(hhh@ahsstsss)", manifest_handle,
        configuration_handle, signature_handle,
        g_variant_builder_end(&component_builder),
        _candidate.manifest_sha256_.c_str(),
        _candidate.configuration_sha256_.c_str(),
        static_cast<guint64>(_expected_inventory_revision),
        _operation_request.idempotency_key_.c_str(),
        _operation_request.payload_sha256_.c_str(),
        _operation_request.app_id_.c_str());
    GVariant* reply = g_dbus_connection_call_with_unix_fd_list_sync(
        implementation_->connection_, unique_owner.c_str(), _config.object_path_.c_str(),
        app_manager_dbus_protocol::g_interface_name,
        _method_name, parameters, G_VARIANT_TYPE("(s)"),
        G_DBUS_CALL_FLAGS_NONE, _config.rpc_timeout_ms_,
        descriptors, nullptr, nullptr, &error.value_);
    g_object_unref(descriptors);
    return vqec_vision_ai_fwctl_amdbs_take_operation_id(
        reply, error.value_, _operation_id);
}

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_submit_rollback(
    const app_manager_dbus_client_config& _config,
    const app_operation_request& _operation_request,
    std::uint64_t _expected_inventory_revision,
    std::string& _operation_id) {
    if (_operation_request.kind_ != app_operation_kind::rollback ||
        !vqec_vision_ai_cntr_ident_is_valid(_operation_request.app_id_,
            app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(_operation_request.idempotency_key_,
            app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_sha256_hex(
            _operation_request.payload_sha256_) ||
        _expected_inventory_revision == 0) {
        return {status_code::invalid_argument,
            "invalid app rollback operation submission"};
    }
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
        app_manager_dbus_protocol::g_submit_rollback_method,
        g_variant_new("(stss)", _operation_request.app_id_.c_str(),
            static_cast<guint64>(_expected_inventory_revision),
            _operation_request.idempotency_key_.c_str(),
            _operation_request.payload_sha256_.c_str()),
        G_VARIANT_TYPE("(s)"), G_DBUS_CALL_FLAGS_NONE,
        _config.rpc_timeout_ms_, nullptr, &error.value_);
    return vqec_vision_ai_fwctl_amdbs_take_operation_id(
        reply, error.value_, _operation_id);
}

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_get_operation(
    const app_manager_dbus_client_config& _config,
    const std::string& _operation_id,
    app_operation_record& _operation) {
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _operation_id, app_lifecycle_limits::g_max_identifier_bytes)) {
        return {status_code::invalid_argument, "invalid app operation query"};
    }
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
        app_manager_dbus_protocol::g_get_operation_method,
        g_variant_new("(s)", _operation_id.c_str()),
        G_VARIANT_TYPE("(sssuuuts)"), G_DBUS_CALL_FLAGS_NONE,
        _config.rpc_timeout_ms_, nullptr, &error.value_);
    return vqec_vision_ai_fwctl_amdbs_take_operation(
        reply, error.value_, _operation);
}

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_cancel_operation(
    const app_manager_dbus_client_config& _config,
    const std::string& _operation_id,
    app_operation_record& _operation) {
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _operation_id, app_lifecycle_limits::g_max_identifier_bytes)) {
        return {status_code::invalid_argument, "invalid app operation cancellation"};
    }
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
        app_manager_dbus_protocol::g_cancel_operation_method,
        g_variant_new("(s)", _operation_id.c_str()),
        G_VARIANT_TYPE("(sssuuuts)"), G_DBUS_CALL_FLAGS_NONE,
        _config.rpc_timeout_ms_, nullptr, &error.value_);
    return vqec_vision_ai_fwctl_amdbs_take_operation(
        reply, error.value_, _operation);
}

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_submit_entitlement(
    const app_manager_dbus_client_config& _config,
    const app_operation_request& _operation_request,
    const app_entitlement_candidate& _candidate,
    std::string& _operation_id) {
    if (_operation_request.kind_ != app_operation_kind::entitlement ||
        _operation_request.payload_sha256_ != _candidate.grant_sha256_ ||
        !vqec_vision_ai_cntr_ident_is_valid(_operation_request.app_id_,
            app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(_operation_request.idempotency_key_,
            app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_sha256_hex(
            _operation_request.payload_sha256_)) {
        return {status_code::invalid_argument,
            "invalid entitlement operation submission"};
    }
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
        app_manager_dbus_protocol::g_submit_entitlement_method,
        g_variant_new("(hhssss)", grant_handle, signature_handle,
            _candidate.grant_sha256_.c_str(), _operation_request.app_id_.c_str(),
            _operation_request.idempotency_key_.c_str(),
            _operation_request.payload_sha256_.c_str()), G_VARIANT_TYPE("(s)"),
        G_DBUS_CALL_FLAGS_NONE, _config.rpc_timeout_ms_, descriptors, nullptr,
        nullptr, &error.value_);
    g_object_unref(descriptors);
    return vqec_vision_ai_fwctl_amdbs_take_operation_id(
        reply, error.value_, _operation_id);
}

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_submit_configuration(
    const app_manager_dbus_client_config& _config,
    const app_operation_request& _operation_request,
    std::uint64_t _expected_configuration_revision,
    const std::vector<std::uint8_t>& _configuration_payload,
    const std::string& _configuration_sha256,
    std::string& _operation_id) {
    if (_operation_request.kind_ != app_operation_kind::configure ||
        _operation_request.payload_sha256_ != _configuration_sha256 ||
        !vqec_vision_ai_cntr_ident_is_valid(_operation_request.app_id_,
            app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(_operation_request.idempotency_key_,
            app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_sha256_hex(
            _operation_request.payload_sha256_)) {
        return {status_code::invalid_argument,
            "invalid configuration operation submission"};
    }
    std::string unique_owner;
    auto current = implementation_->vqec_vision_ai_fwctl_amdbs_prepare(
        _config, unique_owner);
    if (current.code_ != status_code::ok) {
        return current;
    }
    fd_owner configuration;
    current = vqec_vision_ai_fwctl_amdbs_make_payload_fd(
        _configuration_payload, configuration);
    if (current.code_ != status_code::ok) {
        return current;
    }
    error_owner error;
    GUnixFDList* descriptors = g_unix_fd_list_new();
    const int configuration_handle = g_unix_fd_list_append(
        descriptors, configuration.value_, &error.value_);
    if (configuration_handle < 0) {
        g_object_unref(descriptors);
        return {status_code::io_error,
            "cannot attach App Manager configuration descriptor"};
    }
    GVariant* reply = g_dbus_connection_call_with_unix_fd_list_sync(
        implementation_->connection_, unique_owner.c_str(), _config.object_path_.c_str(),
        app_manager_dbus_protocol::g_interface_name,
        app_manager_dbus_protocol::g_submit_configuration_method,
        g_variant_new("(sthsss)", _operation_request.app_id_.c_str(),
            static_cast<guint64>(_expected_configuration_revision),
            configuration_handle, _configuration_sha256.c_str(),
            _operation_request.idempotency_key_.c_str(),
            _operation_request.payload_sha256_.c_str()),
        G_VARIANT_TYPE("(s)"), G_DBUS_CALL_FLAGS_NONE, _config.rpc_timeout_ms_,
        descriptors, nullptr, nullptr, &error.value_);
    g_object_unref(descriptors);
    return vqec_vision_ai_fwctl_amdbs_take_operation_id(
        reply, error.value_, _operation_id);
}

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_submit_desired(
    const app_manager_dbus_client_config& _config,
    const app_operation_request& _operation_request,
    const app_desired_update& _update,
    std::string& _operation_id) {
    if (_operation_request.kind_ != app_operation_kind::desired ||
        _operation_request.app_id_ != _update.app_id_ ||
        !vqec_vision_ai_cntr_ident_is_valid(_operation_request.app_id_,
            app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(_operation_request.idempotency_key_,
            app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_sha256_hex(
            _operation_request.payload_sha256_)) {
        return {status_code::invalid_argument,
            "invalid desired-state operation submission"};
    }
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
        app_manager_dbus_protocol::g_submit_desired_method,
        g_variant_new("(sstbss)", _update.app_id_.c_str(), _update.source_id_.c_str(),
            static_cast<guint64>(_update.expected_desired_revision_),
            _update.desired_ ? TRUE : FALSE,
            _operation_request.idempotency_key_.c_str(),
            _operation_request.payload_sha256_.c_str()), G_VARIANT_TYPE("(s)"),
        G_DBUS_CALL_FLAGS_NONE, _config.rpc_timeout_ms_, nullptr, &error.value_);
    return vqec_vision_ai_fwctl_amdbs_take_operation_id(
        reply, error.value_, _operation_id);
}

status app_manager_dbus_client::vqec_vision_ai_fwctl_amdbs_submit_uninstall(
    const app_manager_dbus_client_config& _config,
    const app_operation_request& _operation_request,
    std::uint64_t _expected_inventory_revision,
    std::string& _operation_id) {
    if (_operation_request.kind_ != app_operation_kind::uninstall ||
        !vqec_vision_ai_cntr_ident_is_valid(_operation_request.app_id_,
            app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(_operation_request.idempotency_key_,
            app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_sha256_hex(
            _operation_request.payload_sha256_) ||
        _expected_inventory_revision == 0) {
        return {status_code::invalid_argument,
            "invalid uninstall operation submission"};
    }
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
        app_manager_dbus_protocol::g_submit_uninstall_method,
        g_variant_new("(stss)", _operation_request.app_id_.c_str(),
            static_cast<guint64>(_expected_inventory_revision),
            _operation_request.idempotency_key_.c_str(),
            _operation_request.payload_sha256_.c_str()),
        G_VARIANT_TYPE("(s)"), G_DBUS_CALL_FLAGS_NONE,
        _config.rpc_timeout_ms_, nullptr, &error.value_);
    return vqec_vision_ai_fwctl_amdbs_take_operation_id(
        reply, error.value_, _operation_id);
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
        if (authority_connection_ != nullptr) {
            g_dbus_connection_close(
                authority_connection_, nullptr, nullptr, nullptr);
            g_object_unref(authority_connection_);
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
    if (implementation_->connection_ != nullptr) {
        implementation_->authority_connection_ =
            g_dbus_connection_new_for_address_sync(address,
                static_cast<GDBusConnectionFlags>(
                    G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                    G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
                nullptr, nullptr, &error.value_);
    }
    g_free(address);
    if (implementation_->connection_ == nullptr ||
        implementation_->authority_connection_ == nullptr) {
        return {status_code::io_error, "cannot connect to app manager DBus"};
    }
    g_dbus_connection_set_exit_on_close(implementation_->connection_, FALSE);
    g_dbus_connection_set_exit_on_close(
        implementation_->authority_connection_, FALSE);
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
