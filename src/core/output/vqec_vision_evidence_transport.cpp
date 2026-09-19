#include "vqec/vision/ai/contracts/output/vqec_vision_evidence_transport.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::uint32_t g_evidence_wire_magic = 0x31514556U;
constexpr std::uint32_t g_evidence_command_kind = 1U;
constexpr std::uint32_t g_evidence_receipt_kind = 2U;

struct evidence_wire_reader {
    const std::uint8_t* data_{nullptr};
    std::size_t size_{0};
    std::size_t offset_{0};
};

bool vqec_vision_ai_core_evtrn_is_bounded_text(
    const std::string& _value, std::size_t _maximum, bool _allow_empty) noexcept {
    if ((!_allow_empty && _value.empty()) || _value.size() > _maximum) {
        return false;
    }
    return std::all_of(_value.begin(), _value.end(), [](unsigned char _character) {
        return _character >= 0x20U && _character <= 0x7EU;
    });
}

void vqec_vision_ai_core_evtrn_append_u32(
    std::vector<std::uint8_t>& _wire, std::uint32_t _value) {
    for (std::size_t index = 0; index < sizeof(_value); ++index) {
        _wire.push_back(static_cast<std::uint8_t>(_value >> (index * 8U)));
    }
}

void vqec_vision_ai_core_evtrn_append_u64(
    std::vector<std::uint8_t>& _wire, std::uint64_t _value) {
    for (std::size_t index = 0; index < sizeof(_value); ++index) {
        _wire.push_back(static_cast<std::uint8_t>(_value >> (index * 8U)));
    }
}

void vqec_vision_ai_core_evtrn_append_string(
    std::vector<std::uint8_t>& _wire, const std::string& _value) {
    vqec_vision_ai_core_evtrn_append_u32(
        _wire, static_cast<std::uint32_t>(_value.size()));
    _wire.insert(_wire.end(), _value.begin(), _value.end());
}

bool vqec_vision_ai_core_evtrn_read_u32(
    evidence_wire_reader& _reader, std::uint32_t& _value) noexcept {
    if (_reader.offset_ > _reader.size_ ||
        sizeof(_value) > _reader.size_ - _reader.offset_) {
        return false;
    }
    _value = 0U;
    for (std::size_t index = 0; index < sizeof(_value); ++index) {
        _value |= static_cast<std::uint32_t>(
            _reader.data_[_reader.offset_++]) << (index * 8U);
    }
    return true;
}

bool vqec_vision_ai_core_evtrn_read_u64(
    evidence_wire_reader& _reader, std::uint64_t& _value) noexcept {
    if (_reader.offset_ > _reader.size_ ||
        sizeof(_value) > _reader.size_ - _reader.offset_) {
        return false;
    }
    _value = 0U;
    for (std::size_t index = 0; index < sizeof(_value); ++index) {
        _value |= static_cast<std::uint64_t>(
            _reader.data_[_reader.offset_++]) << (index * 8U);
    }
    return true;
}

bool vqec_vision_ai_core_evtrn_read_string(
    evidence_wire_reader& _reader, std::size_t _maximum,
    std::string& _value) {
    std::uint32_t length = 0U;
    if (!vqec_vision_ai_core_evtrn_read_u32(_reader, length) ||
        length > _maximum || _reader.offset_ > _reader.size_ ||
        length > _reader.size_ - _reader.offset_) {
        return false;
    }
    _value.assign(reinterpret_cast<const char*>(_reader.data_ + _reader.offset_), length);
    _reader.offset_ += length;
    return true;
}

void vqec_vision_ai_core_evtrn_append_header(
    std::vector<std::uint8_t>& _wire, std::uint32_t _kind) {
    vqec_vision_ai_core_evtrn_append_u32(_wire, g_evidence_wire_magic);
    vqec_vision_ai_core_evtrn_append_u32(_wire, _kind);
    vqec_vision_ai_core_evtrn_append_u32(
        _wire, evidence_transport_limits::g_protocol_major);
    vqec_vision_ai_core_evtrn_append_u32(
        _wire, evidence_transport_limits::g_protocol_minor);
}

