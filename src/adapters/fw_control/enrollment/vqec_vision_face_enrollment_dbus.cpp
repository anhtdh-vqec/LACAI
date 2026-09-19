#include "vqec_vision_face_enrollment_dbus.hpp"

#include <gio/gio.h>

#include <limits>
#include <new>
#include <utility>

namespace vqec::vision::ai {
namespace {

// org.freedesktop.DBus protocol values, independent of LACAI C++ enum layout.
constexpr guint g_request_name_do_not_queue = 4U;
constexpr guint g_request_name_primary_owner = 1U;
constexpr guint g_request_name_already_owner = 4U;
constexpr std::size_t g_max_callbacks_ceiling = 64;
constexpr gsize g_max_request_wire_bytes = 4096;

struct dbus_binding {
    face_enrollment_port* port_{nullptr};
    std::string trusted_sender_;
};

constexpr char g_introspection_xml[] =
    "<node><interface name='com.vqec.Lacai.FaceEnrollment1'>"
    "<method name='BeginEnrollment'><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='u' direction='in'/><arg type='u' direction='in'/><arg type='t' direction='in'/><arg type='u' direction='in'/><arg type='t' direction='in'/><arg type='s' direction='out'/><arg type='s' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='t' direction='out'/><arg type='i' direction='out'/></method>"
    "<method name='CancelEnrollment'><arg type='s' direction='in'/><arg type='s' direction='out'/><arg type='s' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='t' direction='out'/><arg type='i' direction='out'/></method>"
    "<method name='RemoveSubject'><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='t' direction='out'/><arg type='i' direction='out'/></method>"
    "<method name='GetEnrollmentStatus'><arg type='s' direction='in'/><arg type='s' direction='out'/><arg type='s' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='t' direction='out'/><arg type='i' direction='out'/></method>"
    "<method name='GetGalleryStatus'><arg type='t' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='b' direction='out'/><arg type='b' direction='out'/></method>"
    "</interface></node>";

struct error_owner {
    GError* value_{nullptr};
    ~error_owner() noexcept { if (value_ != nullptr) { g_error_free(value_); } }
};

struct node_owner {
    GDBusNodeInfo* value_{nullptr};
    ~node_owner() noexcept { if (value_ != nullptr) { g_dbus_node_info_unref(value_); } }
};

status vqec_vision_ai_fwctl_fedbs_status_reply(
    const status& _result, GDBusMethodInvocation* _invocation) {
    if (_result.code_ == status_code::ok) {
        return {};
    }
    g_dbus_method_invocation_return_error(
        _invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "%s", _result.message_.c_str());
    return _result;
}

guint vqec_vision_ai_fwctl_fedbs_state_value(face_enrollment_state _state) noexcept {
    switch (_state) {
    case face_enrollment_state::idle: return 0;
    case face_enrollment_state::collecting: return 1;
    case face_enrollment_state::completed: return 2;
    case face_enrollment_state::cancelled: return 3;
    case face_enrollment_state::failed: return 4;
    }
    return 4;
}

gint vqec_vision_ai_fwctl_fedbs_error_value(status_code _code) noexcept {
    switch (_code) {
    case status_code::ok: return 0;
    case status_code::invalid_argument: return 1;
    case status_code::unsupported: return 2;
    case status_code::missing_plugin: return 3;
    case status_code::incompatible_plugin: return 4;
    case status_code::graph_link_failed: return 5;
    case status_code::timeout: return 6;
    case status_code::source_lost: return 7;
    case status_code::protocol_error: return 8;
    case status_code::resource_exhausted: return 9;
    case status_code::unauthorized: return 10;
    case status_code::io_error: return 11;
    case status_code::invalid_state: return 12;
    case status_code::pending: return 13;
    }
    return 8;
}

void vqec_vision_ai_fwctl_fedbs_method_call(
    GDBusConnection* _connection, const gchar* _sender, const gchar* _object_path,
    const gchar* _interface_name, const gchar* _method_name, GVariant* _parameters,
    GDBusMethodInvocation* _invocation, gpointer _user_data) {
    (void)_connection;
    (void)_object_path;
    (void)_interface_name;
    auto* binding = static_cast<dbus_binding*>(_user_data);
    if (binding == nullptr || _sender == nullptr ||
        binding->trusted_sender_ != _sender) {
        g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
            G_DBUS_ERROR_ACCESS_DENIED, "enrollment caller is not the configured FW peer");
        return;
    }
    if (g_variant_get_size(_parameters) > g_max_request_wire_bytes) {
        g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
            G_DBUS_ERROR_INVALID_ARGS, "enrollment request exceeds wire byte limit");
        return;
    }
    auto* port = binding->port_;
    if (port == nullptr) {
        g_dbus_method_invocation_return_error_literal(
            _invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "enrollment port unavailable");
        return;
    }
    try {
    face_enrollment_status enrollment_status;
    status result;
    if (g_strcmp0(_method_name, face_enrollment_dbus_protocol::g_begin_method) == 0) {
        const gchar* request_id = nullptr;
        const gchar* subject_ref = nullptr;
        const gchar* image_path = nullptr;
        const gchar* source_id = nullptr;
        guint camera_id = 0;
        guint channel_id = 0;
        guint64 target_track_id = 0;
        guint expected_samples = 0;
        guint64 expected_revision = 0;
        g_variant_get(_parameters, "(&s&s&s&suutut)", &request_id, &subject_ref,
            &image_path, &source_id,
            &camera_id, &channel_id, &target_track_id, &expected_samples, &expected_revision);
        face_enrollment_begin_request request;
        request.request_id_ = request_id == nullptr ? "" : request_id;
        request.subject_ref_ = subject_ref == nullptr ? "" : subject_ref;
        request.image_path_ = image_path == nullptr ? "" : image_path;
        request.source_id_ = source_id == nullptr ? "" : source_id;
        request.camera_id_ = camera_id;
        request.channel_id_ = channel_id;
        request.target_track_id_ = target_track_id;
        request.expected_samples_ = expected_samples;
        request.expected_gallery_revision_ = expected_revision;
        result = port->vqec_vision_ai_ports_fenrl_begin(request, enrollment_status);
        if (vqec_vision_ai_fwctl_fedbs_status_reply(result, _invocation).code_ != status_code::ok) {
            return;
        }
    } else if (g_strcmp0(_method_name, face_enrollment_dbus_protocol::g_cancel_method) == 0) {
        const gchar* request_id = nullptr;
        g_variant_get(_parameters, "(&s)", &request_id);
        result = port->vqec_vision_ai_ports_fenrl_cancel(
            request_id == nullptr ? "" : request_id, enrollment_status);
        if (vqec_vision_ai_fwctl_fedbs_status_reply(result, _invocation).code_ != status_code::ok) {
            return;
        }
    } else if (g_strcmp0(_method_name, face_enrollment_dbus_protocol::g_remove_method) == 0) {
        const gchar* subject_ref = nullptr;
        guint64 expected_revision = 0;
        g_variant_get(_parameters, "(&st)", &subject_ref, &expected_revision);
        guint64 new_revision = 0;
        result = port->vqec_vision_ai_ports_fenrl_remove_subject(
            subject_ref == nullptr ? "" : subject_ref, expected_revision, new_revision);
        if (result.code_ != status_code::ok) {
            vqec_vision_ai_fwctl_fedbs_status_reply(result, _invocation);
            return;
        }
        g_dbus_method_invocation_return_value(_invocation, g_variant_new("(ti)", new_revision, 0));
        return;
    } else if (g_strcmp0(_method_name,
                   face_enrollment_dbus_protocol::g_gallery_status_method) == 0) {
        face_gallery_status gallery_status;
        result = port->vqec_vision_ai_ports_fenrl_get_gallery_status(gallery_status);
        if (vqec_vision_ai_fwctl_fedbs_status_reply(
                result, _invocation).code_ != status_code::ok) {
            return;
        }
        g_dbus_method_invocation_return_value(_invocation,
            g_variant_new("(tuubb)", gallery_status.gallery_revision_,
                static_cast<guint>(gallery_status.subject_count_),
                static_cast<guint>(gallery_status.template_count_),
                gallery_status.is_available_, gallery_status.is_faulted_));
        return;
    } else if (g_strcmp0(_method_name, face_enrollment_dbus_protocol::g_status_method) == 0) {
        const gchar* request_id = nullptr;
        g_variant_get(_parameters, "(&s)", &request_id);
        result = port->vqec_vision_ai_ports_fenrl_get_status(
            request_id == nullptr ? "" : request_id, enrollment_status);
        if (vqec_vision_ai_fwctl_fedbs_status_reply(result, _invocation).code_ != status_code::ok) {
            return;
        }
    } else {
        g_dbus_method_invocation_return_error_literal(
            _invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD, "unknown enrollment method");
        return;
    }
    g_dbus_method_invocation_return_value(_invocation,
        g_variant_new("(ssuuuti)", enrollment_status.request_id_.c_str(),
            enrollment_status.subject_ref_.c_str(),
            vqec_vision_ai_fwctl_fedbs_state_value(enrollment_status.state_),
            static_cast<guint>(enrollment_status.accepted_samples_),
            static_cast<guint>(enrollment_status.expected_samples_),
            enrollment_status.gallery_revision_,
            vqec_vision_ai_fwctl_fedbs_error_value(enrollment_status.last_error_)));
    } catch (...) {
        // C++ exceptions must never cross the GIO callback ABI. Mutation outcomes may
        // be unknown, so callers reconcile the request receipt before retrying.
        g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED, "enrollment operation failed; reconcile request status");
    }
}

