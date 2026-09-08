#include "vqec_vision_legacy_wire.hpp"

#include <cstring>

namespace vqec::vision::ai {
namespace {

template <typename value_type>
value_type vqec_vision_ai_camer_lwire_read_field(const std::uint8_t* _bytes) {
    value_type value{};
    std::memcpy(&value, _bytes, sizeof(value));
    return value;
}

}  // namespace

status vqec_vision_ai_camer_lwire_make_limits(
    const source_deployment_config& _source, std::uint32_t _nv12_format_value,
    legacy_frame_limits& _limits) {
    if (_source.raw_source_ref_.empty() || _nv12_format_value == 0 ||
        _source.profile_.width_ == 0 || _source.profile_.height_ == 0 ||
        _source.profile_.width_ > deployment_limits::g_max_dimension_pixels ||
        _source.profile_.height_ > deployment_limits::g_max_dimension_pixels ||
        _source.profile_.width_ % 2 != 0 || _source.profile_.height_ % 2 != 0) {
        return {status_code::invalid_argument, "invalid FW RAW deployment source"};
    }
    const auto pixels = static_cast<std::uint64_t>(_source.profile_.width_) *
        _source.profile_.height_;
    const auto packed_bytes = pixels + pixels / 2;
    if (_source.memory_.max_frame_allocation_bytes_ < packed_bytes ||
        _source.memory_.max_frame_allocation_bytes_ >
            deployment_limits::g_max_frame_allocation_bytes) {
        return {status_code::invalid_argument, "invalid FW RAW frame budget"};
    }
    legacy_frame_limits candidate;
    candidate.nv12_format_value_ = _nv12_format_value;
    candidate.max_width_ = _source.profile_.width_;
    candidate.max_height_ = _source.profile_.height_;
    candidate.max_allocation_bytes_ = _source.memory_.max_frame_allocation_bytes_;
    _limits = candidate;
    return {};
}

status vqec_vision_ai_camer_lwire_decode_frame(
    const std::uint8_t* _packet, std::size_t _size, const legacy_frame_limits& _limits,
    frame_descriptor& _frame) {
    if (_limits.nv12_format_value_ == 0 || _limits.max_width_ == 0 ||
        _limits.max_height_ == 0 || _limits.max_allocation_bytes_ == 0) {
        return {status_code::invalid_argument, "missing legacy ABI or resource limits"};
    }
    if (_packet == nullptr || _size != legacy_wire_layout::g_header_bytes) {
        return {status_code::protocol_error, "legacy header must be exactly 104 bytes"};
    }
    frame_descriptor frame;
    frame.buffer_id_ = vqec_vision_ai_camer_lwire_read_field<std::uint64_t>(_packet);
    frame.width_ = vqec_vision_ai_camer_lwire_read_field<std::uint32_t>(_packet + legacy_wire_layout::g_width_offset);
    frame.height_ = vqec_vision_ai_camer_lwire_read_field<std::uint32_t>(_packet + legacy_wire_layout::g_height_offset);
    const auto format = vqec_vision_ai_camer_lwire_read_field<std::uint32_t>(_packet + legacy_wire_layout::g_format_offset);
    const auto planes = vqec_vision_ai_camer_lwire_read_field<std::uint32_t>(_packet + legacy_wire_layout::g_plane_count_offset);
    if (format != _limits.nv12_format_value_ || planes != legacy_wire_layout::g_nv12_planes) {
        return {status_code::unsupported, "only the pinned two-plane NV12 ABI is accepted"};
    }
    if (frame.buffer_id_ == 0 || frame.width_ == 0 || frame.height_ == 0 ||
        (frame.width_ & 1U) != 0 || (frame.height_ & 1U) != 0 ||
        frame.width_ > _limits.max_width_ || frame.height_ > _limits.max_height_) {
        return {status_code::protocol_error, "invalid frame identity or dimensions"};
    }
    frame.view_size_bytes_ =
        vqec_vision_ai_camer_lwire_read_field<std::uint64_t>(_packet + legacy_wire_layout::g_view_size_offset);
    frame.memory_offset_bytes_ =
        vqec_vision_ai_camer_lwire_read_field<std::uint64_t>(_packet + legacy_wire_layout::g_memory_offset_offset);
    frame.allocation_size_bytes_ =
        vqec_vision_ai_camer_lwire_read_field<std::uint64_t>(_packet + legacy_wire_layout::g_allocation_size_offset);
    if (frame.allocation_size_bytes_ == 0 ||
        frame.allocation_size_bytes_ > _limits.max_allocation_bytes_ ||
        frame.memory_offset_bytes_ > frame.allocation_size_bytes_ ||
        frame.view_size_bytes_ > frame.allocation_size_bytes_ - frame.memory_offset_bytes_) {
        return {status_code::protocol_error, "invalid allocation/view range"};
    }
    std::array<std::uint64_t, legacy_wire_layout::g_nv12_planes> ends{};
    for (std::size_t plane = 0; plane < legacy_wire_layout::g_nv12_planes; ++plane) {
        frame.offsets_[plane] =
            vqec_vision_ai_camer_lwire_read_field<std::uint32_t>(_packet + legacy_wire_layout::g_plane_offsets_offset + plane * legacy_wire_layout::g_plane_field_bytes);
        frame.strides_[plane] =
            vqec_vision_ai_camer_lwire_read_field<std::int32_t>(_packet + legacy_wire_layout::g_plane_strides_offset + plane * legacy_wire_layout::g_plane_field_bytes);
        if (frame.strides_[plane] <= 0 ||
            static_cast<std::uint32_t>(frame.strides_[plane]) < frame.width_) {
            return {status_code::protocol_error, "invalid NV12 stride"};
        }
        const std::uint64_t rows = plane == 0 ? frame.height_ : frame.height_ / 2;
        // uint32 dimensions * positive int32 stride + uint32 offset fits uint64.
        ends[plane] = frame.offsets_[plane] +
            rows * static_cast<std::uint64_t>(frame.strides_[plane]);
        if (ends[plane] > frame.view_size_bytes_) {
            return {status_code::protocol_error, "plane span exceeds valid view"};
        }
    }
    if (frame.offsets_[0] < ends[1] && frame.offsets_[1] < ends[0]) {
        return {status_code::protocol_error, "NV12 plane spans overlap"};
    }
    frame.pts_ns_ = vqec_vision_ai_camer_lwire_read_field<std::uint64_t>(_packet + legacy_wire_layout::g_pts_offset);
    frame.dts_ns_ = vqec_vision_ai_camer_lwire_read_field<std::uint64_t>(_packet + legacy_wire_layout::g_dts_offset);
    frame.duration_ns_ = vqec_vision_ai_camer_lwire_read_field<std::uint64_t>(_packet + legacy_wire_layout::g_duration_offset);
    _frame = frame;
    return {};
}

}  // namespace vqec::vision::ai
