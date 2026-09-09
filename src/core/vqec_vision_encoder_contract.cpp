#include "vqec/vision/ai/contracts/vqec_vision_encoder_backend.hpp"

#include "vqec/vision/ai/contracts/vqec_vision_preview_limits.hpp"

namespace vqec::vision::ai {

status vqec_vision_ai_core_encct_validate_event(const encoder_event& _event) {
    const bool has_token = _event.token_.cycle_id_ != 0 && _event.token_.job_id_ != 0;
    const bool is_global = _event.token_.cycle_id_ == 0 && _event.token_.job_id_ == 0;
    switch (_event.kind_) {
        case encoder_event_kind::input_complete:
        case encoder_event_kind::output_dropped:
            if (!has_token || _event.output_ || _event.detail_.code_ != status_code::ok) {
                return {status_code::protocol_error, "malformed encoder completion event"};
            }
            break;
        case encoder_event_kind::output_ready:
            if (!has_token || !_event.output_ || _event.detail_.code_ != status_code::ok) {
                return {status_code::protocol_error, "malformed encoder output event"};
            }
            break;
        case encoder_event_kind::fault:
            if ((!has_token && !is_global) || _event.output_ ||
                _event.detail_.code_ == status_code::ok ||
                _event.detail_.code_ == status_code::pending) {
                return {status_code::protocol_error, "malformed encoder fault event"};
            }
            break;
        default:
            return {status_code::protocol_error, "unknown encoder event kind"};
    }
    return {};
}

status vqec_vision_ai_core_encct_validate_input(
    const encoder_input& _input, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry, const submission_ticket& _expected_ticket,
    std::uint64_t _expected_generation) {
    const auto& frame = _input.frame_;
    const auto& geometry = _input.geometry_;
    const auto& ticket = _input.ticket_;
    if (!_input.pixels_ || frame.source_epoch_ == 0 || frame.source_pts_ns_ == UINT64_MAX ||
        ticket.token_.cycle_id_ == 0 || ticket.token_.job_id_ == 0 ||
        ticket.source_epoch_ != frame.source_epoch_ ||
        ticket.source_frame_id_ != frame.frame_id_ ||
        ticket.source_pts_ns_ != frame.source_pts_ns_ || ticket.pipeline_pts_ns_ == UINT64_MAX ||
        _input.dispatch_generation_ == 0 || _expected_generation == 0 ||
        geometry.width_ == 0 || geometry.height_ == 0 ||
        geometry.width_ > preview_limits::g_max_dimension_pixels ||
        geometry.height_ > preview_limits::g_max_dimension_pixels ||
        geometry.width_ % 2 != 0 || geometry.height_ % 2 != 0) {
        return {status_code::invalid_argument, "invalid encoder input metadata or owner"};
    }
    if (frame.camera_id_ != _expected_frame.camera_id_ ||
        frame.channel_id_ != _expected_frame.channel_id_ ||
        frame.source_epoch_ != _expected_frame.source_epoch_ ||
        frame.frame_id_ != _expected_frame.frame_id_ ||
        frame.source_pts_ns_ != _expected_frame.source_pts_ns_ ||
        geometry.width_ != _expected_geometry.width_ ||
        geometry.height_ != _expected_geometry.height_ ||
        ticket.token_.cycle_id_ != _expected_ticket.token_.cycle_id_ ||
        ticket.token_.job_id_ != _expected_ticket.token_.job_id_ ||
        ticket.source_epoch_ != _expected_ticket.source_epoch_ ||
        ticket.source_frame_id_ != _expected_ticket.source_frame_id_ ||
        ticket.source_pts_ns_ != _expected_ticket.source_pts_ns_ ||
        ticket.pipeline_pts_ns_ != _expected_ticket.pipeline_pts_ns_ ||
        _input.dispatch_generation_ != _expected_generation) {
        return {status_code::invalid_state, "encoder input binding mismatch"};
    }
    const auto pixels = static_cast<std::uint64_t>(geometry.width_) * geometry.height_;
    const auto bytes = pixels + pixels / 2; // Packed NV12: Y plus interleaved half-size UV.
    if (bytes > preview_limits::g_max_surface_bytes) {
        return {status_code::resource_exhausted, "encoder input exceeds surface ceiling"};
    }
    if (_input.pixels_->size() != bytes) {
        return {status_code::invalid_argument, "encoder input is not exact packed NV12 size"};
    }
    return {};
}

}  // namespace vqec::vision::ai
