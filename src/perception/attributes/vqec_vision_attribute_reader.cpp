#include "vqec_vision_attribute_reader.hpp"

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_attr_atrdr_is_identifier(const std::string& _value) noexcept {
    return vqec_vision_ai_cntr_ident_is_valid(
        _value, observation_limits::g_max_identifier_bytes);
}

}  // namespace

status vqec_vision_ai_attr_atrdr_find_current_attribute(
    const observation_batch& _batch, std::uint64_t _track_id,
    const std::string& _schema_id, const std::string& _schema_version,
    std::uint64_t _now_source_ns, const observation_attribute*& _attribute) {
    const auto valid = vqec_vision_ai_core_obval_validate_batch(
        _batch, _batch.frame_, _batch.geometry_);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (_track_id == 0 || !vqec_vision_ai_attr_atrdr_is_identifier(_schema_id) ||
        !vqec_vision_ai_attr_atrdr_is_identifier(_schema_version) ||
        _now_source_ns == UINT64_MAX) {
        return {status_code::invalid_argument, "invalid attribute lookup request"};
    }
    for (const auto& observation : _batch.observations_) {
        if (observation.track_id_ != _track_id) {
            continue;
        }
        for (const auto& candidate : observation.attributes_) {
            if (candidate.schema_id_ != _schema_id ||
                candidate.schema_version_ != _schema_version) {
                continue;
            }
            if (_now_source_ns < candidate.observed_at_ns_) {
                return {status_code::invalid_argument, "attribute clock precedes observation"};
            }
            if (_now_source_ns >= candidate.expires_at_ns_) {
                return {status_code::pending, "attribute is expired"};
            }
            _attribute = &candidate;
            return {};
        }
        return {status_code::pending, "attribute is unavailable on the requested track"};
    }
    return {status_code::pending, "requested track is unavailable"};
}

}  // namespace vqec::vision::ai
