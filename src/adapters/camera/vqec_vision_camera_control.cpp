#include "vqec_vision_camera_control.hpp"
#include "vqec_vision_camera_protocol.hpp"

#include <charconv>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_camer_cctrl_is_identifier(const std::string& _value) {
    return vqec_vision_ai_cntr_ident_is_valid(
        _value, camera_protocol::g_max_identifier_bytes);
}

bool vqec_vision_ai_camer_cctrl_read_number(
    const camera_fields& _fields, const char* _key, std::uint32_t& _value) {
    const auto entry = _fields.find(_key);
    if (entry == _fields.end() || entry->second.empty()) {
        return false;
    }
    const auto* begin = entry->second.data();
    const auto* end = begin + entry->second.size();
    const auto parsed = std::from_chars(begin, end, _value);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

status vqec_vision_ai_camer_cctrl_read_result(
    const camera_fields& _fields, std::uint32_t& _code) {
    if (_fields.size() > camera_protocol::g_max_fields) {
        return {status_code::protocol_error, "too many Camera response fields"};
    }
    std::size_t total = 0;
    for (const auto& entry : _fields) {
        if (entry.first.size() > camera_protocol::g_max_key_bytes || entry.second.size() > camera_protocol::g_max_value_bytes) {
            return {status_code::protocol_error, "oversized Camera response field"};
        }
        total += entry.first.size() + entry.second.size();
    }
    if (total > camera_protocol::g_max_field_bytes || !vqec_vision_ai_camer_cctrl_read_number(_fields, camera_protocol::g_code_field, _code)) {
        return {status_code::protocol_error, "missing or invalid Camera response code"};
    }
    if (_code == camera_protocol::g_code_ok) {
        return {};
    }
    status_code code = status_code::io_error;
    if (_code == camera_protocol::g_code_invalid_argument || _code == camera_protocol::g_code_invalid_profile) {
        code = status_code::invalid_argument;
    } else if (_code == camera_protocol::g_code_not_found || _code == camera_protocol::g_code_not_running) {
        code = status_code::source_lost;
    } else if (_code == camera_protocol::g_code_busy ||
               _code == camera_protocol::g_code_resource_exhausted) {
        code = status_code::resource_exhausted;
    } else if (_code == camera_protocol::g_code_timeout) {
        code = status_code::timeout;
    } else if (_code == camera_protocol::g_code_unsupported) {
        code = status_code::unsupported;
    } else if (_code == camera_protocol::g_code_permission_denied) {
        code = status_code::unauthorized;
    } else if (_code == camera_protocol::g_code_version_mismatch) {
        code = status_code::protocol_error;
    }
    return {code, "Camera Service returned code " + std::to_string(_code)};
}

}  // namespace

camera_control::camera_control(std::shared_ptr<camera_rpc> _rpc) : rpc_(std::move(_rpc)) {}

