#include "vqec_vision_raw_source_resolver.hpp"

#include <limits>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::size_t g_max_socket_path_bytes = 107;

bool vqec_vision_ai_camer_rsrsv_is_identifier(const std::string& _value) noexcept {
    return vqec_vision_ai_cntr_ident_is_valid(
        _value, deployment_limits::g_max_identifier_bytes);
}

bool vqec_vision_ai_camer_rsrsv_is_socket_path(const std::string& _value) noexcept {
    return !_value.empty() && _value.front() == '/' &&
        _value.size() <= g_max_socket_path_bytes &&
        _value.find('\0') == std::string::npos;
}

status vqec_vision_ai_camer_rsrsv_validate_route(const raw_source_route& _route) {
    if (!vqec_vision_ai_camer_rsrsv_is_identifier(_route.raw_source_ref_) ||
        !vqec_vision_ai_camer_rsrsv_is_socket_path(_route.media_socket_path_) ||
        _route.producer_uid_ == std::numeric_limits<std::uint32_t>::max() ||
        _route.nv12_format_value_ == 0) {
        return {status_code::invalid_argument, "invalid FW RAW-source route"};
    }
    return {};
}

}  // namespace

status static_raw_source_resolver::vqec_vision_ai_camer_rsrsv_add_route(
    const raw_source_route& _route) {
    const auto valid = vqec_vision_ai_camer_rsrsv_validate_route(_route);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (route_count_ >= routes_.size()) {
        return {status_code::resource_exhausted, "FW RAW-source registry is full"};
    }
    for (std::uint16_t index = 0; index < route_count_; ++index) {
        const auto& existing = routes_[index];
        if (existing.raw_source_ref_ == _route.raw_source_ref_ ||
            existing.media_socket_path_ == _route.media_socket_path_ ||
            (existing.camera_id_ == _route.camera_id_ &&
             existing.channel_id_ == _route.channel_id_)) {
            return {status_code::invalid_argument, "duplicate FW RAW-source route"};
        }
    }
    raw_source_route candidate = _route;
    routes_[route_count_] = std::move(candidate);
    ++route_count_;
    return {};
}

status static_raw_source_resolver::vqec_vision_ai_camer_rsrsv_resolve_source(
    const std::string& _raw_source_ref, raw_source_route& _route) const {
    if (!vqec_vision_ai_camer_rsrsv_is_identifier(_raw_source_ref)) {
        return {status_code::invalid_argument, "invalid FW RAW-source reference"};
    }
    for (std::uint16_t index = 0; index < route_count_; ++index) {
        if (routes_[index].raw_source_ref_ == _raw_source_ref) {
            raw_source_route candidate = routes_[index];
            _route = std::move(candidate);
            return {};
        }
    }
    return {status_code::source_lost, "FW RAW-source reference is not registered"};
}

std::uint16_t static_raw_source_resolver::vqec_vision_ai_camer_rsrsv_get_count()
    const noexcept {
    return route_count_;
}

status vqec_vision_ai_camer_rsrsv_make_legacy_route(
    const source_deployment_config& _source, const std::string& _socket_dir,
    std::uint32_t _producer_uid, std::uint32_t _nv12_format_value,
    raw_source_route& _route) {
    if (_source.channel_id_ != 0) {
        return {status_code::unsupported,
                "released third RAW route supports channel zero only"};
    }
    if (!vqec_vision_ai_camer_rsrsv_is_identifier(_source.raw_source_ref_) ||
        _socket_dir.empty() || _socket_dir.front() != '/' ||
        _socket_dir.find('\0') != std::string::npos ||
        _producer_uid == std::numeric_limits<std::uint32_t>::max() ||
        _nv12_format_value == 0) {
        return {status_code::invalid_argument, "invalid released third RAW route input"};
    }
    raw_source_route candidate;
    candidate.raw_source_ref_ = _source.raw_source_ref_;
    candidate.camera_id_ = _source.camera_id_;
    candidate.channel_id_ = _source.channel_id_;
    candidate.media_socket_path_ = _socket_dir + "/0_third_ai";
    if (_source.camera_id_ != 0) {
        candidate.media_socket_path_ += "_cam" + std::to_string(_source.camera_id_);
    }
    candidate.media_socket_path_ += ".sock";
    candidate.producer_uid_ = _producer_uid;
    candidate.nv12_format_value_ = _nv12_format_value;
    const auto valid = vqec_vision_ai_camer_rsrsv_validate_route(candidate);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    _route = std::move(candidate);
    return {};
}

status vqec_vision_ai_camer_rsrsv_compose_lifecycle(
    const source_deployment_config& _source, const raw_source_route& _route,
    const raw_source_cycle_identity& _identity, camera_lifecycle_config& _config) {
    const auto valid = vqec_vision_ai_camer_rsrsv_validate_route(_route);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (_source.raw_source_ref_ != _route.raw_source_ref_ ||
        _source.camera_id_ != _route.camera_id_ ||
        _source.channel_id_ != _route.channel_id_ ||
        !vqec_vision_ai_camer_rsrsv_is_identifier(_identity.consumer_id_) ||
        !vqec_vision_ai_camer_rsrsv_is_identifier(_identity.start_request_id_) ||
        !vqec_vision_ai_camer_rsrsv_is_identifier(_identity.stop_request_id_) ||
        _identity.start_request_id_ == _identity.stop_request_id_ ||
        _source.profile_.fps_denominator_ == 0 ||
        _source.profile_.fps_numerator_ % _source.profile_.fps_denominator_ != 0) {
        return {status_code::invalid_argument,
                "deployment, route, cycle identity or legacy FPS mismatch"};
    }
    const auto integer_fps =
        _source.profile_.fps_numerator_ / _source.profile_.fps_denominator_;
    if (integer_fps == 0) {
        return {status_code::invalid_argument, "legacy RAW route requires positive FPS"};
    }
    legacy_frame_limits limits;
    const auto frame_limits = vqec_vision_ai_camer_lwire_make_limits(
        _source, _route.nv12_format_value_, limits);
    if (frame_limits.code_ != status_code::ok) {
        return frame_limits;
    }

    camera_lifecycle_config candidate;
    candidate.acquire_.camera_id_ = _route.camera_id_;
    candidate.acquire_.channel_id_ = _route.channel_id_;
    candidate.acquire_.consumer_id_ = _identity.consumer_id_;
    candidate.acquire_.request_id_ = _identity.start_request_id_;
    candidate.media_.socket_path_ = _route.media_socket_path_;
    candidate.media_.producer_uid_ = _route.producer_uid_;
    candidate.media_.limits_ = limits;
    candidate.stop_request_id_ = _identity.stop_request_id_;
    candidate.max_fps_ = integer_fps;
    _config = std::move(candidate);
    return {};
}

}  // namespace vqec::vision::ai
