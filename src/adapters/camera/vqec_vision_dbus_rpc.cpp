#include "vqec_vision_dbus_rpc.hpp"
#include "vqec_vision_camera_protocol.hpp"

#include <utility>

#include <gio/gio.h>

namespace vqec::vision::ai {
namespace {

struct variant_owner {
    GVariant* value_{nullptr};
    ~variant_owner() noexcept {
        if (value_ != nullptr) {
            g_variant_unref(value_);
        }
    }
};

struct error_owner {
    GError* value_{nullptr};
    ~error_owner() noexcept {
        if (value_ != nullptr) {
            g_error_free(value_);
        }
    }
};

struct address_owner {
    gchar* value_{nullptr};
    ~address_owner() noexcept {
        g_free(value_);
    }
};

status vqec_vision_ai_camer_dbrpc_map_error(const GError* _error) {
    if (_error != nullptr) {
        if (g_error_matches(_error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT) ||
            g_error_matches(_error, G_DBUS_ERROR, G_DBUS_ERROR_NO_REPLY) ||
            g_error_matches(_error, G_DBUS_ERROR, G_DBUS_ERROR_TIMEOUT)) {
            return {status_code::timeout, "Camera D-Bus timeout; mutation outcome is unknown"};
        }
        if (g_error_matches(_error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED) ||
            g_error_matches(_error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED)) {
            return {status_code::unauthorized, "Camera D-Bus access denied"};
        }
    }
    return {status_code::io_error, "Camera D-Bus operation failed; reconcile pending mutation"};
}

} // namespace

struct dbus_rpc::implementation {
    GDBusConnection* connection_{nullptr};
    ~implementation() noexcept {
        if (connection_ != nullptr) {
            // Private connection: request close without blocking a destructor.
            g_dbus_connection_close(connection_, nullptr, nullptr, nullptr);
            g_object_unref(connection_);
        }
    }
};

dbus_rpc::dbus_rpc() : implementation_(std::make_unique<implementation>()) {}
dbus_rpc::~dbus_rpc() = default;

status dbus_rpc::vqec_vision_ai_camer_dbrpc_open(bool _use_session_bus) {
    if (implementation_->connection_ != nullptr) {
        return {status_code::invalid_state, "Camera bus connection already initialized"};
    }
    error_owner error;
    address_owner address{g_dbus_address_get_for_bus_sync(
        _use_session_bus ? G_BUS_TYPE_SESSION : G_BUS_TYPE_SYSTEM, nullptr, &error.value_)};
    if (address.value_ == nullptr) {
        return vqec_vision_ai_camer_dbrpc_map_error(error.value_);
    }
    implementation_->connection_ = g_dbus_connection_new_for_address_sync(
        address.value_,
        static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                          G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
        nullptr, nullptr, &error.value_);
    if (implementation_->connection_ == nullptr) {
        return vqec_vision_ai_camer_dbrpc_map_error(error.value_);
    }
    g_dbus_connection_set_exit_on_close(implementation_->connection_, FALSE);
    return {};
}

status dbus_rpc::vqec_vision_ai_camer_cmrpc_call(const std::string& _method,
                                                 const camera_fields& _request, int _timeout_ms,
                                                 camera_fields& _response) {
    _response.clear();
    if (_timeout_ms < 1 || _timeout_ms > camera_protocol::g_max_call_timeout_ms || _request.size() > camera_protocol::g_max_fields ||
        (_method != camera_protocol::g_start_stream && _method != camera_protocol::g_stop_stream && _method != camera_protocol::g_get_status)) {
        return {status_code::invalid_argument, "invalid Camera D-Bus method, size or timeout"};
    }
    if (implementation_->connection_ == nullptr ||
        g_dbus_connection_is_closed(implementation_->connection_)) {
        return {status_code::source_lost, "Camera D-Bus connection is not open"};
    }
    std::size_t total = 0;
    for (const auto& entry : _request) {
        if (entry.first.empty() || entry.first.size() > camera_protocol::g_max_key_bytes || entry.second.size() > camera_protocol::g_max_value_bytes ||
            entry.first.find('\0') != std::string::npos ||
            entry.second.find('\0') != std::string::npos ||
            !g_utf8_validate(entry.first.c_str(), -1, nullptr) ||
            !g_utf8_validate(entry.second.c_str(), -1, nullptr)) {
            return {status_code::invalid_argument, "invalid Camera D-Bus string field"};
        }
        total += entry.first.size() + entry.second.size();
    }
    if (total > camera_protocol::g_max_field_bytes) {
        return {status_code::invalid_argument, "Camera D-Bus request exceeds byte budget"};
    }
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    for (const auto& entry : _request) {
        g_variant_builder_add(&builder, "{sv}", entry.first.c_str(),
                              g_variant_new_string(entry.second.c_str()));
    }
    // Hold a non-floating reference; call_sync borrows this rather than consuming it.
    variant_owner parameters{
        g_variant_ref_sink(g_variant_new("(@a{sv})", g_variant_builder_end(&builder)))};
    error_owner error;
    variant_owner result{g_dbus_connection_call_sync(
        implementation_->connection_, camera_protocol::g_bus_name, camera_protocol::g_object_path,
        camera_protocol::g_interface_name, _method.c_str(), parameters.value_, G_VARIANT_TYPE("(a{sv})"),
        G_DBUS_CALL_FLAGS_NO_AUTO_START, _timeout_ms, nullptr, &error.value_)};
    if (result.value_ == nullptr) {
        return vqec_vision_ai_camer_dbrpc_map_error(error.value_);
    }
    if (g_variant_get_size(result.value_) > camera_protocol::g_max_reply_wire_bytes) {
        return {status_code::protocol_error, "Camera D-Bus reply exceeds wire budget"};
    }
    variant_owner dictionary{g_variant_get_child_value(result.value_, 0)};
    if (g_variant_n_children(dictionary.value_) > camera_protocol::g_max_fields) {
        return {status_code::protocol_error, "too many Camera D-Bus response fields"};
    }
    camera_fields decoded;
    total = 0;
    GVariantIter iterator;
    g_variant_iter_init(&iterator, dictionary.value_);
    const gchar* key = nullptr;
    GVariant* field = nullptr;
    while (g_variant_iter_next(&iterator, "{&sv}", &key, &field)) {
        variant_owner value{field};
        if (!g_variant_is_of_type(value.value_, G_VARIANT_TYPE_STRING)) {
            return {status_code::protocol_error, "legacy FW reply must contain string variants"};
        }
        gsize length = 0;
        const auto* text = g_variant_get_string(value.value_, &length);
        const std::string name(key);
        if (name.empty() || name.size() > camera_protocol::g_max_key_bytes || length > camera_protocol::g_max_value_bytes) {
            return {status_code::protocol_error, "oversized Camera D-Bus reply field"};
        }
        total += name.size() + length;
        if (total > camera_protocol::g_max_field_bytes || !decoded.emplace(name, std::string(text, length)).second) {
            return {status_code::protocol_error, "duplicate field or excessive Camera reply size"};
        }
    }
    _response = std::move(decoded);
    return {};
}

} // namespace vqec::vision::ai