bool vqec_vision_ai_core_evtrn_read_header(
    evidence_wire_reader& _reader, std::uint32_t _expected_kind) noexcept {
    std::uint32_t magic = 0U;
    std::uint32_t kind = 0U;
    std::uint32_t major = 0U;
    std::uint32_t minor = 0U;
    return vqec_vision_ai_core_evtrn_read_u32(_reader, magic) &&
        vqec_vision_ai_core_evtrn_read_u32(_reader, kind) &&
        vqec_vision_ai_core_evtrn_read_u32(_reader, major) &&
        vqec_vision_ai_core_evtrn_read_u32(_reader, minor) &&
        magic == g_evidence_wire_magic && kind == _expected_kind &&
        major == evidence_transport_limits::g_protocol_major &&
        minor == evidence_transport_limits::g_protocol_minor;
}

status vqec_vision_ai_core_evtrn_validate_receipt_envelope(
    const evidence_receipt& _receipt, status_code _failure_code) {
    const auto state = static_cast<std::uint8_t>(_receipt.state_);
    if (_receipt.protocol_major_ != evidence_transport_limits::g_protocol_major ||
        _receipt.protocol_minor_ != evidence_transport_limits::g_protocol_minor ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _receipt.request_id_, feature_event_limits::g_max_identifier_bytes) ||
        _receipt.event_revision_ == 0U || state == 0U ||
        state > static_cast<std::uint8_t>(evidence_receipt_state::expired) ||
        !vqec_vision_ai_core_evtrn_is_bounded_text(
            _receipt.media_id_, evidence_transport_limits::g_max_media_id_bytes, true) ||
        !vqec_vision_ai_core_evtrn_is_bounded_text(
            _receipt.reason_, evidence_transport_limits::g_max_reason_bytes, true) ||
        (_receipt.actual_end_ns_ != 0U &&
         _receipt.actual_end_ns_ < _receipt.actual_begin_ns_)) {
        return {_failure_code, "invalid evidence receipt envelope"};
    }
    if ((_receipt.state_ == evidence_receipt_state::ready ||
         _receipt.state_ == evidence_receipt_state::partial) &&
        _receipt.media_id_.empty()) {
        return {_failure_code, "terminal evidence receipt has no media identity"};
    }
    return {};
}

}  // namespace

status vqec_vision_ai_core_evtrn_make_command(
    const feature_event& _event, evidence_command& _command) {
    if (_event.evidence_request_id_.empty() || _event.policy_revision_ == 0U) {
        return {status_code::invalid_argument,
            "event does not contain an authorized evidence intent"};
    }
    try {
        evidence_command candidate;
        candidate.request_id_ = _event.evidence_request_id_;
        candidate.event_id_ = _event.event_id_;
        candidate.event_revision_ = _event.episode_revision_;
        candidate.source_id_ = _event.source_id_;
        candidate.feature_id_ = _event.feature_id_;
        candidate.schema_id_ = _event.event_schema_id_;
        candidate.schema_version_ = _event.event_schema_version_;
        candidate.frame_ = _event.frame_;
        candidate.occurred_at_ns_ = _event.occurred_at_ns_;
        candidate.policy_revision_ = _event.policy_revision_;
        candidate.config_revision_ = _event.config_revision_;
        candidate.fields_ = _event.fields_;
        const auto valid = vqec_vision_ai_core_evtrn_validate_command(candidate);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        _command = std::move(candidate);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "evidence command allocation failed"};
    }
    return {};
}