const GDBusInterfaceVTable g_vtable = {
    vqec_vision_ai_fwctl_fedbs_method_call, nullptr, nullptr, {nullptr, nullptr, nullptr, nullptr}};

}  // namespace

struct face_enrollment_dbus_server::implementation : dbus_binding {
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

face_enrollment_dbus_server::face_enrollment_dbus_server()
    : implementation_(std::make_unique<implementation>()) {}
face_enrollment_dbus_server::~face_enrollment_dbus_server() noexcept = default;

status face_enrollment_dbus_server::vqec_vision_ai_fwctl_fedbs_open(
    face_enrollment_port& _port, const face_enrollment_dbus_config& _config) {
    if (implementation_->connection_ != nullptr) {
        return {status_code::invalid_state, "face enrollment DBus is already open"};
    }
    if (!g_dbus_is_name(_config.trusted_peer_bus_name_.c_str()) ||
        _config.rpc_timeout_ms_ <= 0 || _config.max_callbacks_per_poll_ == 0 ||
        _config.max_callbacks_per_poll_ > g_max_callbacks_ceiling) {
        return {status_code::invalid_argument, "invalid enrollment DBus configuration"};
    }
    error_owner error;
    gchar* address = g_dbus_address_get_for_bus_sync(
        _config.use_session_bus_ ? G_BUS_TYPE_SESSION : G_BUS_TYPE_SYSTEM,
        nullptr, &error.value_);
    if (address == nullptr) {
        return {status_code::io_error, "cannot resolve face enrollment DBus address"};
    }
    implementation_->connection_ = g_dbus_connection_new_for_address_sync(address,
        static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
            G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION), nullptr, nullptr, &error.value_);
    g_free(address);
    if (implementation_->connection_ == nullptr) {
        return {status_code::io_error, "cannot connect to face enrollment DBus"};
    }
    g_dbus_connection_set_exit_on_close(implementation_->connection_, FALSE);
    GVariant* peer_reply = g_dbus_connection_call_sync(implementation_->connection_,
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "GetNameOwner", g_variant_new("(s)", _config.trusted_peer_bus_name_.c_str()),
        G_VARIANT_TYPE("(s)"), G_DBUS_CALL_FLAGS_NONE, _config.rpc_timeout_ms_, nullptr,
        &error.value_);
    if (peer_reply == nullptr) {
        return {status_code::unauthorized, "configured FW peer has no DBus owner"};
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
        return {status_code::protocol_error, "invalid face enrollment DBus introspection"};
    }
    implementation_->node_ = node.value_;
    node.value_ = nullptr;
    g_main_context_push_thread_default(implementation_->context_);
    implementation_->registration_id_ = g_dbus_connection_register_object(
        implementation_->connection_, face_enrollment_dbus_protocol::g_object_path,
        implementation_->node_->interfaces[0], &g_vtable,
        static_cast<dbus_binding*>(implementation_.get()), nullptr,
        &error.value_);
    g_main_context_pop_thread_default(implementation_->context_);
    if (implementation_->registration_id_ == 0) {
        return {status_code::io_error, "cannot register face enrollment DBus object"};
    }
    // Name ownership is deliberately requested through the standard bus API. If another
    // process owns it, registration is rejected instead of silently routing to it.
    GVariant* reply = g_dbus_connection_call_sync(implementation_->connection_,
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "RequestName", g_variant_new("(su)", face_enrollment_dbus_protocol::g_bus_name,
            g_request_name_do_not_queue), G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE,
        _config.rpc_timeout_ms_, nullptr, &error.value_);
    if (reply == nullptr) {
        return {status_code::io_error, "cannot acquire face enrollment DBus name"};
    }
    guint32 reply_code = 0;
    g_variant_get(reply, "(u)", &reply_code);
    g_variant_unref(reply);
    if (reply_code != g_request_name_primary_owner &&
        reply_code != g_request_name_already_owner) {
        return {status_code::invalid_state, "face enrollment DBus name is already owned"};
    }
    return {};
}

void face_enrollment_dbus_server::vqec_vision_ai_fwctl_fedbs_poll() noexcept {
    if (implementation_->connection_ == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < implementation_->max_callbacks_per_poll_ &&
            g_main_context_pending(implementation_->context_); ++index) {
        g_main_context_iteration(implementation_->context_, FALSE);
    }
}

}  // namespace vqec::vision::ai
