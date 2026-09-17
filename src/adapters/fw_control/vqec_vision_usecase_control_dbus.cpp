#include "vqec_vision_usecase_control_dbus.hpp"

#include <gio/gio.h>

#include <new>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_usecase_activation.hpp"

namespace vqec::vision::ai {
namespace {

constexpr guint g_request_name_do_not_queue = 4U;
constexpr guint g_request_name_primary_owner = 1U;
constexpr guint g_request_name_already_owner = 4U;
constexpr std::size_t g_max_callbacks_ceiling = 64;
constexpr gsize g_max_request_wire_bytes = 128U * 1024U;

struct dbus_binding {
    usecase_control_port* port_{nullptr};
    std::string trusted_sender_;
};

constexpr char g_introspection_xml[] =
    "<node><interface name='com.vqec.AiVision.UsecaseControl1'>"
    "<method name='ApplyDesiredPlan'><arg type='s' direction='in'/><arg type='t' direction='in'/><arg type='a(ssb)' direction='in'/><arg type='b' direction='out'/><arg type='t' direction='out'/><arg type='s' direction='out'/><arg type='u' direction='out'/></method>"
    "<method name='GetUsecaseStatus'><arg type='t' direction='out'/><arg type='t' direction='out'/><arg type='t' direction='out'/><arg type='a(ssbbbbbbbbss)' direction='out'/></method>"
    "<method name='GetCapabilities'><arg type='t' direction='out'/><arg type='a(ss)' direction='out'/></method>"
    "</interface></node>";

struct error_owner {
    GError* value_{nullptr};
    ~error_owner() noexcept { if (value_ != nullptr) { g_error_free(value_); } }
};

struct node_owner {
    GDBusNodeInfo* value_{nullptr};
    ~node_owner() noexcept { if (value_ != nullptr) { g_dbus_node_info_unref(value_); } }
};

const char* vqec_vision_ai_fwctl_ucdbs_apply_state(usecase_apply_state _state) noexcept {
    switch (_state) {
    case usecase_apply_state::unchanged: return "unchanged";
    case usecase_apply_state::reconciling: return "reconciling";
    case usecase_apply_state::running: return "running";
    case usecase_apply_state::degraded: return "degraded";
    case usecase_apply_state::failed: return "failed";
    }
    return "failed";
}

const char* vqec_vision_ai_fwctl_ucdbs_runtime_state(
    usecase_runtime_state _state) noexcept {
    switch (_state) {
    case usecase_runtime_state::disabled: return "disabled";
    case usecase_runtime_state::denied: return "denied";
    case usecase_runtime_state::unsupported: return "unsupported";
    case usecase_runtime_state::incompatible: return "incompatible";
    case usecase_runtime_state::resource_limited: return "resource_limited";
    case usecase_runtime_state::loading: return "loading";
    case usecase_runtime_state::running: return "running";
    case usecase_runtime_state::draining: return "draining";
    case usecase_runtime_state::faulted: return "faulted";
    }
    return "faulted";
}

guint vqec_vision_ai_fwctl_ucdbs_reason_code(status_code _code) noexcept {
    return static_cast<guint>(_code);
}

void vqec_vision_ai_fwctl_ucdbs_return_error(
    GDBusMethodInvocation* _invocation, const status& _result) {
    g_dbus_method_invocation_return_error(_invocation, G_DBUS_ERROR,
        G_DBUS_ERROR_FAILED, "%s", _result.message_.c_str());
}

void vqec_vision_ai_fwctl_ucdbs_apply(
    usecase_control_port& _port, GVariant* _parameters,
    GDBusMethodInvocation* _invocation) {
    const gchar* request_id = nullptr;
    guint64 expected_revision = 0;
    GVariant* wire_entries = nullptr;
    g_variant_get(_parameters, "(&st@a(ssb))", &request_id, &expected_revision, &wire_entries);
    usecase_desired_plan plan;
    plan.request_id_ = request_id == nullptr ? "" : request_id;
    plan.expected_control_revision_ = expected_revision;
    try {
        const auto count = g_variant_n_children(wire_entries);
        if (count > usecase_activation_limits::g_max_associations) {
            g_variant_unref(wire_entries);
            g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
                G_DBUS_ERROR_LIMITS_EXCEEDED, "desired plan exceeds association limit");
            return;
        }
        plan.entries_.reserve(count);
        GVariantIter iterator;
        g_variant_iter_init(&iterator, wire_entries);
        const gchar* source_id = nullptr;
        const gchar* usecase_id = nullptr;
        gboolean desired = FALSE;
        while (g_variant_iter_loop(&iterator, "(&s&sb)",
                   &source_id, &usecase_id, &desired)) {
            plan.entries_.push_back({source_id == nullptr ? "" : source_id,
                usecase_id == nullptr ? "" : usecase_id, desired != FALSE});
        }
    } catch (const std::bad_alloc&) {
        g_variant_unref(wire_entries);
        g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
            G_DBUS_ERROR_NO_MEMORY, "desired plan allocation failed");
        return;
    }
    g_variant_unref(wire_entries);