status camera_control::vqec_vision_ai_camer_cctrl_start(
    const camera_acquire_request& _request, int _timeout_ms) {
    if (!rpc_ || _timeout_ms < 1 || _timeout_ms > camera_protocol::g_max_call_timeout_ms ||
        !vqec_vision_ai_camer_cctrl_is_identifier(_request.consumer_id_) ||
        !vqec_vision_ai_camer_cctrl_is_identifier(_request.request_id_)) {
        return {status_code::invalid_argument, "invalid Camera acquisition identity or timeout"};
    }
    if (state_ == camera_lease_state::stop_pending) {
        return {status_code::invalid_state, "reconcile pending StopStream before StartStream"};
    }
    if (state_ != camera_lease_state::idle &&
        (_request.camera_id_ != request_.camera_id_ ||
         _request.channel_id_ != request_.channel_id_ ||
         _request.consumer_id_ != request_.consumer_id_ ||
         _request.request_id_ != request_.request_id_)) {
        return {status_code::invalid_state, "cannot replace an unresolved acquisition identity"};
    }
    if (state_ == camera_lease_state::acquired) {
        return profile_.is_valid_ ? status{} :
            status{status_code::protocol_error, "acquired handle has an invalid profile; stop it"};
    }
    if (state_ == camera_lease_state::idle) {
        request_ = _request;
        state_ = camera_lease_state::start_pending;
    }
    camera_fields fields{{camera_protocol::g_camera_id_field, std::to_string(request_.camera_id_)},
                         {camera_protocol::g_channel_id_field,
                          std::to_string(request_.channel_id_)},
                         {camera_protocol::g_stream_id_field, camera_protocol::g_ai_stream},
                         {camera_protocol::g_transport_field, camera_protocol::g_fd_transport}, {camera_protocol::g_consumer_id_field, request_.consumer_id_},
                         {camera_protocol::g_request_id_field, request_.request_id_}};
    camera_fields response;
    const auto transport = rpc_->vqec_vision_ai_camer_cmrpc_call(
        camera_protocol::g_start_stream, fields, _timeout_ms, response);
    if (transport.code_ != status_code::ok) {
        return transport;
    }
    std::uint32_t code = UINT32_MAX;
    const auto result = vqec_vision_ai_camer_cctrl_read_result(response, code);
    if (result.code_ != status_code::ok) {
        return result;
    }
    const auto handle = response.find(camera_protocol::g_stream_handle_field);
    if (handle == response.end() || handle->second.empty() || handle->second.size() > camera_protocol::g_max_handle_bytes ||
        handle->second.find('\0') != std::string::npos) {
        return {status_code::protocol_error, "StartStream did not return a valid handle"};
    }
    handle_ = handle->second;
    state_ = camera_lease_state::acquired;
    camera_stream_profile profile;
    const auto codec = response.find(camera_protocol::g_codec_field);
    if (!vqec_vision_ai_camer_cctrl_read_number(response, camera_protocol::g_width_field, profile.width_) ||
        !vqec_vision_ai_camer_cctrl_read_number(response, camera_protocol::g_height_field, profile.height_) ||
        !vqec_vision_ai_camer_cctrl_read_number(response, camera_protocol::g_fps_field, profile.fps_) ||
        profile.width_ == 0 || profile.height_ == 0 || profile.fps_ == 0 ||
        (profile.width_ & 1U) != 0 || (profile.height_ & 1U) != 0 ||
        codec == response.end() || codec->second != camera_protocol::g_raw_codec) {
        return {status_code::protocol_error, "invalid RAW profile; handle retained for cleanup"};
    }
    profile.is_valid_ = true;
    profile_ = profile;
    return {};
}

status camera_control::vqec_vision_ai_camer_cctrl_stop(
    const std::string& _request_id, int _timeout_ms) {
    if (!rpc_ || _timeout_ms < 1 || _timeout_ms > camera_protocol::g_max_call_timeout_ms ||
        !vqec_vision_ai_camer_cctrl_is_identifier(_request_id)) {
        return {status_code::invalid_argument, "invalid StopStream identity or timeout"};
    }
    if (state_ == camera_lease_state::idle) {
        return {};
    }
    if (state_ == camera_lease_state::start_pending) {
        return {status_code::invalid_state, "reconcile StartStream to recover the handle first"};
    }
    if (_request_id == request_.request_id_ ||
        (state_ == camera_lease_state::stop_pending && _request_id != stop_request_id_)) {
        return {status_code::invalid_argument, "StopStream needs its own stable request ID"};
    }
    if (state_ == camera_lease_state::acquired) {
        stop_request_id_ = _request_id;
        state_ = camera_lease_state::stop_pending;
    }
    camera_fields fields{{camera_protocol::g_stream_handle_field, handle_}, {camera_protocol::g_stream_id_field, camera_protocol::g_ai_stream},
                         {camera_protocol::g_consumer_id_field, request_.consumer_id_},
                         {camera_protocol::g_request_id_field, stop_request_id_}};
    camera_fields response;
    const auto transport = rpc_->vqec_vision_ai_camer_cmrpc_call(
        camera_protocol::g_stop_stream, fields, _timeout_ms, response);
    if (transport.code_ != status_code::ok) {
        return transport;
    }
    std::uint32_t code = UINT32_MAX;
    const auto result = vqec_vision_ai_camer_cctrl_read_result(response, code);
    if (result.code_ != status_code::ok &&
        !(result.code_ == status_code::source_lost && code == camera_protocol::g_code_not_found)) {
        return result;
    }
    state_ = camera_lease_state::idle;
    handle_.clear();
    stop_request_id_.clear();
    request_ = {};
    profile_ = {};
    return {};
}

camera_lease_state camera_control::vqec_vision_ai_camer_cctrl_get_state() const noexcept {
    return state_;
}

const camera_stream_profile&
camera_control::vqec_vision_ai_camer_cctrl_get_profile() const noexcept {
    return profile_;
}

const std::string& camera_control::vqec_vision_ai_camer_cctrl_get_handle() const noexcept {
    return handle_;
}

}  // namespace vqec::vision::ai
