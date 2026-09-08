#include "vqec_vision_ring_sink.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preview_limits.hpp"

#include <new>
#include <utility>

#include <camera_ai/common/ai_output.hpp>
#include <camera_ai/common/ring_buffer.hpp>

namespace vqec::vision::ai {
namespace {

static_assert(camera_ai::kRingVersion == 4, "review changed FW ring ABI before integration");
static_assert(camera_ai::kRingParameterSetMaxSize == preview_limits::g_max_parameter_set_bytes,
    "FW parameter-set bound changed");

// Exact legacy writer field values; distinct from codec factory/property names.
constexpr char g_ai_stream_id[] = "ai";
constexpr char g_annex_b_format[] = "byte-stream";

status vqec_vision_ai_fwout_rgsnk_translate_result(const camera_ai::Result& _result) {
    using camera_ai::ErrorCode;
    switch (_result.code) {
        case ErrorCode::Ok: return {};
        case ErrorCode::InvalidArgument: return {status_code::invalid_argument, _result.message};
        case ErrorCode::Busy:
        case ErrorCode::ResourceExhausted:
        case ErrorCode::StorageFull: return {status_code::resource_exhausted, _result.message};
        case ErrorCode::Timeout: return {status_code::timeout, _result.message};
        case ErrorCode::Unsupported: return {status_code::unsupported, _result.message};
        case ErrorCode::PermissionDenied: return {status_code::unauthorized, _result.message};
        case ErrorCode::VersionMismatch: return {status_code::protocol_error, _result.message};
        default: return {status_code::io_error, _result.message};
    }
}

}  // namespace

ring_sink::ring_sink(camera_ai::SharedMemoryFrameRingBuffer& _ring, fw_ring_sink_config _config)
    : ring_(_ring), config_(_config) {}

status vqec_vision_ai_fwout_rgsnk_make_open_options(
    unsigned _detect_index, camera_ai::SharedRingOpenOptions& _options) {
    if (_detect_index > 1) {
        return {status_code::invalid_argument, "unsupported FW detect ring selection"};
    }
    try {
        camera_ai::SharedRingOpenOptions candidate;
        candidate.ring_id = _detect_index == 0 ? camera_ai::ai_output::kDetect0RingId
                                               : camera_ai::ai_output::kDetect1RingId;
        candidate.slot_count = preview_limits::g_encoded_ring_slots;
        candidate.payload_size = preview_limits::g_max_encoded_payload_bytes;
        candidate.create_if_missing = true;
        candidate.replace_existing = false;
        _options = std::move(candidate);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "FW ring options allocation failed"};
    }
    return {};
}

status ring_sink::vqec_vision_ai_fwout_rgsnk_validate_binding() const {
    if (config_.detect_index_ > 1 || config_.mapping_generation_ == 0 ||
        config_.dispatch_generation_ == 0 || config_.source_epoch_ == 0 ||
        config_.geometry_.width_ == 0 || config_.geometry_.height_ == 0 ||
        config_.geometry_.width_ > preview_limits::g_max_dimension_pixels ||
        config_.geometry_.height_ > preview_limits::g_max_dimension_pixels ||
        config_.geometry_.width_ % 2 != 0 || config_.geometry_.height_ % 2 != 0) {
        return {status_code::invalid_argument, "invalid FW ring sink configuration"};
    }
    if (!ring_.is_open() || ring_.mapping_generation() != config_.mapping_generation_) {
        return {status_code::invalid_state, "FW ring is closed or remapped"};
    }
    const auto* expected_id = config_.detect_index_ == 0 ? camera_ai::ai_output::kDetect0RingId
                                                       : camera_ai::ai_output::kDetect1RingId;
    if (ring_.ring_id() != expected_id ||
        ring_.slot_count() != preview_limits::g_encoded_ring_slots ||
        ring_.payload_size() != preview_limits::g_max_encoded_payload_bytes) {
        return {status_code::protocol_error, "FW AI ring identity or layout mismatch"};
    }
    return {};
}

status ring_sink::vqec_vision_ai_cntr_encsk_query_demand(encoded_sink_demand& _demand) {
    const auto valid = vqec_vision_ai_fwout_rgsnk_validate_binding();
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    _demand = {config_.dispatch_generation_, ring_.active_consumer_count()};
    return {};
}

status ring_sink::vqec_vision_ai_cntr_encsk_write(
    const h264_access_unit_view& _unit, std::uint64_t _expected_generation) {
    const auto bound = vqec_vision_ai_fwout_rgsnk_validate_binding();
    if (bound.code_ != status_code::ok) {
        return bound;
    }
    if (_expected_generation != config_.dispatch_generation_) {
        return {status_code::invalid_state, "stale encoded dispatch generation"};
    }
    auto expected_frame = _unit.frame_;
    expected_frame.camera_id_ = config_.camera_id_;
    expected_frame.channel_id_ = config_.channel_id_;
    expected_frame.source_epoch_ = config_.source_epoch_;
    try {
        camera_ai::RingFrameHeader header;
        const auto mapped = vqec_vision_ai_fwout_rgsnk_map_header(
            _unit, expected_frame, config_.geometry_, header);
        if (mapped.code_ != status_code::ok) {
            return mapped;
        }
        return vqec_vision_ai_fwout_rgsnk_translate_result(
            ring_.push(header, _unit.payload_.data_, _unit.payload_.size_));
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "FW ring write allocation failed"};
    }
}

status vqec_vision_ai_fwout_rgsnk_map_header(
    const h264_access_unit_view& _unit, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry, camera_ai::RingFrameHeader& _header) {
    const auto valid = vqec_vision_ai_core_pvctr_validate_access_unit(
        _unit, _expected_frame, _expected_geometry);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    try {
        camera_ai::RingFrameHeader header;
        header.frame_id = _unit.frame_.frame_id_;
        header.timestamp_ns = _unit.frame_.source_pts_ns_;
        header.camera_id = _unit.frame_.camera_id_;
        header.channel_id = _unit.frame_.channel_id_;
        header.stream_id = g_ai_stream_id;
        header.width = _unit.geometry_.width_;
        header.height = _unit.geometry_.height_;
        header.stride = 0;
        header.format = g_annex_b_format;
        header.codec = camera_ai::ai_output::kCodec;
        header.is_keyframe = _unit.is_keyframe_;
        if (_unit.sps_.size_ != 0) {
            header.h264_sps.assign(_unit.sps_.data_, _unit.sps_.data_ + _unit.sps_.size_);
        }
        if (_unit.pps_.size_ != 0) {
            header.h264_pps.assign(_unit.pps_.data_, _unit.pps_.data_ + _unit.pps_.size_);
        }
        header.data_size = static_cast<std::uint32_t>(_unit.payload_.size_);
        _header = std::move(header);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "FW ring metadata allocation failed"};
    }
    return {};
}

}  // namespace vqec::vision::ai