    usecase_apply_receipt receipt;
    const auto applied = _port.vqec_vision_ai_ports_ucctl_apply_desired_plan(plan, receipt);
    if (applied.code_ != status_code::ok) {
        usecase_control_status current;
        const auto queried = _port.vqec_vision_ai_ports_ucctl_get_status(current);
        if (queried.code_ != status_code::ok) {
            vqec_vision_ai_fwctl_ucdbs_return_error(_invocation, applied);
            return;
        }
        g_dbus_method_invocation_return_value(_invocation,
            g_variant_new("(btsu)", FALSE, current.control_revision_, "failed",
                vqec_vision_ai_fwctl_ucdbs_reason_code(applied.code_)));
        return;
    }
    g_dbus_method_invocation_return_value(_invocation,
        g_variant_new("(btsu)", receipt.accepted_, receipt.control_revision_,
            vqec_vision_ai_fwctl_ucdbs_apply_state(receipt.apply_state_),
            vqec_vision_ai_fwctl_ucdbs_reason_code(receipt.reason_code_)));
}

void vqec_vision_ai_fwctl_ucdbs_status(
    usecase_control_port& _port, GDBusMethodInvocation* _invocation) {
    usecase_control_status snapshot;
    const auto queried = _port.vqec_vision_ai_ports_ucctl_get_status(snapshot);
    if (queried.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_ucdbs_return_error(_invocation, queried);
        return;
    }
    for (const auto& entry : snapshot.entries_) {
        // Bound every reply string before it reaches the bus. A port bug that produced an
        // over-long reason must fail the call, not emit an oversized D-Bus message.
        if (entry.reason_.size() > usecase_control_limits::g_max_reason_bytes) {
            vqec_vision_ai_fwctl_ucdbs_return_error(_invocation,
                {status_code::protocol_error, "usecase status reason exceeds the wire bound"});
            return;
        }
    }
    GVariantBuilder entries;
    g_variant_builder_init(&entries, G_VARIANT_TYPE("a(ssbbbbbbbbss)"));
    for (const auto& entry : snapshot.entries_) {
        g_variant_builder_add(&entries, "(ssbbbbbbbbss)", entry.source_id_.c_str(),
            entry.usecase_id_.c_str(), entry.installed_, entry.entitled_, entry.desired_,
            entry.supported_, entry.compatible_, entry.admitted_, entry.loaded_,
            entry.running_, vqec_vision_ai_fwctl_ucdbs_runtime_state(entry.effective_state_),
            entry.reason_.c_str());
    }
    g_dbus_method_invocation_return_value(_invocation,
        g_variant_new("(ttt@a(ssbbbbbbbbss))", snapshot.control_revision_,
            snapshot.entitlement_revision_, snapshot.runtime_generation_,
            g_variant_builder_end(&entries)));
}

void vqec_vision_ai_fwctl_ucdbs_capabilities(
    usecase_control_port& _port, GDBusMethodInvocation* _invocation) {
    usecase_capability_snapshot snapshot;
    const auto queried = _port.vqec_vision_ai_ports_ucctl_get_capabilities(snapshot);
    if (queried.code_ != status_code::ok) {
        vqec_vision_ai_fwctl_ucdbs_return_error(_invocation, queried);
        return;
    }
    GVariantBuilder entries;
    g_variant_builder_init(&entries, G_VARIANT_TYPE("a(ss)"));
    for (const auto& entry : snapshot.usecases_) {
        g_variant_builder_add(&entries, "(ss)", entry.usecase_id_.c_str(),
            entry.usecase_version_.c_str());
    }
    g_dbus_method_invocation_return_value(_invocation,
        g_variant_new("(t@a(ss))", snapshot.catalog_revision_,
            g_variant_builder_end(&entries)));
}

