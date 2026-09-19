#include "vqec/vision/ai/contracts/output/vqec_vision_trajectory_codec.hpp"

#include <array>
#include <limits>

namespace vqec::vision::ai {
namespace {

constexpr std::array<std::uint8_t, 4> g_trajectory_codec_magic{'V', 'Q', 'T', 'R'};
constexpr std::size_t g_trajectory_codec_fixed_header_bytes = 12U;
constexpr std::size_t g_trajectory_codec_checksum_bytes = 4U;
constexpr std::uint32_t g_trajectory_crc_polynomial = 0xedb88320U;
constexpr std::uint64_t g_trajectory_varint_continuation = 0x80U;
constexpr std::uint64_t g_trajectory_varint_value_mask = 0x7fU;
constexpr std::size_t g_trajectory_max_varint_bytes = 10U;

void vqec_vision_ai_core_trcod_append_u32(
    std::vector<std::uint8_t>& _bytes, std::uint32_t _value) {
    for (std::size_t index = 0U; index < sizeof(_value); ++index) {
        _bytes.push_back(static_cast<std::uint8_t>(_value & 0xffU));
        _value >>= 8U;
    }
}

bool vqec_vision_ai_core_trcod_read_u32(
    const std::vector<std::uint8_t>& _bytes, std::size_t& _offset, std::uint32_t& _value) {
    if (_offset > _bytes.size() || _bytes.size() - _offset < sizeof(_value)) {
        return false;
    }
    _value = 0U;
    for (std::size_t index = 0U; index < sizeof(_value); ++index) {
        _value |= static_cast<std::uint32_t>(_bytes[_offset + index]) << (index * 8U);
    }
    _offset += sizeof(_value);
    return true;
}

void vqec_vision_ai_core_trcod_append_varint(
    std::vector<std::uint8_t>& _bytes, std::uint64_t _value) {
    while (_value >= g_trajectory_varint_continuation) {
        _bytes.push_back(static_cast<std::uint8_t>(
            (_value & g_trajectory_varint_value_mask) | g_trajectory_varint_continuation));
        _value >>= 7U;
    }
    _bytes.push_back(static_cast<std::uint8_t>(_value));
}

bool vqec_vision_ai_core_trcod_read_varint(
    const std::vector<std::uint8_t>& _bytes, std::size_t _end_offset,
    std::size_t& _offset, std::uint64_t& _value) {
    _value = 0U;
    for (std::size_t index = 0U; index < g_trajectory_max_varint_bytes; ++index) {
        if (_offset >= _end_offset) {
            return false;
        }
        const auto byte = _bytes[_offset++];
        if (index == g_trajectory_max_varint_bytes - 1U && byte > 1U) {
            return false;
        }
        _value |= static_cast<std::uint64_t>(byte & g_trajectory_varint_value_mask) <<
            (index * 7U);
        if ((byte & g_trajectory_varint_continuation) == 0U) {
            return true;
        }
    }
    return false;
}

std::uint64_t vqec_vision_ai_core_trcod_encode_signed(std::int64_t _value) {
    const auto bits = static_cast<std::uint64_t>(_value);
    return (bits << 1U) ^ static_cast<std::uint64_t>(_value >> 63U);
}

std::int64_t vqec_vision_ai_core_trcod_decode_signed(std::uint64_t _value) {
    const auto magnitude = static_cast<std::int64_t>(_value >> 1U);
    return (_value & 1U) == 0U ? magnitude : -magnitude - 1;
}

std::uint32_t vqec_vision_ai_core_trcod_calculate_crc32(
    const std::vector<std::uint8_t>& _bytes, std::size_t _length) {
    std::uint32_t crc = std::numeric_limits<std::uint32_t>::max();
    for (std::size_t index = 0U; index < _length; ++index) {
        crc ^= _bytes[index];
        for (unsigned int bit = 0U; bit < 8U; ++bit) {
            const auto mask = static_cast<std::uint32_t>(
                -static_cast<std::int32_t>(crc & 1U));
            crc = (crc >> 1U) ^ (g_trajectory_crc_polynomial & mask);
        }
    }
    return ~crc;
}

bool vqec_vision_ai_core_trcod_append_point(
    const trajectory_point& _point, std::uint64_t _previous_frame,
    std::uint64_t _previous_pts, std::vector<std::uint8_t>& _bytes,
    std::size_t _maximum_encoded_bytes) {
    vqec_vision_ai_core_trcod_append_varint(_bytes, _point.frame_id_ - _previous_frame);
    vqec_vision_ai_core_trcod_append_varint(_bytes, _point.source_pts_ns_ - _previous_pts);
    vqec_vision_ai_core_trcod_append_varint(_bytes, _point.has_capture_utc_ ? 1U : 0U);
    if (_point.has_capture_utc_) {
        vqec_vision_ai_core_trcod_append_varint(
            _bytes, vqec_vision_ai_core_trcod_encode_signed(_point.capture_utc_ns_));
    }
    vqec_vision_ai_core_trcod_append_varint(
        _bytes, vqec_vision_ai_core_trcod_encode_signed(_point.anchor_x_));
    vqec_vision_ai_core_trcod_append_varint(
        _bytes, vqec_vision_ai_core_trcod_encode_signed(_point.anchor_y_));
    vqec_vision_ai_core_trcod_append_varint(_bytes, _point.flags_);
    if ((_point.flags_ & static_cast<std::uint32_t>(trajectory_point_flag::has_box)) != 0U) {
        vqec_vision_ai_core_trcod_append_varint(
            _bytes, vqec_vision_ai_core_trcod_encode_signed(_point.box_left_));
        vqec_vision_ai_core_trcod_append_varint(
            _bytes, vqec_vision_ai_core_trcod_encode_signed(_point.box_top_));
        vqec_vision_ai_core_trcod_append_varint(
            _bytes, vqec_vision_ai_core_trcod_encode_signed(_point.box_right_));
        vqec_vision_ai_core_trcod_append_varint(
            _bytes, vqec_vision_ai_core_trcod_encode_signed(_point.box_bottom_));
    }
    return _bytes.size() <= _maximum_encoded_bytes - g_trajectory_codec_checksum_bytes;
}

bool vqec_vision_ai_core_trcod_read_signed(
    const std::vector<std::uint8_t>& _bytes, std::size_t _end_offset,
    std::size_t& _offset, std::int64_t& _value) {
    std::uint64_t encoded = 0U;
    if (!vqec_vision_ai_core_trcod_read_varint(_bytes, _end_offset, _offset, encoded)) {
        return false;
    }
    _value = vqec_vision_ai_core_trcod_decode_signed(encoded);
    return true;
}

bool vqec_vision_ai_core_trcod_read_point(
    const std::vector<std::uint8_t>& _bytes, std::size_t _end_offset,
    std::size_t& _offset, std::uint64_t _previous_frame,
    std::uint64_t _previous_pts, trajectory_point& _point) {
    std::uint64_t frame_delta = 0U;
    std::uint64_t pts_delta = 0U;
    std::uint64_t has_utc = 0U;
    std::uint64_t flags = 0U;
    std::int64_t signed_value = 0;
    if (!vqec_vision_ai_core_trcod_read_varint(
            _bytes, _end_offset, _offset, frame_delta) ||
        !vqec_vision_ai_core_trcod_read_varint(
            _bytes, _end_offset, _offset, pts_delta) ||
        !vqec_vision_ai_core_trcod_read_varint(_bytes, _end_offset, _offset, has_utc) ||
        frame_delta == 0U || pts_delta == 0U || has_utc > 1U ||
        frame_delta > std::numeric_limits<std::uint64_t>::max() - _previous_frame ||
        pts_delta > std::numeric_limits<std::uint64_t>::max() - _previous_pts) {
        return false;
    }
    _point.frame_id_ = _previous_frame + frame_delta;
    _point.source_pts_ns_ = _previous_pts + pts_delta;
    _point.has_capture_utc_ = has_utc != 0U;
    if (_point.has_capture_utc_) {
        if (!vqec_vision_ai_core_trcod_read_signed(
                _bytes, _end_offset, _offset, signed_value) || signed_value < 0) {
            return false;
        }
        _point.capture_utc_ns_ = signed_value;
    }
    if (!vqec_vision_ai_core_trcod_read_signed(
            _bytes, _end_offset, _offset, signed_value) ||
        signed_value < std::numeric_limits<std::int32_t>::min() ||
        signed_value > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    _point.anchor_x_ = static_cast<std::int32_t>(signed_value);
    if (!vqec_vision_ai_core_trcod_read_signed(
            _bytes, _end_offset, _offset, signed_value) ||
        signed_value < std::numeric_limits<std::int32_t>::min() ||
        signed_value > std::numeric_limits<std::int32_t>::max() ||
        !vqec_vision_ai_core_trcod_read_varint(_bytes, _end_offset, _offset, flags) ||
        flags > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    _point.anchor_y_ = static_cast<std::int32_t>(signed_value);
    _point.flags_ = static_cast<std::uint32_t>(flags);
    std::array<std::int32_t*, 4> box_fields{
        &_point.box_left_, &_point.box_top_, &_point.box_right_, &_point.box_bottom_};
    if ((_point.flags_ & static_cast<std::uint32_t>(trajectory_point_flag::has_box)) != 0U) {
        for (auto* field : box_fields) {
            if (!vqec_vision_ai_core_trcod_read_signed(
                    _bytes, _end_offset, _offset, signed_value) ||
                signed_value < std::numeric_limits<std::int32_t>::min() ||
                signed_value > std::numeric_limits<std::int32_t>::max()) {
                return false;
            }
            *field = static_cast<std::int32_t>(signed_value);
        }
    }
    return true;
}

}  // namespace

status vqec_vision_ai_cntr_trcod_encode_points(
    const trajectory_chunk& _chunk, std::size_t _maximum_encoded_bytes,
    encoded_trajectory_points& _encoded) {
    const auto validation = vqec_vision_ai_cntr_stmet_validate_trajectory_chunk(_chunk);
    if (validation.code_ != status_code::ok) {
        return validation;
    }
    if (_maximum_encoded_bytes <=
            g_trajectory_codec_fixed_header_bytes + g_trajectory_codec_checksum_bytes ||
        _maximum_encoded_bytes > static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max())) {
        return {status_code::invalid_argument, "trajectory encoded byte budget is invalid"};
    }
    encoded_trajectory_points encoded;
    encoded.bytes_.reserve(std::min(_maximum_encoded_bytes,
        g_trajectory_codec_fixed_header_bytes + _chunk.points_.size() * 32U));
    encoded.bytes_.insert(encoded.bytes_.end(),
        g_trajectory_codec_magic.begin(), g_trajectory_codec_magic.end());
    vqec_vision_ai_core_trcod_append_u32(encoded.bytes_, g_trajectory_codec_schema_version);
    vqec_vision_ai_core_trcod_append_u32(
        encoded.bytes_, static_cast<std::uint32_t>(_chunk.points_.size()));
    std::uint64_t previous_frame = 0U;
    std::uint64_t previous_pts = 0U;
    for (const auto& point : _chunk.points_) {
        if (!vqec_vision_ai_core_trcod_append_point(point, previous_frame, previous_pts,
                encoded.bytes_, _maximum_encoded_bytes)) {
            return {status_code::resource_exhausted, "trajectory encoded byte budget exceeded"};
        }
        previous_frame = point.frame_id_;
        previous_pts = point.source_pts_ns_;
    }
    encoded.checksum_crc32_ = vqec_vision_ai_core_trcod_calculate_crc32(
        encoded.bytes_, encoded.bytes_.size());
    vqec_vision_ai_core_trcod_append_u32(encoded.bytes_, encoded.checksum_crc32_);
    _encoded = std::move(encoded);
    return {};
}

status vqec_vision_ai_cntr_trcod_decode_points(
    const encoded_trajectory_points& _encoded, std::size_t _maximum_encoded_bytes,
    std::size_t _maximum_points, std::vector<trajectory_point>& _points) {
    if (_encoded.schema_version_ != g_trajectory_codec_schema_version ||
        _encoded.bytes_.size() > _maximum_encoded_bytes ||
        _encoded.bytes_.size() <
            g_trajectory_codec_fixed_header_bytes + g_trajectory_codec_checksum_bytes ||
        _maximum_points == 0U || _maximum_points > g_spatiotemporal_max_points_per_chunk) {
        return {status_code::invalid_argument, "encoded trajectory envelope is invalid"};
    }
    if (!std::equal(g_trajectory_codec_magic.begin(), g_trajectory_codec_magic.end(),
            _encoded.bytes_.begin())) {
        return {status_code::protocol_error, "trajectory codec magic differs"};
    }
    const auto payload_end = _encoded.bytes_.size() - g_trajectory_codec_checksum_bytes;
    std::size_t checksum_offset = payload_end;
    std::uint32_t stored_checksum = 0U;
    if (!vqec_vision_ai_core_trcod_read_u32(
            _encoded.bytes_, checksum_offset, stored_checksum) ||
        stored_checksum != _encoded.checksum_crc32_ ||
        stored_checksum != vqec_vision_ai_core_trcod_calculate_crc32(
            _encoded.bytes_, payload_end)) {
        return {status_code::protocol_error, "trajectory checksum differs"};
    }
    std::size_t offset = g_trajectory_codec_magic.size();
    std::uint32_t schema_version = 0U;
    std::uint32_t point_count = 0U;
    if (!vqec_vision_ai_core_trcod_read_u32(_encoded.bytes_, offset, schema_version) ||
        !vqec_vision_ai_core_trcod_read_u32(_encoded.bytes_, offset, point_count) ||
        schema_version != g_trajectory_codec_schema_version || point_count == 0U ||
        point_count > _maximum_points) {
        return {status_code::protocol_error, "trajectory codec header is invalid"};
    }
    std::vector<trajectory_point> points;
    points.reserve(point_count);
    std::uint64_t previous_frame = 0U;
    std::uint64_t previous_pts = 0U;
    for (std::uint32_t index = 0U; index < point_count; ++index) {
        trajectory_point point;
        if (!vqec_vision_ai_core_trcod_read_point(
                _encoded.bytes_, payload_end, offset, previous_frame, previous_pts, point)) {
            return {status_code::protocol_error, "trajectory point encoding is invalid"};
        }
        previous_frame = point.frame_id_;
        previous_pts = point.source_pts_ns_;
        points.push_back(point);
    }
    if (offset != payload_end) {
        return {status_code::protocol_error, "trajectory encoding has trailing bytes"};
    }
    _points = std::move(points);
    return {};
}

}  // namespace vqec::vision::ai
