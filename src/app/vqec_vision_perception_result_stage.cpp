#include "vqec_vision_perception_result_stage.hpp"

#include <utility>

namespace vqec::vision::ai {

perception_result_stage::perception_result_stage(
    model_decode_stage& _decoder, tracking_stage& _tracker) noexcept
    : decoder_(_decoder), tracker_(_tracker) {}

status perception_result_stage::vqec_vision_ai_appl_prstg_configure(
    const perception_result_config& _config) {
    if (is_configured_) {
        return {status_code::invalid_state, "perception result stage is already configured"};
    }
    const preview_frame_key validation_frame{_config.camera_id_, _config.channel_id_, 1, 0, 0};
    const auto valid_geometry = vqec_vision_ai_core_pvctr_validate_identity(
        validation_frame, validation_frame, _config.geometry_, _config.geometry_);
    if (valid_geometry.code_ != status_code::ok) {
        return valid_geometry;
    }
    const auto decoder_geometry =
        decoder_.vqec_vision_ai_detec_mdstg_validate_geometry(_config.geometry_);
    if (decoder_geometry.code_ != status_code::ok) {
        return decoder_geometry;
    }
    const auto valid_tracker = tracker_.vqec_vision_ai_track_trkst_validate_activation();
    if (valid_tracker.code_ != status_code::ok) {
        return valid_tracker;
    }
    config_ = _config;
    is_configured_ = true;
    return {};
}

status perception_result_stage::vqec_vision_ai_appl_prstg_process(
    const tensor_result& _result, const submission_ticket& _ticket,
    std::uint64_t _now_monotonic_ns, bool _is_source_gap,
    observation_batch& _tracked) {
    if (!is_configured_) {
        return {status_code::invalid_state, "perception result stage is not configured"};
    }
    if (_ticket.token_.cycle_id_ == 0 || _ticket.token_.job_id_ == 0 ||
        _ticket.source_epoch_ == 0 || _ticket.source_pts_ns_ == UINT64_MAX ||
        _ticket.pipeline_pts_ns_ == UINT64_MAX ||
        _result.pipeline_pts_ns_ != _ticket.pipeline_pts_ns_) {
        return {status_code::invalid_argument, "tensor result and source ticket do not correlate"};
    }
    const preview_frame_key expected_frame{
        config_.camera_id_, config_.channel_id_, _ticket.source_epoch_,
        _ticket.source_frame_id_, _ticket.source_pts_ns_};
    observation_batch detections;
    const auto decoded = decoder_.vqec_vision_ai_detec_mdstg_decode_result(
        _result, expected_frame, detections);
    if (decoded.code_ != status_code::ok) {
        return decoded;
    }
    observation_batch candidate;
    const auto tracked = tracker_.vqec_vision_ai_track_trkst_process(
        detections, _now_monotonic_ns, _is_source_gap, candidate);
    if (tracked.code_ != status_code::ok) {
        return tracked;
    }
    _tracked = std::move(candidate);
    return {};
}

}  // namespace vqec::vision::ai