status vqec_vision_ai_core_evtrn_validate_command(
    const evidence_command& _command) {
    if (_command.protocol_major_ != evidence_transport_limits::g_protocol_major ||
        _command.protocol_minor_ != evidence_transport_limits::g_protocol_minor ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _command.request_id_, feature_event_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _command.event_id_, feature_event_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _command.source_id_, feature_event_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _command.feature_id_, feature_event_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _command.schema_id_, feature_event_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _command.schema_version_, feature_event_limits::g_max_identifier_bytes) ||
        _command.event_revision_ == 0U || _command.frame_.source_epoch_ == 0U ||
        _command.frame_.frame_id_ == 0U ||
        _command.frame_.source_pts_ns_ == UINT64_MAX ||
        _command.occurred_at_ns_ == UINT64_MAX || _command.policy_revision_ == 0U ||
        _command.config_revision_ == 0U ||
        _command.fields_.size() > feature_event_limits::g_max_fields_per_event) {
        return {status_code::invalid_argument, "invalid evidence command envelope"};
    }
    for (const auto& field : _command.fields_) {
        if (!vqec_vision_ai_cntr_ident_is_valid(
                field.schema_id_, feature_event_limits::g_max_identifier_bytes) ||
            !vqec_vision_ai_cntr_ident_is_valid(
                field.schema_version_, feature_event_limits::g_max_identifier_bytes) ||
            !vqec_vision_ai_core_evtrn_is_bounded_text(
                field.value_, feature_event_limits::g_max_field_value_bytes, true) ||
            !std::isfinite(field.confidence_) || field.confidence_ < 0.0F ||
            field.confidence_ > 1.0F) {
            return {status_code::invalid_argument, "invalid evidence command field"};
        }
    }
    return {};
}

status vqec_vision_ai_core_evtrn_validate_receipt(
    const evidence_receipt& _receipt, const evidence_command& _command) {
    const auto envelope = vqec_vision_ai_core_evtrn_validate_receipt_envelope(
        _receipt, status_code::protocol_error);
    if (envelope.code_ != status_code::ok) {
        return envelope;
    }
    if (_receipt.request_id_ != _command.request_id_ ||
        _receipt.event_revision_ != _command.event_revision_) {
        return {status_code::protocol_error, "invalid evidence receipt"};
    }
    return {};
}

status vqec_vision_ai_core_evtrn_encode_command(
    const evidence_command& _command, std::vector<std::uint8_t>& _wire) {
    const auto valid = vqec_vision_ai_core_evtrn_validate_command(_command);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    try {
        std::vector<std::uint8_t> wire;
        wire.reserve(512U + _command.fields_.size() * 64U);
        vqec_vision_ai_core_evtrn_append_header(wire, g_evidence_command_kind);
        vqec_vision_ai_core_evtrn_append_string(wire, _command.request_id_);
        vqec_vision_ai_core_evtrn_append_string(wire, _command.event_id_);
        vqec_vision_ai_core_evtrn_append_u64(wire, _command.event_revision_);
        vqec_vision_ai_core_evtrn_append_string(wire, _command.source_id_);
        vqec_vision_ai_core_evtrn_append_string(wire, _command.feature_id_);
        vqec_vision_ai_core_evtrn_append_string(wire, _command.schema_id_);
        vqec_vision_ai_core_evtrn_append_string(wire, _command.schema_version_);
        vqec_vision_ai_core_evtrn_append_u32(wire, _command.frame_.camera_id_);
        vqec_vision_ai_core_evtrn_append_u32(wire, _command.frame_.channel_id_);
        vqec_vision_ai_core_evtrn_append_u64(wire, _command.frame_.source_epoch_);
        vqec_vision_ai_core_evtrn_append_u64(wire, _command.frame_.frame_id_);
        vqec_vision_ai_core_evtrn_append_u64(wire, _command.frame_.source_pts_ns_);
        vqec_vision_ai_core_evtrn_append_u64(wire, _command.occurred_at_ns_);
        vqec_vision_ai_core_evtrn_append_u64(wire, _command.policy_revision_);
        vqec_vision_ai_core_evtrn_append_u64(wire, _command.config_revision_);
        vqec_vision_ai_core_evtrn_append_u32(
            wire, static_cast<std::uint32_t>(_command.fields_.size()));
        for (const auto& field : _command.fields_) {
            vqec_vision_ai_core_evtrn_append_string(wire, field.schema_id_);
            vqec_vision_ai_core_evtrn_append_string(wire, field.schema_version_);
            vqec_vision_ai_core_evtrn_append_string(wire, field.value_);
            std::uint32_t confidence = 0U;
            static_assert(sizeof(confidence) == sizeof(field.confidence_),
                "float wire size mismatch");
            std::memcpy(&confidence, &field.confidence_, sizeof(confidence));
            vqec_vision_ai_core_evtrn_append_u32(wire, confidence);
            vqec_vision_ai_core_evtrn_append_u32(
                wire, static_cast<std::uint32_t>(field.quality_));
        }
        if (wire.size() > evidence_transport_limits::g_max_message_bytes) {
            return {status_code::resource_exhausted,
                "evidence command exceeds wire bound"};
        }
        _wire = std::move(wire);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "evidence command serialization failed"};
    }
    return {};
}

