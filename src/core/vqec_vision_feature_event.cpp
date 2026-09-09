#include "vqec/vision/ai/contracts/vqec_vision_feature_event.hpp"

#include <cmath>

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_ftevt_is_identifier(const std::string& _value) noexcept {
    if (_value.empty() || _value.size() > feature_event_limits::g_max_identifier_bytes) {
        return false;
    }
    for (const unsigned char character : _value) {
        if (!((character >= 'a' && character <= 'z') ||
              (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == '_' ||
              character == '-' || character == '.' || character == ':')) {
            return false;
        }
    }
    return true;
}

bool vqec_vision_ai_core_ftevt_is_kind_valid(feature_event_kind _kind) noexcept {
    switch (_kind) {
        case feature_event_kind::episode_opened:
        case feature_event_kind::episode_updated:
        case feature_event_kind::episode_closed:
        case feature_event_kind::snapshot:
            return true;
    }
    return false;
}

bool vqec_vision_ai_core_ftevt_is_quality_valid(observation_quality _quality) noexcept {
    switch (_quality) {
        case observation_quality::unknown:
        case observation_quality::low:
        case observation_quality::medium:
        case observation_quality::high:
            return true;
    }
    return false;
}

status vqec_vision_ai_core_ftevt_validate_event(
    const feature_event& _event, const feature_event_batch& _batch,
    const feature_processor_config& _config) {
    const auto identity = vqec_vision_ai_core_pvctr_validate_identity(
        _event.frame_, _batch.frame_, _batch.geometry_, _batch.geometry_);
    if (identity.code_ != status_code::ok) {
        return identity;
    }
    if (_event.source_id_ != _config.source_id_ ||
        _event.feature_id_ != _config.feature_id_ ||
        _event.config_revision_ != _config.config_revision_ ||
        !vqec_vision_ai_core_ftevt_is_identifier(_event.event_id_) ||
        !vqec_vision_ai_core_ftevt_is_identifier(_event.event_schema_id_) ||
        !vqec_vision_ai_core_ftevt_is_identifier(_event.event_schema_version_) ||
        !vqec_vision_ai_core_ftevt_is_kind_valid(_event.kind_) ||
        _event.occurred_at_ns_ == UINT64_MAX ||
        _event.occurred_at_ns_ > _event.frame_.source_pts_ns_ ||
        _event.model_version_ids_.size() >
            feature_event_limits::g_max_model_versions_per_event ||
        _event.track_ids_.size() > _config.max_track_references_per_event_ ||
        _event.fields_.size() > _config.max_fields_per_event_ ||
        (!_event.evidence_request_id_.empty() &&
         !vqec_vision_ai_core_ftevt_is_identifier(_event.evidence_request_id_))) {
        return {status_code::invalid_argument, "invalid feature event identity or limits"};
    }
    for (std::size_t index = 0; index < _event.model_version_ids_.size(); ++index) {
        if (!vqec_vision_ai_core_ftevt_is_identifier(_event.model_version_ids_[index])) {
            return {status_code::invalid_argument, "invalid model-version provenance"};
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (_event.model_version_ids_[previous] == _event.model_version_ids_[index]) {
                return {status_code::invalid_argument, "duplicate model-version provenance"};
            }
        }
    }
    for (std::size_t index = 0; index < _event.track_ids_.size(); ++index) {
        if (_event.track_ids_[index] == 0) {
            return {status_code::invalid_argument, "feature event has a zero track reference"};
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (_event.track_ids_[previous] == _event.track_ids_[index]) {
                return {status_code::invalid_argument, "duplicate feature-event track reference"};
            }
        }
    }
    for (std::size_t index = 0; index < _event.fields_.size(); ++index) {
        const auto& field = _event.fields_[index];
        if (!vqec_vision_ai_core_ftevt_is_identifier(field.schema_id_) ||
            !vqec_vision_ai_core_ftevt_is_identifier(field.schema_version_) ||
            field.value_.size() > feature_event_limits::g_max_field_value_bytes ||
            !std::isfinite(field.confidence_) ||
            field.confidence_ < observation_limits::g_min_confidence ||
            field.confidence_ > observation_limits::g_max_confidence ||
            !vqec_vision_ai_core_ftevt_is_quality_valid(field.quality_)) {
            return {status_code::invalid_argument, "invalid feature event field"};
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            const auto& other = _event.fields_[previous];
            if (other.schema_id_ == field.schema_id_) {
                return {status_code::invalid_argument, "duplicate feature event field"};
            }
        }
    }
    return {};
}

}  // namespace

status vqec_vision_ai_core_ftevt_validate_processor_config(
    const feature_processor_config& _config) {
    if (!vqec_vision_ai_core_ftevt_is_identifier(_config.source_id_) ||
        !vqec_vision_ai_core_ftevt_is_identifier(_config.feature_id_) ||
        _config.config_revision_ == 0 || _config.config_revision_ == UINT64_MAX ||
        _config.max_events_per_update_ == 0 ||
        _config.max_events_per_update_ > feature_event_limits::g_max_events_per_batch ||
        _config.max_track_references_per_event_ == 0 ||
        _config.max_track_references_per_event_ >
            feature_event_limits::g_max_track_references_per_event ||
        _config.max_fields_per_event_ == 0 ||
        _config.max_fields_per_event_ > feature_event_limits::g_max_fields_per_event) {
        return {status_code::invalid_argument, "invalid feature processor configuration"};
    }
    return {};
}

status vqec_vision_ai_core_ftevt_validate_batch(
    const feature_event_batch& _batch, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry, const feature_processor_config& _config) {
    const auto valid_config = vqec_vision_ai_core_ftevt_validate_processor_config(_config);
    if (valid_config.code_ != status_code::ok) {
        return valid_config;
    }
    const auto identity = vqec_vision_ai_core_pvctr_validate_identity(
        _batch.frame_, _expected_frame, _batch.geometry_, _expected_geometry);
    if (identity.code_ != status_code::ok) {
        return identity;
    }
    if (_batch.events_.size() > _config.max_events_per_update_) {
        return {status_code::resource_exhausted, "feature event batch exceeds activation limit"};
    }
    for (std::size_t index = 0; index < _batch.events_.size(); ++index) {
        const auto valid_event =
            vqec_vision_ai_core_ftevt_validate_event(_batch.events_[index], _batch, _config);
        if (valid_event.code_ != status_code::ok) {
            return valid_event;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (_batch.events_[previous].event_id_ == _batch.events_[index].event_id_) {
                return {status_code::invalid_argument, "duplicate event ID in feature batch"};
            }
        }
    }
    return {};
}

}  // namespace vqec::vision::ai
