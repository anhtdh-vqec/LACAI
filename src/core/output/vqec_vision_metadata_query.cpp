#include "vqec/vision/ai/contracts/output/vqec_vision_metadata_query.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <unordered_set>

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_mdqry_is_valid_identifier(const std::string& _value) {
    return !_value.empty() && _value.size() <= g_metadata_query_max_identifier_bytes;
}

bool vqec_vision_ai_core_mdqry_is_valid_optional_identifier(const std::string& _value) {
    return _value.size() <= g_metadata_query_max_identifier_bytes;
}

bool vqec_vision_ai_core_mdqry_has_unique_strings(const std::vector<std::string>& _values) {
    std::unordered_set<std::string> unique_values;
    unique_values.reserve(_values.size());
    return std::all_of(_values.begin(), _values.end(), [&unique_values](const auto& _value) {
        return vqec_vision_ai_core_mdqry_is_valid_identifier(_value) &&
               unique_values.insert(_value).second;
    });
}

bool vqec_vision_ai_core_mdqry_is_valid_projection(const std::string& _value) {
    constexpr std::array<const char*, 17> g_allowed_projection_fields{
        "record_id", "revision", "family", "source_id", "subject_ref", "object_ref",
        "scene_ref", "semantic_type", "typed_value", "value_state", "valid_interval",
        "recorded_ns", "confidence", "sensitivity_scope", "producer_revision", "payload",
        "clock_uncertainty_ns"};
    return std::any_of(g_allowed_projection_fields.begin(), g_allowed_projection_fields.end(),
        [&_value](const char* _candidate) { return _value == _candidate; });
}

bool vqec_vision_ai_core_mdqry_matches_query_id(const metadata_query_request& _request) {
    const auto query_number = static_cast<unsigned int>(_request.kind_);
    if (query_number == 0U || query_number > 30U || _request.query_id_.size() != 3U ||
        _request.query_id_[0] != 'Q') {
        return false;
    }
    const auto tens = static_cast<char>('0' + (query_number / 10U));
    const auto ones = static_cast<char>('0' + (query_number % 10U));
    return _request.query_id_[1] == tens && _request.query_id_[2] == ones;
}

}  // namespace

status vqec_vision_ai_cntr_mdqry_validate_record(
    const metadata_record& _record, std::size_t _max_payload_bytes) {
    if (_record.schema_version_ != g_metadata_query_schema_version ||
        !vqec_vision_ai_core_mdqry_is_valid_identifier(_record.record_id_) ||
        !vqec_vision_ai_core_mdqry_is_valid_identifier(_record.device_id_) ||
        !vqec_vision_ai_core_mdqry_is_valid_identifier(_record.source_id_) ||
        !vqec_vision_ai_core_mdqry_is_valid_identifier(_record.boot_id_) ||
        !vqec_vision_ai_core_mdqry_is_valid_identifier(_record.subject_ref_) ||
        !vqec_vision_ai_core_mdqry_is_valid_identifier(_record.semantic_type_) ||
        !vqec_vision_ai_core_mdqry_is_valid_identifier(_record.producer_revision_) ||
        !vqec_vision_ai_core_mdqry_is_valid_optional_identifier(_record.object_ref_) ||
        !vqec_vision_ai_core_mdqry_is_valid_optional_identifier(_record.scene_ref_)) {
        return {status_code::invalid_argument, "metadata record identity is invalid"};
    }
    if (_record.revision_ == 0U || _record.source_epoch_ == 0U ||
        (_record.revision_ == 1U && _record.supersedes_revision_ != 0U) ||
        (_record.revision_ > 1U && _record.supersedes_revision_ != _record.revision_ - 1U) ||
        _record.revision_ > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        _record.source_epoch_ > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        _record.clock_uncertainty_ns_ >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return {status_code::invalid_argument, "metadata revision chain is invalid"};
    }
    const auto family = static_cast<unsigned int>(_record.family_);
    const auto value_state = static_cast<unsigned int>(_record.value_state_);
    const auto scope = vqec_vision_ai_cntr_mdqry_get_scope_mask(_record.sensitivity_scope_);
    constexpr std::uint32_t g_all_scope_mask = (1U << 10U) - 1U;
    if (family == 0U || family > 18U || value_state == 0U || value_state > 5U ||
        scope == 0U || (scope & (scope - 1U)) != 0U || (scope & ~g_all_scope_mask) != 0U) {
        return {status_code::invalid_argument, "metadata family, state or sensitivity is invalid"};
    }
    if (_record.valid_begin_ns_ < 0 || _record.recorded_ns_ < 0 ||
        (_record.valid_end_ns_ != 0 && _record.valid_end_ns_ <= _record.valid_begin_ns_) ||
        _record.confidence_ppm_ > g_metadata_query_confidence_scale_ppm ||
        _record.payload_.size() > _max_payload_bytes ||
        _max_payload_bytes == 0U) {
        return {status_code::invalid_argument, "metadata time, confidence or payload is invalid"};
    }
    if (_record.value_state_ == metadata_value_state::known && _record.typed_value_.empty() &&
        !_record.is_tombstone_) {
        return {status_code::invalid_argument, "known metadata value is empty"};
    }
    if (_record.is_tombstone_ && _record.revision_ == 1U) {
        return {status_code::invalid_argument, "initial metadata revision cannot be tombstone"};
    }
    return {};
}

status vqec_vision_ai_cntr_mdqry_validate_request(
    const metadata_query_request& _request, std::size_t _max_page_size) {
    if (_request.schema_version_ != g_metadata_query_schema_version ||
        !vqec_vision_ai_core_mdqry_is_valid_identifier(_request.query_id_) ||
        !vqec_vision_ai_core_mdqry_matches_query_id(_request) ||
        _request.authorization_revision_ == 0U || _request.allowed_scope_mask_ == 0U) {
        return {status_code::invalid_argument, "metadata query identity or authorization is invalid"};
    }
    if (_request.source_ids_.empty() ||
        _request.source_ids_.size() > g_metadata_query_max_sources ||
        !vqec_vision_ai_core_mdqry_has_unique_strings(_request.source_ids_) ||
        _request.projection_.empty() ||
        _request.projection_.size() > g_metadata_query_max_projection_fields ||
        !vqec_vision_ai_core_mdqry_has_unique_strings(_request.projection_) ||
        !std::all_of(_request.projection_.begin(), _request.projection_.end(),
            vqec_vision_ai_core_mdqry_is_valid_projection)) {
        return {status_code::invalid_argument, "metadata query source or projection is invalid"};
    }
    if (_request.begin_ns_ < 0 || _request.end_ns_ <= _request.begin_ns_ ||
        _request.page_size_ == 0U || _request.page_size_ > _max_page_size ||
        _max_page_size == 0U ||
        !vqec_vision_ai_core_mdqry_is_valid_optional_identifier(_request.subject_ref_) ||
        !vqec_vision_ai_core_mdqry_is_valid_optional_identifier(_request.scene_ref_) ||
        !vqec_vision_ai_core_mdqry_is_valid_optional_identifier(_request.semantic_type_) ||
        !vqec_vision_ai_core_mdqry_is_valid_optional_identifier(_request.typed_value_)) {
        return {status_code::invalid_argument, "metadata query range, page or filter is invalid"};
    }
    if (_request.snapshot_sequence_ != 0U &&
        _request.cursor_sequence_ > _request.snapshot_sequence_) {
        return {status_code::invalid_argument, "metadata query cursor exceeds snapshot"};
    }
    return {};
}

std::uint32_t vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope _scope) noexcept {
    return static_cast<std::uint32_t>(_scope);
}

}  // namespace vqec::vision::ai