status vqec_vision_ai_core_evtrn_decode_command(
    const std::uint8_t* _wire, std::size_t _wire_bytes,
    evidence_command& _command) {
    if (_wire == nullptr || _wire_bytes == 0U ||
        _wire_bytes > evidence_transport_limits::g_max_message_bytes) {
        return {status_code::invalid_argument, "invalid evidence command wire size"};
    }
    try {
        evidence_wire_reader reader{_wire, _wire_bytes, 0U};
        evidence_command candidate;
        std::uint32_t field_count = 0U;
        if (!vqec_vision_ai_core_evtrn_read_header(reader, g_evidence_command_kind) ||
            !vqec_vision_ai_core_evtrn_read_string(reader,
                feature_event_limits::g_max_identifier_bytes, candidate.request_id_) ||
            !vqec_vision_ai_core_evtrn_read_string(reader,
                feature_event_limits::g_max_identifier_bytes, candidate.event_id_) ||
            !vqec_vision_ai_core_evtrn_read_u64(reader, candidate.event_revision_) ||
            !vqec_vision_ai_core_evtrn_read_string(reader,
                feature_event_limits::g_max_identifier_bytes, candidate.source_id_) ||
            !vqec_vision_ai_core_evtrn_read_string(reader,
                feature_event_limits::g_max_identifier_bytes, candidate.feature_id_) ||
            !vqec_vision_ai_core_evtrn_read_string(reader,
                feature_event_limits::g_max_identifier_bytes, candidate.schema_id_) ||
            !vqec_vision_ai_core_evtrn_read_string(reader,
                feature_event_limits::g_max_identifier_bytes, candidate.schema_version_) ||
            !vqec_vision_ai_core_evtrn_read_u32(reader, candidate.frame_.camera_id_) ||
            !vqec_vision_ai_core_evtrn_read_u32(reader, candidate.frame_.channel_id_) ||
            !vqec_vision_ai_core_evtrn_read_u64(reader, candidate.frame_.source_epoch_) ||
            !vqec_vision_ai_core_evtrn_read_u64(reader, candidate.frame_.frame_id_) ||
            !vqec_vision_ai_core_evtrn_read_u64(reader, candidate.frame_.source_pts_ns_) ||
            !vqec_vision_ai_core_evtrn_read_u64(reader, candidate.occurred_at_ns_) ||
            !vqec_vision_ai_core_evtrn_read_u64(reader, candidate.policy_revision_) ||
            !vqec_vision_ai_core_evtrn_read_u64(reader, candidate.config_revision_) ||
            !vqec_vision_ai_core_evtrn_read_u32(reader, field_count) ||
            field_count > feature_event_limits::g_max_fields_per_event) {
            return {status_code::protocol_error, "malformed evidence command wire"};
        }
        candidate.fields_.reserve(field_count);
        for (std::uint32_t index = 0U; index < field_count; ++index) {
            feature_event_field field;
            std::uint32_t confidence = 0U;
            std::uint32_t quality = 0U;
            if (!vqec_vision_ai_core_evtrn_read_string(reader,
                    feature_event_limits::g_max_identifier_bytes, field.schema_id_) ||
                !vqec_vision_ai_core_evtrn_read_string(reader,
                    feature_event_limits::g_max_identifier_bytes,
                    field.schema_version_) ||
                !vqec_vision_ai_core_evtrn_read_string(reader,
                    feature_event_limits::g_max_field_value_bytes, field.value_) ||
                !vqec_vision_ai_core_evtrn_read_u32(reader, confidence) ||
                !vqec_vision_ai_core_evtrn_read_u32(reader, quality) ||
                quality > static_cast<std::uint32_t>(observation_quality::high)) {
                return {status_code::protocol_error,
                    "malformed evidence command field"};
            }
            std::memcpy(&field.confidence_, &confidence, sizeof(confidence));
            field.quality_ = static_cast<observation_quality>(quality);
            candidate.fields_.push_back(std::move(field));
        }
        if (reader.offset_ != reader.size_) {
            return {status_code::protocol_error,
                "evidence command has trailing bytes"};
        }
        const auto valid = vqec_vision_ai_core_evtrn_validate_command(candidate);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        _command = std::move(candidate);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "evidence command decode allocation failed"};
    }
    return {};
}

