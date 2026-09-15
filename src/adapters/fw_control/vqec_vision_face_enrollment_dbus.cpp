#include "vqec_vision_face_enrollment_dbus.hpp"

#include <gio/gio.h>

#include <limits>
#include <utility>

namespace vqec::vision::ai {
namespace {

constexpr char g_introspection_xml[] =
    "<node><interface name='com.vqec.Lacai.FaceEnrollment1'>"
    "<method name='BeginEnrollment'><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='u' direction='in'/><arg type='u' direction='in'/><arg type='t' direction='in'/><arg type='u' direction='in'/><arg type='t' direction='in'/><arg type='s' direction='out'/><arg type='s' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='t' direction='out'/><arg type='i' direction='out'/></method>"
    "<method name='CancelEnrollment'><arg type='s' direction='in'/><arg type='s' direction='out'/><arg type='s' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='t' direction='out'/><arg type='i' direction='out'/></method>"
    "<method name='RemoveSubject'><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='t' direction='out'/><arg type='i' direction='out'/></method>"
    "<method name='GetEnrollmentStatus'><arg type='s' direction='in'/><arg type='s' direction='out'/><arg type='s' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='u' direction='out'/><arg type='t' direction='out'/><arg type='i' direction='out'/></method>"
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
    return static_cast<guint>(_state);
}

void vqec_vision_ai_fwctl_fedbs_method_call(
    GDBusConnection* _connection, const gchar* _sender, const gchar* _object_path,
    const gchar* _interface_name, const gchar* _method_name, GVariant* _parameters,
    GDBusMethodInvocation* _invocation, gpointer _user_data) {
    (void)_connection;
    (void)_sender;
    (void)_object_path;
    (void)_interface_name;
    auto* port = static_cast<face_enrollment_port*>(_user_data);
    if (port == nullptr) {
        g_dbus_method_invocation_return_error_literal(
            _invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "enrollment port unavailable");
        return;
    }
    face_enrollment_status enrollment_status;
    status result;
    if (g_strcmp0(_method_name, face_enrollment_dbus_protocol::g_begin_method) == 0) {
        const gchar* request_id = nullptr;
        const gchar* subject_ref = nullptr;
        const gchar* source_id = nullptr;
        guint camera_id = 0;
        guint channel_id = 0;
        guint64 target_track_id = 0;
        guint expected_samples = 0;
        guint64 expected_revision = 0;
        g_variant_get(_parameters, "(&s&s&suututu)", &request_id, &subject_ref, &source_id,
            &camera_id, &channel_id, &target_track_id, &expected_samples, &expected_revision);
        face_enrollment_begin_request request;
        request.request_id_ = request_id == nullptr ? "" : request_id;
        request.subject_ref_ = subject_ref == nullptr ? "" : subject_ref;
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
            static_cast<gint>(enrollment_status.last_error_)));
}

const GDBusInterfaceVTable g_vtable = {
    vqec_vision_ai_fwctl_fedbs_method_call, nullptr, nullptr, {nullptr, nullptr, nullptr, nullptr}};

}  // namespace

struct face_enrollment_dbus_server::implementation {
    GDBusConnection* connection_{nullptr};
    guint registration_id_{0};
    GDBusNodeInfo* node_{nullptr};
    face_enrollment_port* port_{nullptr};
    ~implementation() noexcept {
        if (connection_ != nullptr && registration_id_ != 0) {
            g_dbus_connection_unregister_object(connection_, registration_id_);
        }
        if (node_ != nullptr) {
            g_dbus_node_info_unref(node_);
        }
        if (connection_ != nullptr) {
            g_object_unref(connection_);
        }
    }
};

face_enrollment_dbus_server::face_enrollment_dbus_server()
    : implementation_(std::make_unique<implementation>()) {}
face_enrollment_dbus_server::~face_enrollment_dbus_server() noexcept = default;

status face_enrollment_dbus_server::vqec_vision_ai_fwctl_fedbs_open(
    face_enrollment_port& _port, bool _use_session_bus) {
    if (implementation_->connection_ != nullptr) {
        return {status_code::invalid_state, "face enrollment DBus is already open"};
    }
    error_owner error;
    implementation_->connection_ = g_bus_get_sync(
        _use_session_bus ? G_BUS_TYPE_SESSION : G_BUS_TYPE_SYSTEM, nullptr, &error.value_);
    if (implementation_->connection_ == nullptr) {
        return {status_code::io_error, "cannot connect to face enrollment DBus"};
    }
    implementation_->port_ = &_port;
    node_owner node{g_dbus_node_info_new_for_xml(g_introspection_xml, &error.value_)};
    if (node.value_ == nullptr || node.value_->interfaces == nullptr ||
        node.value_->interfaces[0] == nullptr) {
        return {status_code::protocol_error, "invalid face enrollment DBus introspection"};
    }
    implementation_->node_ = node.value_;
    node.value_ = nullptr;
    implementation_->registration_id_ = g_dbus_connection_register_object(
        implementation_->connection_, face_enrollment_dbus_protocol::g_object_path,
        implementation_->node_->interfaces[0], &g_vtable, implementation_->port_, nullptr,
        &error.value_);
    if (implementation_->registration_id_ == 0) {
        return {status_code::io_error, "cannot register face enrollment DBus object"};
    }
    // Name ownership is deliberately requested through the standard bus API. If another
    // process owns it, registration is rejected instead of silently routing to it.
    GVariant* reply = g_dbus_connection_call_sync(implementation_->connection_,
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "RequestName", g_variant_new("(su)", face_enrollment_dbus_protocol::g_bus_name, 4U),
        G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, 1000, nullptr, &error.value_);
    if (reply == nullptr) {
        return {status_code::io_error, "cannot acquire face enrollment DBus name"};
    }
    guint32 reply_code = 0;
    g_variant_get(reply, "(u)", &reply_code);
    g_variant_unref(reply);
    if (reply_code != 1U && reply_code != 4U) {
        return {status_code::invalid_state, "face enrollment DBus name is already owned"};
    }
    return {};
}

void face_enrollment_dbus_server::vqec_vision_ai_fwctl_fedbs_poll() noexcept {
    if (implementation_->connection_ == nullptr) {
        return;
    }
    constexpr std::size_t g_max_callbacks_per_poll = 4;
    for (std::size_t index = 0; index < g_max_callbacks_per_poll &&
            g_main_context_pending(nullptr); ++index) {
        g_main_context_iteration(nullptr, FALSE);
    }
}

}  // namespace vqec::vision::ai