void vqec_vision_ai_fwctl_ucdbs_method_call(
    GDBusConnection*, const gchar* _sender, const gchar*, const gchar*,
    const gchar* _method_name, GVariant* _parameters,
    GDBusMethodInvocation* _invocation, gpointer _user_data) {
    auto* binding = static_cast<dbus_binding*>(_user_data);
    if (binding == nullptr || _sender == nullptr ||
        binding->trusted_sender_ != _sender) {
        g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
            G_DBUS_ERROR_ACCESS_DENIED, "usecase caller is not the configured FW peer");
        return;
    }
    if (g_variant_get_size(_parameters) > g_max_request_wire_bytes) {
        g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
            G_DBUS_ERROR_LIMITS_EXCEEDED, "usecase request exceeds wire byte limit");
        return;
    }
    if (binding->port_ == nullptr) {
        g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED, "usecase control port unavailable");
        return;
    }
    try {
        if (g_strcmp0(_method_name, usecase_control_dbus_protocol::g_apply_method) == 0) {
            vqec_vision_ai_fwctl_ucdbs_apply(*binding->port_, _parameters, _invocation);
        } else if (g_strcmp0(
                       _method_name, usecase_control_dbus_protocol::g_status_method) == 0) {
            vqec_vision_ai_fwctl_ucdbs_status(*binding->port_, _invocation);
        } else if (g_strcmp0(_method_name,
                       usecase_control_dbus_protocol::g_capabilities_method) == 0) {
            vqec_vision_ai_fwctl_ucdbs_capabilities(*binding->port_, _invocation);
        } else {
            g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
                G_DBUS_ERROR_UNKNOWN_METHOD, "unknown usecase control method");
        }
    } catch (...) {
        g_dbus_method_invocation_return_error_literal(_invocation, G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED, "usecase control operation failed; query status");
    }
}

const GDBusInterfaceVTable g_vtable = {
    vqec_vision_ai_fwctl_ucdbs_method_call, nullptr, nullptr,
    {nullptr, nullptr, nullptr, nullptr}};

}  // namespace

struct usecase_control_dbus_server::implementation : dbus_binding {
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

usecase_control_dbus_server::usecase_control_dbus_server()
    : implementation_(std::make_unique<implementation>()) {}
usecase_control_dbus_server::~usecase_control_dbus_server() noexcept = default;

status usecase_control_dbus_server::vqec_vision_ai_fwctl_ucdbs_open(
    usecase_control_port& _port, const usecase_control_dbus_config& _config) {
    if (implementation_->connection_ != nullptr) {
        return {status_code::invalid_state, "usecase control DBus is already open"};
    }
    if (!g_dbus_is_name(_config.service_bus_name_.c_str()) ||
        !g_variant_is_object_path(_config.object_path_.c_str()) ||
        !g_dbus_is_name(_config.trusted_peer_bus_name_.c_str()) ||
        _config.rpc_timeout_ms_ <= 0 || _config.max_callbacks_per_poll_ == 0 ||
        _config.max_callbacks_per_poll_ > g_max_callbacks_ceiling) {
        return {status_code::invalid_argument, "invalid usecase control DBus configuration"};
    }
    error_owner error;
    gchar* address = g_dbus_address_get_for_bus_sync(
        _config.use_session_bus_ ? G_BUS_TYPE_SESSION : G_BUS_TYPE_SYSTEM,
        nullptr, &error.value_);
    if (address == nullptr) {
        return {status_code::io_error, "cannot resolve usecase control DBus address"};
    }
    implementation_->connection_ = g_dbus_connection_new_for_address_sync(address,
        static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
            G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION), nullptr, nullptr,
        &error.value_);
    g_free(address);
    if (implementation_->connection_ == nullptr) {
        return {status_code::io_error, "cannot connect to usecase control DBus"};
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
        return {status_code::protocol_error, "invalid usecase control DBus introspection"};
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
        return {status_code::io_error, "cannot register usecase control DBus object"};
    }
    GVariant* reply = g_dbus_connection_call_sync(implementation_->connection_,
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "RequestName", g_variant_new("(su)", _config.service_bus_name_.c_str(),
            g_request_name_do_not_queue), G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE,
        _config.rpc_timeout_ms_, nullptr, &error.value_);
    if (reply == nullptr) {
        return {status_code::io_error, "cannot acquire usecase control DBus name"};
    }
    guint32 reply_code = 0;
    g_variant_get(reply, "(u)", &reply_code);
    g_variant_unref(reply);
    if (reply_code != g_request_name_primary_owner &&
        reply_code != g_request_name_already_owner) {
        return {status_code::invalid_state, "usecase control DBus name is already owned"};
    }
    return {};
}

void usecase_control_dbus_server::vqec_vision_ai_fwctl_ucdbs_poll() noexcept {
    if (implementation_->connection_ == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < implementation_->max_callbacks_per_poll_ &&
            g_main_context_pending(implementation_->context_); ++index) {
        g_main_context_iteration(implementation_->context_, FALSE);
    }
}

}  // namespace vqec::vision::ai