status vqec_vision_ai_core_evtrn_encode_receipt(
    const evidence_receipt& _receipt, std::vector<std::uint8_t>& _wire) {
    const auto valid = vqec_vision_ai_core_evtrn_validate_receipt_envelope(
        _receipt, status_code::invalid_argument);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    try {
        std::vector<std::uint8_t> wire;
        wire.reserve(256U);
        vqec_vision_ai_core_evtrn_append_header(wire, g_evidence_receipt_kind);
        vqec_vision_ai_core_evtrn_append_string(wire, _receipt.request_id_);
        vqec_vision_ai_core_evtrn_append_u64(wire, _receipt.event_revision_);
        vqec_vision_ai_core_evtrn_append_u32(
            wire, static_cast<std::uint32_t>(_receipt.state_));
        vqec_vision_ai_core_evtrn_append_string(wire, _receipt.media_id_);
        vqec_vision_ai_core_evtrn_append_u64(wire, _receipt.actual_begin_ns_);
        vqec_vision_ai_core_evtrn_append_u64(wire, _receipt.actual_end_ns_);
        vqec_vision_ai_core_evtrn_append_string(wire, _receipt.reason_);
        if (wire.size() > evidence_transport_limits::g_max_message_bytes) {
            return {status_code::resource_exhausted,
                "evidence receipt exceeds wire bound"};
        }
        _wire = std::move(wire);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "evidence receipt serialization failed"};
    }
    return {};
}

status vqec_vision_ai_core_evtrn_decode_receipt(
    const std::uint8_t* _wire, std::size_t _wire_bytes,
    evidence_receipt& _receipt) {
    if (_wire == nullptr || _wire_bytes == 0U ||
        _wire_bytes > evidence_transport_limits::g_max_message_bytes) {
        return {status_code::invalid_argument, "invalid evidence receipt wire size"};
    }
    try {
        evidence_wire_reader reader{_wire, _wire_bytes, 0U};
        evidence_receipt candidate;
        std::uint32_t state = 0U;
        if (!vqec_vision_ai_core_evtrn_read_header(reader, g_evidence_receipt_kind) ||
            !vqec_vision_ai_core_evtrn_read_string(reader,
                feature_event_limits::g_max_identifier_bytes, candidate.request_id_) ||
            !vqec_vision_ai_core_evtrn_read_u64(reader, candidate.event_revision_) ||
            !vqec_vision_ai_core_evtrn_read_u32(reader, state) ||
            state == 0U ||
            state > static_cast<std::uint32_t>(evidence_receipt_state::expired) ||
            !vqec_vision_ai_core_evtrn_read_string(reader,
                evidence_transport_limits::g_max_media_id_bytes,
                candidate.media_id_) ||
            !vqec_vision_ai_core_evtrn_read_u64(reader, candidate.actual_begin_ns_) ||
            !vqec_vision_ai_core_evtrn_read_u64(reader, candidate.actual_end_ns_) ||
            !vqec_vision_ai_core_evtrn_read_string(reader,
                evidence_transport_limits::g_max_reason_bytes, candidate.reason_) ||
            reader.offset_ != reader.size_) {
            return {status_code::protocol_error, "malformed evidence receipt wire"};
        }
        candidate.state_ = static_cast<evidence_receipt_state>(state);
        const auto valid = vqec_vision_ai_core_evtrn_validate_receipt_envelope(
            candidate, status_code::protocol_error);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        _receipt = std::move(candidate);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "evidence receipt decode allocation failed"};
    }
    return {};
}

}  // namespace vqec::vision::ai
